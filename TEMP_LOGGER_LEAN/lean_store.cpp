#include "lean_store.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <string.h>

// =============================================================================
//  Tiered buffer:
//    RTC RAM ring  -> hizli tampon (deep-sleep'te korunur, flash asinmasi yok)
//    LittleFS file -> kalici tampon (30+ gun; guc kesilse de kalir)
//
//  Akis: kayitlar once RTC'ye; RTC dolunca TUM RTC batch halinde LittleFS'e
//  eklenir (append). Pil dusukse kayit dogrudan LittleFS'e gider. Drain sirasi:
//  once LittleFS (eski), sonra RTC (yeni).
//
//  LittleFS dosyasi: ardisik 16B SensorRecord. Gonderilmis kayitlar bastan
//  "okuma imleci" (f_read) ile tuketilir; imlec NVS'te (dusuk asinma, wake
//  basina 1 yazim). Dosya tamamen bosalinca truncate; imlec buyuyunce compaction.
// =============================================================================

// ---- RTC RAM ring -----------------------------------------------------------
RTC_DATA_ATTR static SensorRecord rtc_buf[RTC_BUF_MAX];
RTC_DATA_ATTR static uint16_t     rtc_head;
RTC_DATA_ATTR static uint16_t     rtc_count;
RTC_DATA_ATTR static uint32_t     rtc_magic;
#define RTC_MAGIC 0x1EA5F00DUL

// ---- LittleFS durable tier --------------------------------------------------
static bool     fs_ok    = false;
static uint32_t f_count  = 0;   // dosyadaki toplam kayit
static uint32_t f_read   = 0;   // bastan tuketilmis (gonderilmis) kayit sayisi
#define REC_SZ  ((uint32_t)sizeof(SensorRecord))
#define COMPACT_THRESHOLD 512   // f_read bu kadar olunca dosyayi sikistir

static void nvs_put_read(uint32_t v) {
  Preferences p;
  if (p.begin(BUFFER_NVS_NAMESPACE, false)) { p.putULong("frd", v); p.end(); }
}
static uint32_t nvs_get_read() {
  Preferences p; uint32_t v = 0;
  if (p.begin(BUFFER_NVS_NAMESPACE, true)) { v = p.getULong("frd", 0); p.end(); }
  return v;
}

static void fs_load() {
  fs_ok = LittleFS.begin(true);   // true = gerekirse formatla
  f_count = 0; f_read = 0;
  if (!fs_ok) { DEBUG_PRINTLN("[STORE] LittleFS mount FAIL"); return; }
  File f = LittleFS.open(FLASH_BUF_PATH, "r");
  if (f) { f_count = (uint32_t)(f.size() / REC_SZ); f.close(); }
  f_read = nvs_get_read();
  if (f_read > f_count) f_read = f_count;
}

// Gonderilmis kayitlari fiziksel olarak at (dosyayi bastan yeniden yaz).
static void fs_compact() {
  if (!fs_ok || f_read == 0) return;
  File in = LittleFS.open(FLASH_BUF_PATH, "r");
  if (!in) return;
  File out = LittleFS.open("/lbuf.tmp", "w");
  if (!out) { in.close(); return; }
  in.seek(f_read * REC_SZ);
  uint8_t chunk[REC_SZ * 16];
  size_t n;
  while ((n = in.read(chunk, sizeof(chunk))) > 0) out.write(chunk, n);
  in.close(); out.close();
  LittleFS.remove(FLASH_BUF_PATH);
  LittleFS.rename("/lbuf.tmp", FLASH_BUF_PATH);
  f_count -= f_read;
  f_read = 0;
  nvs_put_read(0);
  DEBUG_PRINT("[STORE] compact -> count="); DEBUG_PRINTLN(f_count);
}

static void fs_append(const SensorRecord& rec) {
  if (!fs_ok) return;
  // Cap: doluysa once tuketileni sikistir, hala doluysa en eskiyi mantiksal dusur
  if ((f_count - f_read) >= FLASH_CAP_RECORDS) {
    if (f_read > 0) fs_compact();
    if ((f_count - f_read) >= FLASH_CAP_RECORDS) { f_read++; }  // drop-oldest
  }
  File f = LittleFS.open(FLASH_BUF_PATH, "a");
  if (!f) { DEBUG_PRINTLN("[STORE] append open FAIL"); return; }
  f.write((const uint8_t*)&rec, REC_SZ);
  f.close();
  f_count++;
}

static bool fs_peek(SensorRecord* out) {
  if (!fs_ok || (f_count - f_read) == 0) return false;
  File f = LittleFS.open(FLASH_BUF_PATH, "r");
  if (!f) return false;
  f.seek(f_read * REC_SZ);
  bool ok = (f.read((uint8_t*)out, REC_SZ) == (int)REC_SZ);
  f.close();
  return ok;
}

static void fs_remove_oldest() {
  if (!fs_ok || (f_count - f_read) == 0) return;
  f_read++;
  if (f_read >= f_count) {           // tamamen bosaldi -> dosyayi sifirla
    LittleFS.remove(FLASH_BUF_PATH);
    f_count = 0; f_read = 0;
    nvs_put_read(0);
    return;
  }
  nvs_put_read(f_read);
  if (f_read >= COMPACT_THRESHOLD) fs_compact();
}

// ---- RTC ring ----------------------------------------------------------------
static void rtc_push(const SensorRecord& rec) {
  if (rtc_count >= RTC_BUF_MAX) { rtc_head = (rtc_head + 1) % RTC_BUF_MAX; rtc_count--; }
  rtc_buf[(rtc_head + rtc_count) % RTC_BUF_MAX] = rec;
  rtc_count++;
}

// ---- Public API --------------------------------------------------------------
void store_init() {
  if (rtc_magic != RTC_MAGIC) { rtc_head = 0; rtc_count = 0; rtc_magic = RTC_MAGIC;
                                DEBUG_PRINTLN("[STORE] RTC RAM cold-init"); }
  fs_load();
  DEBUG_PRINT("[STORE] init rtc="); DEBUG_PRINT(rtc_count);
  DEBUG_PRINT(" flash="); DEBUG_PRINT(f_count - f_read);
  DEBUG_PRINT("/"); DEBUG_PRINTLN(FLASH_CAP_RECORDS);
}

static uint16_t clamp16(uint32_t v) { return (v > 0xFFFF) ? 0xFFFF : (uint16_t)v; }
uint16_t store_rtc_count()   { return rtc_count; }
uint16_t store_flash_count() { return clamp16(f_count - f_read); }
uint16_t store_total()       { return clamp16((uint32_t)rtc_count + (f_count - f_read)); }

void store_migrate_to_flash() {
  if (rtc_count == 0) return;
  DEBUG_PRINT("[STORE] RTC->FLASH batch: "); DEBUG_PRINTLN(rtc_count);
  while (rtc_count > 0) {
    fs_append(rtc_buf[rtc_head]);
    rtc_head = (rtc_head + 1) % RTC_BUF_MAX;
    rtc_count--;
  }
}

void store_push(const SensorRecord& rec, uint8_t batt_perc) {
  (void)batt_perc;
  // GUC-KESINTISINE DAYANIKLILIK: buffered kayit HER ZAMAN LittleFS'e yazilir
  // (RTC RAM guc tam kesilince silinir). Online calisirken kayit gonderildigi
  // icin buffer'a dusmez -> flash yazimi yalnizca OFFLINE'da olur (asinma minimal).
  DEBUG_PRINTLN("[STORE] push -> LittleFS (kalici)");
  fs_append(rec);
}

bool store_peek_oldest(SensorRecord* out) {
  if (fs_peek(out)) return true;                 // once LittleFS (eski)
  if (rtc_count > 0) { *out = rtc_buf[rtc_head]; return true; }
  return false;
}

void store_remove_oldest() {
  if ((f_count - f_read) > 0) { fs_remove_oldest(); return; }
  if (rtc_count > 0) { rtc_head = (rtc_head + 1) % RTC_BUF_MAX; rtc_count--; }
}
