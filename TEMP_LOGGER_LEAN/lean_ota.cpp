#include "lean_ota.h"
#include "config.h"

#if ENABLE_OTA
#include <Update.h>
#include <ArduinoJson.h>

static bool     s_active  = false;
static uint32_t s_last_ms = 0;
static uint16_t s_chunks  = 0;

bool     ota_active()  { return s_active; }
uint32_t ota_last_ms() { return s_last_ms; }

void ota_abort() {
  if (s_active) { Update.abort(); s_active = false; DEBUG_PRINTLN("[OTA] abort"); }
}

// "AABB.." hex -> byte. out en az len byte. Basarili mi.
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

void ota_handle_json(const char* json, int len) {
  s_last_ms = millis();
  StaticJsonDocument<1024> doc;   // ota_data hex ~400 char + alanlar
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return;
  const char* cmd = doc["cmd"] | "";

  if (strcmp(cmd, "ota_begin") == 0) {
    size_t size = doc["size"] | 0;
    const char* md5 = doc["md5"] | "";
    if (size == 0 || size > OTA_MAX_FW_BYTES) {
      DEBUG_PRINT("[OTA] REJECT boyut="); DEBUG_PRINTLN((uint32_t)size); return;
    }
    DEBUG_PRINT("[OTA] BEGIN size="); DEBUG_PRINTLN((uint32_t)size);
    if (Update.begin(size)) {
      if (strlen(md5) == 32) Update.setMD5(md5);
      s_active = true; s_chunks = 0;
      DEBUG_PRINTLN("[OTA] Update.begin OK");
    } else {
      DEBUG_PRINT("[OTA] begin FAIL "); DEBUG_PRINTLN(Update.getError());
    }
  }
  else if (strcmp(cmd, "ota_data") == 0) {
    if (!s_active) return;
    const char* hex = doc["hex"] | "";
    int dlen = doc["len"] | 0;
    if (dlen <= 0 || dlen > 250 || strlen(hex) == 0) { DEBUG_PRINTLN("[OTA] DATA bozuk"); return; }
    uint8_t buf[256];
    if (!hex_to_bytes(hex, buf, dlen)) { DEBUG_PRINTLN("[OTA] hex decode FAIL"); ota_abort(); return; }
    if (Update.write(buf, dlen) != (size_t)dlen) {
      DEBUG_PRINTLN("[OTA] write FAIL -> abort"); ota_abort(); return;
    }
    s_chunks++;
    if ((s_chunks % 32) == 0) { DEBUG_PRINT("[OTA] chunk #"); DEBUG_PRINTLN(s_chunks); }
  }
  else if (strcmp(cmd, "ota_end") == 0) {
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
}

#else  // ENABLE_OTA == false
void     ota_handle_json(const char*, int) {}
bool     ota_active()  { return false; }
uint32_t ota_last_ms() { return 0; }
void     ota_abort()   {}
#endif
