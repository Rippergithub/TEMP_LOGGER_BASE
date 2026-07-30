#ifndef LEAN_CRYPTO_H
#define LEAN_CRYPTO_H
// =============================================================================
//  AES-128-GCM sifreleme — LEAN   (Gateway: Faydam_GTW202_ESPGTW ile uyumlu)
//
//  DOGRULANMIS KONTRAT (gateway espnow_security.cpp + docs/GATEWAY_SPEC.md):
//    PMK = SHA-256(PROJECT_ID + "_FYDM_PMK_SALT_V1")[:16]
//        FYDM-BOARD-01 icin = 1680e9159118feb685a24c7e1e78bba3
//    IV (12 byte) = [Sender_MAC:6][boot_cnt:2 BE][nonce:4 BE]
//    AES-128-GCM, 16 byte TAG.
//    boot_cnt = uint16 (NVS'te kalici, her boot'ta artar)
//    nonce    = uint32 (her pakette monoton artar)
//    Gateway decrypt imzasi: (key, mac, payload, len, out, cap, &nonce, &boot_cnt)
//      -> nonce ve boot_cnt cerceve icinde CLEARTEXT header'da tasinir.
//
//  ⚠️ TEK BELIRSIZ NOKTA — cerceve BAYT SIRASI:
//     Header/TAG/CIPHERTEXT dizilisi ve AAD icerigi yalnizca paylasilan
//     `EspNowCrypto` Arduino kutuphanesinde tanimli; o kutuphane gateway
//     repo'sunda vendor'lanmamis. Asagidaki duzen DOKUMANTE kontrata gore en
//     olasi haldir ama byte-exact interop icin gercek kutuphane ile
//     dogrulanmali (veya kutuphane dogrudan bu klasore kopyalanmali).
//     Duzen degisirse SADECE bu dosya + config.h GCM_* degisir.
//
//     LEAN cerceve:  [boot_cnt:2 BE][nonce:4 BE][TAG:16][CIPHERTEXT:N]
//     AAD = header (ilk 6 byte: boot_cnt+nonce)  — kimlik-dogrulanir, sifrelenmez
// =============================================================================
#include <Arduino.h>
#include "config.h"

#define GCM_HDR_LEN 6   // [boot_cnt:2][nonce:4]

void   crypto_init();               // PMK turet + kendi MAC'ini oku (idempotent)
bool   crypto_selftest();           // PMK'yi kanonik test vektoruyle dogrula
String crypto_pmk_hex();            // debug

// plaintext -> frame.  frame_out >= (GCM_HDR_LEN+GCM_TAG_LEN+len).
// Donus: toplam cerceve uzunlugu; hata halinde 0.
size_t crypto_encrypt(const uint8_t* plaintext, size_t len,
                      uint16_t boot_cnt, uint32_t nonce,
                      uint8_t* frame_out, size_t frame_out_cap);

// frame -> plaintext.  src_mac = GONDERENIN MAC'i (IV'nin ilk 6 byte'i; gateway
// ACK'i icin gateway MAC'i). Cikan nonce/boot_cnt (opsiyonel) doldurulur.
// Donus: plaintext uzunlugu; dogrulama/uzunluk hatasinda 0.
size_t crypto_decrypt(const uint8_t* frame, size_t frame_len,
                      const uint8_t src_mac[6],
                      uint8_t* plain_out, size_t plain_out_cap,
                      uint32_t* out_nonce, uint16_t* out_boot_cnt);

#endif  // LEAN_CRYPTO_H
