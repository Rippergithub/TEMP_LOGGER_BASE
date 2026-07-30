#ifndef LEAN_OTA_H
#define LEAN_OTA_H
// =============================================================================
//  ESP-NOW OTA alicisi — LEAN   (Gateway: Faydam_GTW202_ESPGTW ile uyumlu)
//
//  Binary paketler (AES-GCM cozuldukten SONRA plaintext olarak gelir):
//    BEGIN 0x10: [type1][file_size 4 LE][md5 32]   (=38B; md5 opsiyonel)
//    DATA  0x11: [type1][index 2][total 2][data_len 1][data ...]  (data offset 6)
//    END   0x12: [type1]
//
//  MD5 dogrulamasi Update.setMD5 ile; END'de Update.end() basariliysa ESP.restart().
//  _ota_active uykuyu bloklar; her pakette deadline yenilenir (idle timeout).
// =============================================================================
#include <Arduino.h>

void     ota_handle(const uint8_t* plain, int len);  // 0x10/0x11/0x12 dispatch
bool     ota_active();                                // OTA suruyor mu (uyku bloke)
uint32_t ota_last_ms();                               // son paket zamani (idle timeout)
void     ota_abort();                                 // iptal (timeout/hata)

// Bir plaintext'in OTA binary paketi olup olmadigi (ilk byte 0x10/0x11/0x12).
static inline bool ota_is_packet(const uint8_t* d, int len) {
  return len >= 1 && (d[0] == 0x10 || d[0] == 0x11 || d[0] == 0x12);
}

#endif  // LEAN_OTA_H
