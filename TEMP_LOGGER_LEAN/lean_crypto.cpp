#include "lean_crypto.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include <string.h>

static uint8_t s_pmk[16];
static bool    s_ready = false;

void crypto_init() {
  if (s_ready) return;
  // PMK = SHA-256(PROJECT_ID + SALT)[:16]
  uint8_t digest[32];
  String material = String(PROJECT_ID) + String(ESPNOW_PMK_SALT);
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 (not 224)
  mbedtls_sha256_update(&ctx, (const unsigned char*)material.c_str(), material.length());
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  memcpy(s_pmk, digest, 16);
  s_ready = true;

  DEBUG_PRINT("[CRYPTO] PMK: ");
  for (int i = 0; i < 16; i++) { DEBUG_PRINT(s_pmk[i] < 16 ? "0" : ""); DEBUG_PRINT(String(s_pmk[i], HEX)); }
  DEBUG_PRINTLN("");
}

// 12-byte IV = [boot_count BE 4][packet_counter BE 4][0x00 x4]
// (key, IV) cifti benzersiz kalir: her boot'ta boot_count artar, boot icinde
// packet_counter artar. GCM guvenligi icin IV tekrarmamasi sarttir.
static void build_iv(uint32_t boot_count, uint32_t packet_counter, uint8_t iv[GCM_IV_LEN]) {
  memset(iv, 0, GCM_IV_LEN);
  iv[0] = (boot_count >> 24) & 0xFF; iv[1] = (boot_count >> 16) & 0xFF;
  iv[2] = (boot_count >>  8) & 0xFF; iv[3] =  boot_count        & 0xFF;
  iv[4] = (packet_counter >> 24) & 0xFF; iv[5] = (packet_counter >> 16) & 0xFF;
  iv[6] = (packet_counter >>  8) & 0xFF; iv[7] =  packet_counter        & 0xFF;
}

static void aad_bytes(uint8_t aad[4]) {
  uint32_t h = PROJECT_ID_HASH;
  aad[0] = h & 0xFF; aad[1] = (h >> 8) & 0xFF; aad[2] = (h >> 16) & 0xFF; aad[3] = (h >> 24) & 0xFF;
}

size_t crypto_encrypt(const uint8_t* plaintext, size_t len,
                      uint32_t boot_count, uint32_t packet_counter,
                      uint8_t* frame_out, size_t frame_out_cap) {
  crypto_init();
  size_t need = GCM_IV_LEN + GCM_TAG_LEN + len;
  if (frame_out_cap < need) return 0;

  uint8_t iv[GCM_IV_LEN]; build_iv(boot_count, packet_counter, iv);
  uint8_t aad[4]; aad_bytes(aad);

  uint8_t* p_iv  = frame_out;
  uint8_t* p_tag = frame_out + GCM_IV_LEN;
  uint8_t* p_ct  = frame_out + GCM_IV_LEN + GCM_TAG_LEN;

  memcpy(p_iv, iv, GCM_IV_LEN);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_pmk, 128);
  if (rc == 0) {
    rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len,
                                   iv, GCM_IV_LEN, aad, sizeof(aad),
                                   plaintext, p_ct, GCM_TAG_LEN, p_tag);
  }
  mbedtls_gcm_free(&gcm);
  if (rc != 0) { DEBUG_PRINT("[AES-ENC] FAIL rc="); DEBUG_PRINTLN(rc); return 0; }

  DEBUG_PRINT("[AES-ENC] OK plain="); DEBUG_PRINT((int)len);
  DEBUG_PRINT("B -> frame="); DEBUG_PRINT((int)need);
  DEBUG_PRINT("B nonce="); DEBUG_PRINTLN(packet_counter);
  return need;
}

size_t crypto_decrypt(const uint8_t* frame, size_t frame_len,
                      uint8_t* plain_out, size_t plain_out_cap) {
  crypto_init();
  if (frame_len < (size_t)(GCM_IV_LEN + GCM_TAG_LEN)) return 0;
  size_t ct_len = frame_len - GCM_IV_LEN - GCM_TAG_LEN;
  if (plain_out_cap < ct_len) return 0;

  const uint8_t* p_iv  = frame;
  const uint8_t* p_tag = frame + GCM_IV_LEN;
  const uint8_t* p_ct  = frame + GCM_IV_LEN + GCM_TAG_LEN;

  uint8_t aad[4]; aad_bytes(aad);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, s_pmk, 128);
  if (rc == 0) {
    rc = mbedtls_gcm_auth_decrypt(&gcm, ct_len, p_iv, GCM_IV_LEN,
                                  aad, sizeof(aad), p_tag, GCM_TAG_LEN,
                                  p_ct, plain_out);
  }
  mbedtls_gcm_free(&gcm);
  if (rc != 0) { DEBUG_PRINT("[AES-DEC] FAIL rc="); DEBUG_PRINTLN(rc); return 0; }

  DEBUG_PRINT("[AES-DEC] OK frame="); DEBUG_PRINT((int)frame_len);
  DEBUG_PRINT("B -> plain="); DEBUG_PRINT((int)ct_len); DEBUG_PRINTLN("B");
  return ct_len;
}
