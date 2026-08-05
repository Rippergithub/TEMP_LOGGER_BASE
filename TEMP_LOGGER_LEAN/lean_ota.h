#ifndef LEAN_OTA_H
#define LEAN_OTA_H
// =============================================================================
//  ESP-NOW OTA alicisi — LEAN   (Gateway: Faydam_GTW202_ESPGTW ile uyumlu)
//
//  TRANSPORT (v4.4.5+ / L1.9.13+):
//    Gateway UART'dan JSON/hex alir, radyoda BINARY MSG_OTA_* (0x10/0x11/0x12)
//    forward eder — chunk basina 200B ham (~3x hizli).
//    idx sira kontrolu + ota_ack (ok/exp) → gateway ota_fwd kapisi.
//    Geriye donuk: JSON ota_begin/data/end de desteklenir.
//
//  MD5 dogrulamasi Update.setMD5; ota_end'de Update.end() OK ise ESP.restart().
//  ota_active() uykuyu bloklar; her pakette deadline yenilenir (idle timeout).
// =============================================================================
#include <Arduino.h>

void     ota_handle_json(const char* json, int len);   // JSON ota_* (eski yol)
void     ota_handle_binary(const uint8_t* data, int len); // MSG_OTA_* binary
bool     ota_active();
uint32_t ota_last_ms();
void     ota_abort();

static inline bool ota_is_json(const char* p) {
  return p && (strstr(p, "ota_begin") || strstr(p, "ota_data") || strstr(p, "ota_end"));
}

// Binary OTA: ilk bayt msg_type (espnow_protocol MSG_OTA_*)
#define LEAN_MSG_OTA_BEGIN  0x10
#define LEAN_MSG_OTA_DATA   0x11
#define LEAN_MSG_OTA_END    0x12
static inline bool ota_is_binary(const uint8_t* p, int n) {
  return p && n >= 1 && (p[0] == LEAN_MSG_OTA_BEGIN || p[0] == LEAN_MSG_OTA_DATA ||
                         p[0] == LEAN_MSG_OTA_END);
}

#endif  // LEAN_OTA_H
