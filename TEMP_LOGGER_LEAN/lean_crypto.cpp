#include "lean_crypto.h"
#include "mbedtls/sha256.h"
#include "esp_mac.h"
#include <string.h>

static uint8_t s_pmk[16];
static uint8_t s_mac[6];
static bool    s_ready = false;

void crypto_init() {
  if (s_ready) return;
  // PMK = SHA-256(PROJECT_ID + SALT)[:16]  (gateway derivePmkFromProject ile ayni)
  uint8_t digest[32];
  String material = String(PROJECT_ID) + String(ESPNOW_PMK_SALT);
  mbedtls_sha256((const unsigned char*)material.c_str(), material.length(), digest, 0);
  memcpy(s_pmk, digest, 16);

  esp_read_mac(s_mac, ESP_MAC_WIFI_STA);   // IV'nin ilk 6 byte'i (gonderen = biz)
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
  bool ok = (crypto_pmk_hex() == "1680e9159118feb685a24c7e1e78bba3");
  DEBUG_PRINT("[CRYPTO] selftest="); DEBUG_PRINTLN(ok ? "OK" : "FAIL (PMK mismatch!)");
  return ok;
}

size_t crypto_encrypt(const uint8_t* plaintext, size_t len,
                      uint16_t boot_cnt, uint32_t nonce,
                      uint8_t* frame_out, size_t cap) {
  crypto_init();
  // Gercek kutuphane: IV+cerceve+tag hepsini uretir. Gonderen MAC = bizim MAC.
  return espnow_aes_gcm_encrypt(s_pmk, s_mac, nonce, boot_cnt,
                                plaintext, len, frame_out, cap);
}

size_t crypto_decrypt(const uint8_t* frame, size_t frame_len,
                      const uint8_t src_mac[6],
                      uint8_t* plain_out, size_t cap,
                      uint32_t* out_nonce, uint16_t* out_boot_cnt) {
  crypto_init();
  int n = espnow_aes_gcm_decrypt(s_pmk, src_mac, frame, frame_len,
                                 plain_out, cap, out_nonce, out_boot_cnt);
  return (n > 0) ? (size_t)n : 0;
}
