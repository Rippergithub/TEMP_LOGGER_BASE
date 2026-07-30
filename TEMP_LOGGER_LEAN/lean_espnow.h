#ifndef LEAN_ESPNOW_H
#define LEAN_ESPNOW_H
// =============================================================================
//  ESP-NOW gonderim katmani — LEAN
//  Akis: init -> send(record) -> L2 ACK + uygulama ACK bekle -> ACK parse
//  BC (Birth Certificate) yalnizca special_cmd=0xBC gelince gonderilir (minimum).
// =============================================================================
#include <Arduino.h>
#include "lean_types.h"

// ACK'ten alinan gateway direktifleri.
typedef struct {
  bool     got_ack;         // uygulama ACK alindi mi
  uint32_t sleep_sec;       // sleep_time_sec (0 = yok)
  uint32_t unix_time;       // gateway zamani (0 = yok)
  int32_t  tz_off;          // timezone offset saniye
  uint8_t  special_cmd;     // 0x00 NOP / 0xBC BC-iste / ...
  uint8_t  op_mode;
} AckResult;

// WiFi STA + ESP-NOW baslat (CH6, PMK, broadcast/gateway peer). false = hata.
// NOT: WiFi.mode(STA) burada cagirilir — bu, brownout tanisindaki "step1c" akim
// darbesinin oldugu yer. Guc rayi (buck+LDO) bu cagridan ONCE hazir olmali.
bool espnow_begin();

// Tek kaydi sifreleyip gonderir; L2 + uygulama ACK bekler.
// ack (opsiyonel) doldurulur. Donus: gonderim+L2 ACK basarili mi.
bool espnow_send_record(const SensorRecord& rec, uint16_t boot_cnt,
                        uint32_t nonce, AckResult* ack);

// PAIR_REQ gonderir (gateway allowlist'e eklensin). Donus: PAIR_RESP alindi mi
// (alinmasa bile PAIR_REQ ulastiysa gateway allowlist'ler -> grace).
bool espnow_pair(const char* uid, uint16_t boot_cnt, uint32_t nonce);
bool espnow_is_paired();

// Basit Birth Certificate gonderir (special_cmd=0xBC yanitinda).
// boot_cnt/nonce anti-replay icin gecerli (monoton) olmali.
bool espnow_send_bc(const char* uid, uint16_t boot_cnt, uint32_t nonce);

#if ENABLE_OTA
void espnow_ota_listen();  // send+ACK sonrasi OTA penceresi (BEGIN gelirse OTA dongusu)
#endif

void espnow_end();  // radyoyu kapat

#endif  // LEAN_ESPNOW_H
