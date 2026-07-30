#include "lean_crypto.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include "esp_mac.h"
#include <string.h>

static uint8_t s_pmk[16];
static uint8_t s_mac[6];
static bool    s_ready = false;

void crypto_init() {
  if (s_ready) return;
  // PMK = SHA-256(PROJECT_ID + SALT)[:16]   (gateway derivePmkFromProject ile ayni)
  uint8_t digest[32];
  String material = String(PROJECT_ID) + String(ESPNOW_PMK_SALT);
  mbedtls_sha256((const unsigned char*)material.c_str(), material.length(), digest, 0);
  memcpy(s_pmk, digest, 16);

  esp_read_mac(s_mac, ESP_MAC_WIFI_STA);   // IV'nin ilk 6 byte'i
  s_ready = true;

  DEBUG_PRINT("[CRYPTO] PMK="); DEBUG_PRINTLN(crypto_pmk_hex());
}

String crypto_pmk_hex() {
  crypto_init();
  char hex[33];
  for (int i = 0; i < 16; i++) snprintf(hex + i * 2, 3, "%02x", s_pmk[i]);
  return String(hex);
}

bool crypto_selftest() {
  // FYDM-BOARD-01 icin gateway SEC_SELFTEST_PMK_HEX ile ayni olmali.
  bool ok = (crypto_pmk_hex() == "1680e9159118feb685a24c7e1e78bba3");
  DEBUG_PRINT("[CRYPTO] selftest="); DEBUG_PRINTLN(ok ? "OK" : "FAIL (PMK mismatch!)");
  return ok;
}

// IV (12B) = [MAC:6][boot_cnt:2 BE][nonce:4 BE]
static void build_iv(const uint8_t mac[6], uint16_t boot_cnt, uint32_t nonce, uint8_t iv[GCM_IV_LEN]) {
  memcpy(iv, mac, 6);
  iv[6] = (boot_cnt >> 8) & 0xFF; iv[7] = boot_cnt & 0xFF;
  iv[8] = (nonce >> 24) & 0xFF; iv[9]  = (nonce >> 16) & 0xFF;
  iv[10] = (nonce >> 8) & 0xFF; iv[11] =  nonce        & 0xFF;
}

// Cerceve header (6B, cleartext): [boot_cnt:2 BE][nonce:4 BE]
static void write_hdr(uint8_t* h, uint16_t boot_cnt, uint32_t nonce) {
  h[0] = (boot_cnt >> 8) & 0xFF; h[1] = boot_cnt & 0xFF;
  h[2] = (nonce >> 24) & 0xFF; h[3] = (nonce >> 16) & 0xFF;
  h[4] = (nonce >> 8) & 0xFF;  h[5] =  nonce        & 0xFF;
}
static void read_hdr(const uint8_t* h, uint16_t* boot_cnt, uint32_t* nonce) {
  *boot_cnt = ((uint16_t)h[0] << 8) | h[1];
  *nonce = ((uint32_t)h[2] << 24) | ((uint32_t)h[3] << 16) |
           ((uint32_t)h[4] << 8) | h[5];
}

size_t crypto_encrypt(const uint8_t* plaintext, size_t len,
                      uint16_t boot_cnt, uint32_t nonce,
                      uint8_t* frame_out, size_t cap) {
  crypto_init();
  size_t need = GCM_HDR_LEN + GCM_TAG_LEN + len;
  if (cap < need) return 0;

  uint8_t iv[GCM_IV_LEN]; build_iv(s_mac, boot_cnt, nonce, iv);   // encrypt: gonderen = biz

  uint8_t* p_hdr = frame_out;
  uint8_t* p_tag = frame_out + GCM_HDR_LEN;
  uint8_t* p_ct  = frame_out + GCM_HDR_LEN + GCM_TAG_LEN;
  write_hdr(p_hdr, boot_cnt, nonce);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_pmk, 128);
  if (rc == 0) {
    rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len,
                                   iv, GCM_IV_LEN,
                                   p_hdr, GCM_HDR_LEN,     // AAD = cleartext header
                                   plaintext, p_ct, GCM_TAG_LEN, p_tag);
  }
  mbedtls_gcm_free(&gcm);
  if (rc != 0) { DEBUG_PRINT("[AES-ENC] FAIL rc="); DEBUG_PRINTLN(rc); return 0; }

  DEBUG_PRINT("[AES-ENC] OK plain="); DEBUG_PRINT((int)len);
  DEBUG_PRINT("B frame="); DEBUG_PRINT((int)need);
  DEBUG_PRINT("B nonce="); DEBUG_PRINT(nonce);
  DEBUG_PRINT(" bc="); DEBUG_PRINTLN(boot_cnt);
  return need;
}

size_t crypto_decrypt(const uint8_t* frame, size_t frame_len,
                      const uint8_t src_mac[6],
                      uint8_t* plain_out, size_t cap,
                      uint32_t* out_nonce, uint16_t* out_boot_cnt) {
  crypto_init();
  if (frame_len < (size_t)(GCM_HDR_LEN + GCM_TAG_LEN)) return 0;
  size_t ct_len = frame_len - GCM_HDR_LEN - GCM_TAG_LEN;
  if (cap < ct_len) return 0;

  const uint8_t* p_hdr = frame;
  const uint8_t* p_tag = frame + GCM_HDR_LEN;
  const uint8_t* p_ct  = frame + GCM_HDR_LEN + GCM_TAG_LEN;

  uint16_t boot_cnt; uint32_t nonce;
  read_hdr(p_hdr, &boot_cnt, &nonce);
  if (out_nonce)    *out_nonce = nonce;
  if (out_boot_cnt) *out_boot_cnt = boot_cnt;

  uint8_t iv[GCM_IV_LEN]; build_iv(src_mac, boot_cnt, nonce, iv);  // decrypt: gonderen = gateway

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_pmk, 128);
  if (rc == 0) {
    rc = mbedtls_gcm_auth_decrypt(&gcm, ct_len, iv, GCM_IV_LEN,
                                  p_hdr, GCM_HDR_LEN,      // AAD = header
                                  p_tag, GCM_TAG_LEN, p_ct, plain_out);
  }
  mbedtls_gcm_free(&gcm);
  if (rc != 0) { DEBUG_PRINT("[AES-DEC] FAIL rc="); DEBUG_PRINTLN(rc); return 0; }

  DEBUG_PRINT("[AES-DEC] OK frame="); DEBUG_PRINT((int)frame_len);
  DEBUG_PRINT("B plain="); DEBUG_PRINT((int)ct_len);
  DEBUG_PRINT("B nonce="); DEBUG_PRINTLN(nonce);
  return ct_len;
}
