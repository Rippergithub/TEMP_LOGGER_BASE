#ifndef LEAN_CRYPTO_H
#define LEAN_CRYPTO_H
// =============================================================================
//  AES-128-GCM sarmalayici — LEAN
//  Artik TAHMIN YOK: gercek ortak kutuphane EspNowCrypto.h/.cpp bu klasore
//  vendor'landi ve dogrudan cagriliyor -> Gateway (Faydam_GTW202_ESPGTW) ve
//  sensor firmware ile BYTE-EXACT interop garantili.
//
//  Kutuphane kontrati (EspNowCrypto.h):
//    Cerceve: [0xAE:1][nonce_ctr LE:4][boot_cnt LE:2][CT:N][TAG:16]  (overhead 23B)
//    IV:      [sender_mac:6][boot_cnt LE:2][nonce_ctr LE:4]
//    AAD:     yok (NULL)
//    PMK:     SHA-256(PROJECT_ID + "_FYDM_PMK_SALT_V1")[:16]
//
//  Bu sarmalayici yalnizca: PMK turetimi, kendi MAC'i ve kolay imzalar saglar.
// =============================================================================
#include <Arduino.h>
#include "config.h"
#include "EspNowCrypto.h"     // AES_GCM_HDR_LEN, AES_GCM_TAG_LEN, AES_GCM_OVERHEAD

// espnow tarafi tampon boyutlandirmasi icin (config.h GCM_TAG_LEN=16 ile ayni).
#define GCM_HDR_LEN  AES_GCM_HDR_LEN   // 7 (1 magic + 4 nonce + 2 boot_cnt)

void   crypto_init();               // PMK turet + kendi MAC'ini oku (idempotent)
bool   crypto_selftest();           // PMK'yi kanonik test vektoruyle dogrula
String crypto_pmk_hex();
String crypto_pmk_fpr8();           // SHA256(PMK)[0:4] 8-hex (PAIR_REQ pmk_fpr; gateway ile ayni)

// plaintext -> frame.  Donus: cerceve uzunlugu; hata halinde 0.
size_t crypto_encrypt(const uint8_t* plaintext, size_t len,
                      uint16_t boot_cnt, uint32_t nonce,
                      uint8_t* frame_out, size_t frame_out_cap);

// frame -> plaintext.  src_mac = gonderenin MAC'i (gateway ACK icin gateway MAC).
// Donus: plaintext uzunlugu; dogrulama/uzunluk hatasinda 0.
size_t crypto_decrypt(const uint8_t* frame, size_t frame_len,
                      const uint8_t src_mac[6],
                      uint8_t* plain_out, size_t plain_out_cap,
                      uint32_t* out_nonce, uint16_t* out_boot_cnt);

#endif  // LEAN_CRYPTO_H
