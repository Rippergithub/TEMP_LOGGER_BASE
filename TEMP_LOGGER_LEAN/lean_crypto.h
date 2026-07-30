#ifndef LEAN_CRYPTO_H
#define LEAN_CRYPTO_H
// =============================================================================
//  AES-128-GCM sifreleme — LEAN
//  Anahtar: PMK = SHA-256(PROJECT_ID + ESPNOW_PMK_SALT)[:16]  (Gateway ile AYNI)
//
//  Cerceve formati (LEAN):  [IV:12][TAG:16][CIPHERTEXT:N]
//  AAD (kimlik dogrulanan ama sifrelenmeyen) = PROJECT_ID_HASH (4 byte, LE)
//
//  ⚠️ INTEROP NOTU: Bu cerceve/AAD duzeni Gateway'in ESP-NOW cozme (decrypt)
//     tarafiyla BIREBIR ayni olmalidir. Gateway farkli bir duzen kullaniyorsa
//     (or. TAG'i sona koyuyor, IV'yi nonce'dan turetiyorsa) yalnizca BU dosyayi
//     ve ceceve sabitlerini (config.h GCM_*) degistirmek yeterlidir.
// =============================================================================
#include <Arduino.h>
#include "config.h"

// PMK'yi bir kez turetir (idempotent).
void crypto_init();

// plaintext -> frame.  frame_out en az (GCM_IV_LEN+GCM_TAG_LEN+len) byte olmali.
// IV, verilen packet_counter + boot_count'tan deterministik ama benzersiz uretilir.
// Donus: toplam cerceve uzunlugu, hata halinde 0.
size_t crypto_encrypt(const uint8_t* plaintext, size_t len,
                      uint32_t boot_count, uint32_t packet_counter,
                      uint8_t* frame_out, size_t frame_out_cap);

// frame -> plaintext.  plain_out en az (frame_len - IV - TAG) byte olmali.
// Donus: plaintext uzunlugu, dogrulama/uzunluk hatasinda 0.
size_t crypto_decrypt(const uint8_t* frame, size_t frame_len,
                      uint8_t* plain_out, size_t plain_out_cap);

#endif  // LEAN_CRYPTO_H
