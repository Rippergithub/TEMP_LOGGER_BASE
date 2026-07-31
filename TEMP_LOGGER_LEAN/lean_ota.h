#ifndef LEAN_OTA_H
#define LEAN_OTA_H
// =============================================================================
//  ESP-NOW OTA alicisi — LEAN   (Gateway: Faydam_GTW202_ESPGTW ile uyumlu)
//
//  AKTIF TRANSPORT = JSON. Gateway, PC/donanim merkezinden gelen OTA komutlarini
//  sensore JSON olarak (mac cikarilip AES-GCM ile) forward eder:
//    ota_begin: {"cmd":"ota_begin","size":N,"md5":"..32..","fw_ver":"..?","sig":"..?"}
//    ota_data : {"cmd":"ota_data","idx":i,"len":L,"hex":"AABB.."}
//    ota_end  : {"cmd":"ota_end"}
//  (Binary 0x10/0x11/0x12 yolu bu sistemde kullanilmiyor -> desteklenmez.)
//
//  MD5 dogrulamasi Update.setMD5; ota_end'de Update.end() OK ise ESP.restart().
//  ota_active() uykuyu bloklar; her pakette deadline yenilenir (idle timeout).
// =============================================================================
#include <Arduino.h>

void     ota_handle_json(const char* json, int len);  // ota_begin/data/end JSON dispatch
bool     ota_active();                                 // OTA suruyor mu
uint32_t ota_last_ms();                                // son paket zamani (idle timeout)
void     ota_abort();

// Plaintext bir OTA JSON komutu mu? (cmd=ota_begin/ota_data/ota_end)
static inline bool ota_is_json(const char* p) {
  return p && (strstr(p, "ota_begin") || strstr(p, "ota_data") || strstr(p, "ota_end"));
}

#endif  // LEAN_OTA_H
