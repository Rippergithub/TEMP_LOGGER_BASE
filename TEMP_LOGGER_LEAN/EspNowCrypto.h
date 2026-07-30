/**
 * @file EspNowCrypto.h
 * @brief AES-128-GCM şifreleme/çözme — ESP-NOW çerçeve katmanı (ORTAK KÜTÜPHANE)
 *
 * @details
 * Bu kütüphane, daha önce Gateway (ESPNOW_GTW_ESP) ve Sensör (TEMP_LOGGER)
 * firmware'lerinde AYRI AYRI kopyalanan `espnow_aes_gcm` modülünün tek ortak
 * kaynağıdır. İki kopyanın IV üretimi ve çerçeve formatı byte-byte özdeş
 * olduğu kanıtlandığı için tek dosyada birleştirilmiştir.
 *
 * **Çerçeve Yapısı (Frame Layout) — her iki yönde aynı:**
 * ```
 * [0xAE magic 1B][nonce_ctr LE 4B][boot_cnt LE 2B][ciphertext N B][GCM tag 16B]
 * Toplam overhead: 23 byte
 * ```
 *
 * **GCM IV (12 byte):**
 * ```
 * [sender_mac 6B][boot_cnt LE 2B][nonce_ctr LE 4B]
 * ```
 *
 * **Güvenlik özellikleri:**
 * - Her gönderimde artan nonce → Replay saldırısı engeli
 * - boot_cnt ile IV çeşitliliği → Reboot sonrası IV tekrarı engeli
 * - GCM tag → Bütünlük + kimlik doğrulama (AEAD)
 *
 * **Log davranışı:**
 * - `ESPNOW_CRYPTO_VERBOSE` tanımlıysa → ayrıntılı [AES-ENC]/[AES-DEC] Serial logu
 *   (Gateway tarafında önerilir).
 * - Tanımlı değilse → sessiz (Sensör tarafında önerilir; pil/hız).
 *
 * @version 1.0.0 (gtw 3.9.2 + sns 5.17.3 birleşimi)
 * @note Header guard bilerek ESPNOW_AES_GCM_H bırakıldı; eski
 *       `#include "espnow_aes_gcm.h"` çağrılarıyla ikili uyumluluk için.
 */

#ifndef ESPNOW_AES_GCM_H
#define ESPNOW_AES_GCM_H

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Sabitler (iki firmware'de de birebir aynıydı)
// ---------------------------------------------------------------------------

/** @brief AES-GCM çerçeve başlangıç byte'ı (magic byte) */
#define AES_GCM_MAGIC    0xAE

/** @brief GCM kimlik doğrulama etiketi uzunluğu (byte) */
#define AES_GCM_TAG_LEN  16

/** @brief Çerçeve başlığı uzunluğu: 1 magic + 4 nonce_ctr + 2 boot_cnt */
#define AES_GCM_HDR_LEN  7

/** @brief Toplam şifreleme ek yükü: HDR + TAG = 23 byte */
#define AES_GCM_OVERHEAD (AES_GCM_HDR_LEN + AES_GCM_TAG_LEN)

// ---------------------------------------------------------------------------
// API (imzalar iki firmware'de de özdeşti — değiştirilmedi)
// ---------------------------------------------------------------------------

/**
 * @brief Ham veriyi AES-128-GCM ile şifreler ve ESP-NOW çerçevesi oluşturur.
 *
 * @param[in]  key16       16 byte AES anahtarı (PMK veya per-link session key)
 * @param[in]  sender_mac  Gönderen cihazın MAC adresi (6 byte) — IV'e eklenir
 * @param[in]  nonce_ctr   Monoton artan nonce sayacı (anti-replay)
 * @param[in]  boot_cnt    Yeniden başlatma sayacı (IV çeşitliliği için)
 * @param[in]  plain       Şifrelenecek ham veri
 * @param[in]  plain_len   Ham veri uzunluğu (byte)
 * @param[out] out_buf     Çıkış tamponu (en az plain_len + AES_GCM_OVERHEAD byte)
 * @param[in]  out_buf_len Çıkış tamponu kapasitesi
 * @return     Oluşturulan çerçeve uzunluğu (byte); hata durumunda 0
 *
 * @warning plain_len == 0 ise 0 döner (boş şifreleme desteklenmez).
 */
size_t espnow_aes_gcm_encrypt(const uint8_t *key16,
                               const uint8_t *sender_mac,
                               uint32_t nonce_ctr, uint16_t boot_cnt,
                               const uint8_t *plain, size_t plain_len,
                               uint8_t *out_buf, size_t out_buf_len);

/**
 * @brief ESP-NOW çerçevesini AES-128-GCM ile çözer ve doğrular.
 *
 * @param[in]  key16          16 byte AES anahtarı
 * @param[in]  sender_mac     Paketi gönderen cihazın MAC adresi (6 byte)
 * @param[in]  frame          Şifreli çerçeve verisi
 * @param[in]  frame_len      Çerçeve uzunluğu
 * @param[out] out_plain      Çözülmüş veri tamponu
 * @param[in]  out_plain_len  Çıkış tamponu kapasitesi
 * @param[out] out_nonce_ctr  Çerçeveden çıkarılan nonce (NULL geçilebilir)
 * @param[out] out_boot_cnt   Çerçeveden çıkarılan boot sayacı (NULL geçilebilir)
 * @return     Çözülmüş veri uzunluğu (byte); doğrulama hatası/geçersiz çerçevede -1
 *
 * @see  espnow_is_aes_gcm_frame()
 */
int espnow_aes_gcm_decrypt(const uint8_t *key16, const uint8_t *sender_mac,
                            const uint8_t *frame, size_t frame_len,
                            uint8_t *out_plain, size_t out_plain_len,
                            uint32_t *out_nonce_ctr, uint16_t *out_boot_cnt);

/**
 * @brief Verinin AES-GCM çerçevesi olup olmadığını hızlıca kontrol eder.
 *
 * @param[in] data İncelenecek veri
 * @param[in] len  Veri uzunluğu
 * @return    true: geçerli AES-GCM çerçevesi; false: ham/JSON veri
 *
 * @note İlk byte 0xAE (magic) ve uzunluk AES_GCM_OVERHEAD'den büyükse true döner.
 */
static inline bool espnow_is_aes_gcm_frame(const uint8_t *data, int len) {
  return len > (int)AES_GCM_OVERHEAD && data[0] == AES_GCM_MAGIC;
}

#endif // ESPNOW_AES_GCM_H
