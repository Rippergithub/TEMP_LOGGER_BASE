#include "lean_ota.h"
#include "lean_espnow.h"
#include "config.h"

#if ENABLE_OTA
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

static bool     s_active  = false;
static uint32_t s_last_ms = 0;
static uint16_t s_chunks  = 0;
static uint16_t s_expect_idx = 0;

// ota_handle_* onRecv callback yolundan da cagrilir. O task TWDT'ye kayitli
// degilse esp_task_wdt_reset() "task not found" spam'i uretir.
static inline void feed_wdt_if_registered() {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  if (esp_task_wdt_status(me) == ESP_OK) {
    (void)esp_task_wdt_reset();
  }
}

bool     ota_active()  { return s_active; }
uint32_t ota_last_ms() { return s_last_ms; }

void ota_abort() {
  if (s_active) { Update.abort(); s_active = false; DEBUG_PRINTLN("[OTA] abort"); }
}

static bool hex_to_bytes(const char* hex, uint8_t* out, int len) {
  if ((int)strlen(hex) < len * 2) return false;
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };
  for (int i = 0; i < len; i++) {
    int hi = nib(hex[i * 2]), lo = nib(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

static void ota_begin_common(size_t size, const char* md5) {
  if (size == 0 || size > OTA_MAX_FW_BYTES) {
    DEBUG_PRINT("[OTA] REJECT boyut="); DEBUG_PRINTLN((uint32_t)size);
    espnow_send_ota_ack(1, 0, false, 0);
    return;
  }
  DEBUG_PRINT("[OTA] BEGIN size="); DEBUG_PRINTLN((uint32_t)size);
  if (Update.begin(size)) {
    if (md5 && strlen(md5) == 32) Update.setMD5(md5);
    s_active = true;
    s_chunks = 0;
    s_expect_idx = 0;
    DEBUG_PRINTLN("[OTA] Update.begin OK");
    espnow_send_ota_ack(1, 0, true, 0);
  } else {
    DEBUG_PRINT("[OTA] begin FAIL "); DEBUG_PRINTLN(Update.getError());
    espnow_send_ota_ack(1, 0, false, 0);
  }
}

// idx kontrolu + yazim. Donus: ACK gonderildi mi (ok bayragi).
static void ota_data_common(uint16_t idx, const uint8_t* buf, int dlen) {
  if (!s_active) {
    espnow_send_ota_ack(0, idx, false, s_expect_idx);
    return;
  }
  if (dlen <= 0 || dlen > 200 || !buf) {
    DEBUG_PRINTLN("[OTA] DATA bozuk");
    espnow_send_ota_ack(0, idx, false, s_expect_idx);
    return;
  }

  if (idx < s_expect_idx) {
    // Duplicate — yazma, idempotent ACK
    DEBUG_PRINT("[OTA] dup idx="); DEBUG_PRINT(idx);
    DEBUG_PRINT(" exp="); DEBUG_PRINTLN(s_expect_idx);
    espnow_send_ota_ack(0, idx, true, s_expect_idx);
    return;
  }
  if (idx > s_expect_idx) {
    // Gap — yazma, NACK + beklenen idx
    DEBUG_PRINT("[OTA] gap idx="); DEBUG_PRINT(idx);
    DEBUG_PRINT(" exp="); DEBUG_PRINTLN(s_expect_idx);
    espnow_send_ota_ack(0, idx, false, s_expect_idx);
    return;
  }

  // idx == s_expect_idx
  if (Update.write(const_cast<uint8_t*>(buf), (size_t)dlen) != (size_t)dlen) {
    DEBUG_PRINTLN("[OTA] write FAIL -> abort");
    ota_abort();
    espnow_send_ota_ack(0, idx, false, s_expect_idx);
    return;
  }
  s_expect_idx++;
  s_chunks++;
  feed_wdt_if_registered();
  if ((s_chunks % 32) == 0) { DEBUG_PRINT("[OTA] chunk #"); DEBUG_PRINTLN(s_chunks); }
  espnow_send_ota_ack(0, idx, true, s_expect_idx);
}

static void ota_end_common() {
  if (!s_active) return;
  DEBUG_PRINTLN("[OTA] END -> finalize");
  if (Update.end(true)) {
    DEBUG_PRINTLN("[OTA] OK, reboot");
    s_active = false;
    delay(100);
    ESP.restart();
  } else {
    DEBUG_PRINT("[OTA] END FAIL (MD5?) "); DEBUG_PRINTLN(Update.getError());
    s_active = false;
  }
}

void ota_handle_json(const char* json, int len) {
  s_last_ms = millis();
  feed_wdt_if_registered();
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return;
  const char* cmd = doc["cmd"] | "";

  if (strcmp(cmd, "ota_begin") == 0) {
    ota_begin_common(doc["size"] | 0, doc["md5"] | "");
  } else if (strcmp(cmd, "ota_data") == 0) {
    uint16_t idx = (uint16_t)(doc["idx"] | 0);
    const char* hex = doc["hex"] | "";
    int dlen = doc["len"] | 0;
    if (dlen <= 0 || dlen > 200 || strlen(hex) == 0) {
      DEBUG_PRINTLN("[OTA] DATA bozuk");
      espnow_send_ota_ack(0, idx, false, s_expect_idx);
      return;
    }
    uint8_t buf[200];
    if (!hex_to_bytes(hex, buf, dlen)) {
      DEBUG_PRINT("[OTA] hex decode FAIL len="); DEBUG_PRINT(dlen);
      DEBUG_PRINT(" hex_chars="); DEBUG_PRINTLN((int)strlen(hex));
      ota_abort();
      espnow_send_ota_ack(0, idx, false, s_expect_idx);
      return;
    }
    ota_data_common(idx, buf, dlen);
  } else if (strcmp(cmd, "ota_end") == 0) {
    ota_end_common();
  }
}

void ota_handle_binary(const uint8_t* data, int len) {
  if (!data || len < 1) return;
  s_last_ms = millis();
  feed_wdt_if_registered();

  uint8_t typ = data[0];
  if (typ == LEAN_MSG_OTA_BEGIN) {
    if (len < 1 + 4 + 33) { DEBUG_PRINTLN("[OTA] BEGIN bin kisa"); return; }
    uint32_t size;
    memcpy(&size, data + 1, 4);
    char md5[33];
    memcpy(md5, data + 5, 32);
    md5[32] = '\0';
    ota_begin_common(size, md5);
  } else if (typ == LEAN_MSG_OTA_DATA) {
    if (len < 6) { DEBUG_PRINTLN("[OTA] DATA bin kisa"); return; }
    uint16_t idx;
    memcpy(&idx, data + 1, 2);
    uint8_t dlen = data[5];
    if (dlen == 0 || dlen > 200 || len < (int)(6 + dlen)) {
      DEBUG_PRINTLN("[OTA] DATA bin len uyusmaz");
      espnow_send_ota_ack(0, idx, false, s_expect_idx);
      return;
    }
    ota_data_common(idx, data + 6, dlen);
  } else if (typ == LEAN_MSG_OTA_END) {
    ota_end_common();
  }
}

#else  // ENABLE_OTA == false
void     ota_handle_json(const char*, int) {}
void     ota_handle_binary(const uint8_t*, int) {}
bool     ota_active()  { return false; }
uint32_t ota_last_ms() { return 0; }
void     ota_abort()   {}
#endif
