#ifndef LEAN_TYPES_H
#define LEAN_TYPES_H
// =============================================================================
//  Ortak veri tipleri — LEAN firmware
// =============================================================================
#include <Arduino.h>
#include "config.h"

// Tek bir olcum kaydi. Hem RTC RAM hem FLASH buffer ayni struct'i saklar.
// Kucuk ve sabit boyut (16 byte) — RTC RAM'e ve NVS blob'a birebir sigar.
typedef struct __attribute__((packed)) {
  uint32_t timestamp;   // Unix epoch UTC (0 = zaman senkron yok, gateway doldurur)
  float    temp;        // °C
  float    hum;         // %
  uint16_t batt_mv;     // Batarya mV
  uint8_t  status;      // S_STATUS_*
  uint8_t  flags;       // bit0: alarm, digerleri rezerve
} SensorRecord;          // = 16 byte

// ESP-NOW kablo formatinda gonderilen binary paket (espnow-protocol §SensorDataMessage)
// Bu struct AES-GCM ile sifrelenip [IV][TAG][CIPHERTEXT] cerceveyle yollanir.
typedef struct __attribute__((packed)) {
  uint8_t  msg_type;          // 0x01 = MSG_SENSOR_DATA
  uint8_t  protocol_version;  // 2
  uint32_t packet_counter;    // monoton artan (anti-replay nonce)
  uint32_t project_id_hash;   // 0x7C9246A1
  uint32_t timestamp;         // Unix epoch UTC
  uint8_t  count;             // parametre sayisi (<=5)
  uint8_t  param_ids[5];      // 2=Temp 3=Hum 97=Batt 98=Status
  float    values[5];
  int8_t   rssi;              // gateway doldurur (biz 0 gonderiyoruz)
  uint8_t  channel;           // ESP-NOW kanali
  char     version[12];       // FW_VERSION
} SensorDataMessage;

#define MSG_SENSOR_DATA   0x01

#endif  // LEAN_TYPES_H
