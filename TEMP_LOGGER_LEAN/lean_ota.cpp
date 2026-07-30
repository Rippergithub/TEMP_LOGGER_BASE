#include "lean_ota.h"
#include "config.h"

#if ENABLE_OTA
#include <Update.h>

#define OTA_BEGIN 0x10
#define OTA_DATA  0x11
#define OTA_END   0x12

static bool     s_active  = false;
static uint32_t s_last_ms = 0;
static uint16_t s_chunks  = 0;

bool     ota_active()  { return s_active; }
uint32_t ota_last_ms() { return s_last_ms; }

void ota_abort() {
  if (s_active) { Update.abort(); s_active = false; DEBUG_PRINTLN("[OTA] abort"); }
}

void ota_handle(const uint8_t* d, int len) {
  if (len < 1) return;
  s_last_ms = millis();

  switch (d[0]) {
    case OTA_BEGIN: {
      if (len < 6) { DEBUG_PRINTLN("[OTA] BEGIN kisa"); return; }
      uint32_t fsize = 0; memcpy(&fsize, d + 1, 4);
      if (fsize == 0 || fsize > OTA_MAX_FW_BYTES) {
        DEBUG_PRINT("[OTA] REJECT boyut="); DEBUG_PRINTLN(fsize); return;
      }
      char md5[33] = "";
      if (len >= 38) { memcpy(md5, d + 5, 32); md5[32] = 0; }
      DEBUG_PRINT("[OTA] BEGIN size="); DEBUG_PRINTLN(fsize);
      if (Update.begin(fsize)) {
        if (md5[0]) Update.setMD5(md5);
        s_active = true; s_chunks = 0;
        DEBUG_PRINTLN("[OTA] Update.begin OK");
      } else {
        DEBUG_PRINT("[OTA] begin FAIL "); DEBUG_PRINTLN(Update.getError());
      }
    } break;

    case OTA_DATA: {
      if (!s_active) return;
      if (len < 6) return;
      uint8_t clen = d[5];
      if (clen > 200 || len < 6 + (int)clen) { DEBUG_PRINTLN("[OTA] DATA len bozuk"); return; }
      if (Update.write((uint8_t*)(d + 6), clen) != clen) {
        DEBUG_PRINTLN("[OTA] write FAIL -> abort"); ota_abort(); return;
      }
      s_chunks++;
      uint16_t idx = 0, tot = 0; memcpy(&idx, d + 1, 2); memcpy(&tot, d + 3, 2);
      if ((s_chunks % 32) == 0) { DEBUG_PRINT("[OTA] chunk "); DEBUG_PRINT(idx + 1);
                                  DEBUG_PRINT("/"); DEBUG_PRINTLN(tot); }
    } break;

    case OTA_END: {
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
    } break;
  }
}

#else  // ENABLE_OTA == false
void     ota_handle(const uint8_t*, int) {}
bool     ota_active()  { return false; }
uint32_t ota_last_ms() { return 0; }
void     ota_abort()   {}
#endif
