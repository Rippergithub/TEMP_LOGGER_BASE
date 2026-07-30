/**
 * @file EspNowCrypto.cpp
 * @brief AES-128-GCM şifreleme/çözme implementasyonu — ORTAK KÜTÜPHANE
 *
 * @details Gateway (3.9.2, ayrıntılı loglu) ve Sensör (5.17.3, sessiz)
 * kopyalarının birleşimi. Çekirdek mantık (IV üretimi, mbedTLS çağrıları,
 * çerçeve formatı) iki tarafta byte-byte özdeşti; tek fark Serial log
 * ayrıntısıydı. Bu fark artık derleme-zamanı bayrağı ile yönetilir:
 *
 *   #define ESPNOW_CRYPTO_VERBOSE   → ayrıntılı [AES-ENC]/[AES-DEC] logu
 *   (tanımsız)                      → sessiz, Arduino.h bağımlılığı yok
 *
 * IV yapısı: [sender_mac 6B][boot_cnt LE 2B][nonce_ctr LE 4B]
 */

#include "EspNowCrypto.h"
#include "mbedtls/gcm.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Log yardımcıları — yalnızca ESPNOW_CRYPTO_VERBOSE tanımlıysa Serial'e basar.
// Tanımsızsa hiçbir koda/bağımlılığa dönüşmez (sensör tarafı için ideal).
// ---------------------------------------------------------------------------
#ifdef ESPNOW_CRYPTO_VERBOSE
  #include <Arduino.h>
  #define ENC_LOGLN(s)        Serial.println(F(s))
  #define ENC_LOGF(fmt, ...)  Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define ENC_LOGLN(s)        ((void)0)
  #define ENC_LOGF(fmt, ...)  ((void)0)
#endif

// ---------------------------------------------------------------------------
// Yardımcı: GCM IV üretimi
// ---------------------------------------------------------------------------
/**
 * @brief 12 byte GCM IV'i oluşturur.
 * @details IV yapısı: [sender_mac 6B][boot_cnt LE 2B][nonce_ctr LE 4B]
 *          Her boot + her paket için benzersiz IV garantilenir.
 *          (Önceki iki kopya bu fonksiyonu farklı parametre SIRASIYLA
 *           tanımlıyordu ama ürettikleri IV birebir aynıydı.)
 */
static void build_iv(uint8_t iv[12], const uint8_t *mac6,
                     uint32_t ctr, uint16_t boot_cnt) {
  memcpy(iv, mac6, 6);
  iv[6]  = (uint8_t)(boot_cnt);
  iv[7]  = (uint8_t)(boot_cnt >> 8);
  iv[8]  = (uint8_t)(ctr);
  iv[9]  = (uint8_t)(ctr >> 8);
  iv[10] = (uint8_t)(ctr >> 16);
  iv[11] = (uint8_t)(ctr >> 24);
}

// ---------------------------------------------------------------------------
// Şifreleme
// ---------------------------------------------------------------------------
size_t espnow_aes_gcm_encrypt(const uint8_t *key16,
                               const uint8_t *sender_mac,
                               uint32_t nonce_ctr, uint16_t boot_cnt,
                               const uint8_t *plain, size_t plain_len,
                               uint8_t *out_buf, size_t out_buf_len) {

  size_t frame_len = AES_GCM_HDR_LEN + plain_len + AES_GCM_TAG_LEN;

  // --- Ön kontroller ---
  if (plain_len == 0) {
    ENC_LOGLN("[AES-ENC] FAIL | plain_len=0, sifreleme atlandi");
    return 0;
  }
  if (out_buf_len < frame_len) {
    ENC_LOGF("[AES-ENC] FAIL | Tampon yetersiz: gerekli=%zu mevcut=%zu\n",
             frame_len, out_buf_len);
    return 0;
  }

  // --- IV ve header ---
  uint8_t iv[12];
  build_iv(iv, sender_mac, nonce_ctr, boot_cnt);

  out_buf[0] = AES_GCM_MAGIC;
  memcpy(out_buf + 1, &nonce_ctr, 4);
  memcpy(out_buf + 5, &boot_cnt,  2);

  // --- mbedTLS GCM ---
  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key16, 128) != 0) {
    ENC_LOGLN("[AES-ENC] FAIL | mbedtls_gcm_setkey hatasi (anahtar gecersiz?)");
    mbedtls_gcm_free(&ctx);
    return 0;
  }

  uint8_t *ct  = out_buf + AES_GCM_HDR_LEN;
  uint8_t *tag = ct + plain_len;

  int ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, plain_len,
                                       iv, 12, NULL, 0,
                                       plain, ct,
                                       AES_GCM_TAG_LEN, tag);
  mbedtls_gcm_free(&ctx);

  if (ret != 0) {
    ENC_LOGF("[AES-ENC] FAIL | GCM sifreleme hatasi: ret=%d\n", ret);
    return 0;
  }

  ENC_LOGF("[AES-ENC] OK  | plain=%zuB -> frame=%zuB | nonce=%lu bc=%u "
           "mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
           plain_len, frame_len, (unsigned long)nonce_ctr, boot_cnt,
           sender_mac[0], sender_mac[1], sender_mac[2],
           sender_mac[3], sender_mac[4], sender_mac[5]);

  return frame_len;
}

// ---------------------------------------------------------------------------
// Çözme
// ---------------------------------------------------------------------------
int espnow_aes_gcm_decrypt(const uint8_t *key16, const uint8_t *sender_mac,
                            const uint8_t *frame, size_t frame_len,
                            uint8_t *out_plain, size_t out_plain_len,
                            uint32_t *out_nonce_ctr, uint16_t *out_boot_cnt) {

  // --- Çerçeve geçerlilik kontrolü ---
  if (frame_len <= AES_GCM_OVERHEAD) {
    ENC_LOGF("[AES-DEC] FAIL | Cerceve cok kisa: %zuB (min %dB)\n",
             frame_len, (int)AES_GCM_OVERHEAD + 1);
    return -1;
  }
  if (frame[0] != AES_GCM_MAGIC) {
    ENC_LOGF("[AES-DEC] FAIL | Magic byte yanlis: 0x%02X (beklenen 0x%02X)\n",
             frame[0], AES_GCM_MAGIC);
    return -1;
  }

  size_t plain_len = frame_len - AES_GCM_OVERHEAD;
  if (out_plain_len < plain_len) {
    ENC_LOGF("[AES-DEC] FAIL | Cikis tamponu yetersiz: gerekli=%zu mevcut=%zu\n",
             plain_len, out_plain_len);
    return -1;
  }

  // --- Header'dan nonce ve boot_cnt ---
  uint32_t ctr;
  uint16_t bc;
  memcpy(&ctr, frame + 1, 4);
  memcpy(&bc,  frame + 5, 2);

  if (out_nonce_ctr) *out_nonce_ctr = ctr;
  if (out_boot_cnt)  *out_boot_cnt  = bc;

  // --- IV ve mbedTLS GCM ---
  uint8_t iv[12];
  build_iv(iv, sender_mac, ctr, bc);

  const uint8_t *ct  = frame + AES_GCM_HDR_LEN;
  const uint8_t *tag = ct + plain_len;

  mbedtls_gcm_context ctx;
  mbedtls_gcm_init(&ctx);

  if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key16, 128) != 0) {
    ENC_LOGLN("[AES-DEC] FAIL | mbedtls_gcm_setkey hatasi");
    mbedtls_gcm_free(&ctx);
    return -1;
  }

  int ret = mbedtls_gcm_auth_decrypt(&ctx, plain_len,
                                      iv, 12, NULL, 0,
                                      tag, AES_GCM_TAG_LEN,
                                      ct, out_plain);
  mbedtls_gcm_free(&ctx);

  if (ret != 0) {
    ENC_LOGF("[AES-DEC] FAIL | GCM tag dogrulamasi basarisiz"
             " | frame=%zuB nonce=%lu bc=%u mac=%02X:%02X:%02X:%02X:%02X:%02X"
             " (yanlis anahtar veya bozuk paket)\n",
             frame_len, (unsigned long)ctr, bc,
             sender_mac[0], sender_mac[1], sender_mac[2],
             sender_mac[3], sender_mac[4], sender_mac[5]);
    return -1;
  }

  ENC_LOGF("[AES-DEC] OK  | frame=%zuB -> plain=%zuB | nonce=%lu bc=%u "
           "mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
           frame_len, plain_len, (unsigned long)ctr, bc,
           sender_mac[0], sender_mac[1], sender_mac[2],
           sender_mac[3], sender_mac[4], sender_mac[5]);

  return (int)plain_len;
}
