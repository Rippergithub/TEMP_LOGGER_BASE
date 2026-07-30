#include "lean_store.h"
#include <Preferences.h>
#include <string.h>

// -----------------------------------------------------------------------------
//  RTC RAM ring buffer — deep-sleep boyunca korunur.
// -----------------------------------------------------------------------------
RTC_DATA_ATTR static SensorRecord rtc_buf[RTC_BUF_MAX];
RTC_DATA_ATTR static uint16_t     rtc_head;   // en eski kaydin indeksi
RTC_DATA_ATTR static uint16_t     rtc_count;
RTC_DATA_ATTR static uint32_t     rtc_magic;  // 0xLEAN ilk boot tespiti

#define RTC_MAGIC 0x1EA5F00DUL

// -----------------------------------------------------------------------------
//  FLASH (NVS) buffer — RAM aynasi + Preferences blob.
// -----------------------------------------------------------------------------
static SensorRecord flash_buf[FLASH_BUF_MAX];
static uint16_t     flash_head;
static uint16_t     flash_count;
static bool         flash_dirty;

static void flash_load() {
  Preferences p;
  flash_head = 0; flash_count = 0; flash_dirty = false;
  memset(flash_buf, 0, sizeof(flash_buf));
  if (!p.begin(BUFFER_NVS_NAMESPACE, /*readOnly=*/true)) return;
  uint16_t cnt = p.getUShort("cnt", 0);
  if (cnt > FLASH_BUF_MAX) cnt = FLASH_BUF_MAX;
  if (cnt > 0) {
    p.getBytes("recs", flash_buf, (size_t)cnt * sizeof(SensorRecord));
    flash_count = cnt;
  }
  p.end();
}

static void flash_commit() {
  if (!flash_dirty) return;
  // RAM aynasini bas-hizali (head=0) normalize ederek yaz.
  static SensorRecord tmp[FLASH_BUF_MAX];
  for (uint16_t i = 0; i < flash_count; i++)
    tmp[i] = flash_buf[(flash_head + i) % FLASH_BUF_MAX];

  Preferences p;
  if (!p.begin(BUFFER_NVS_NAMESPACE, /*readOnly=*/false)) return;
  p.putUShort("cnt", flash_count);
  if (flash_count > 0)
    p.putBytes("recs", tmp, (size_t)flash_count * sizeof(SensorRecord));
  else
    p.remove("recs");
  p.end();

  memcpy(flash_buf, tmp, (size_t)flash_count * sizeof(SensorRecord));
  flash_head = 0;
  flash_dirty = false;
}

static void flash_push(const SensorRecord& rec) {
  if (flash_count >= FLASH_BUF_MAX) {            // dolu -> en eskiyi dusur
    flash_head = (flash_head + 1) % FLASH_BUF_MAX;
    flash_count--;
  }
  uint16_t tail = (flash_head + flash_count) % FLASH_BUF_MAX;
  flash_buf[tail] = rec;
  flash_count++;
  flash_dirty = true;
}

static void rtc_push(const SensorRecord& rec) {
  if (rtc_count >= RTC_BUF_MAX) {
    rtc_head = (rtc_head + 1) % RTC_BUF_MAX;
    rtc_count--;
  }
  uint16_t tail = (rtc_head + rtc_count) % RTC_BUF_MAX;
  rtc_buf[tail] = rec;
  rtc_count++;
}

// -----------------------------------------------------------------------------
//  Public API
// -----------------------------------------------------------------------------
void store_init() {
  if (rtc_magic != RTC_MAGIC) {   // cold boot / RTC RAM temiz
    rtc_head = 0; rtc_count = 0; rtc_magic = RTC_MAGIC;
    DEBUG_PRINTLN("[STORE] RTC RAM cold-init");
  }
  flash_load();
  DEBUG_PRINT("[STORE] init rtc="); DEBUG_PRINT(rtc_count);
  DEBUG_PRINT(" flash="); DEBUG_PRINTLN(flash_count);
}

uint16_t store_rtc_count()   { return rtc_count; }
uint16_t store_flash_count() { return flash_count; }
uint16_t store_total()       { return rtc_count + flash_count; }

void store_push(const SensorRecord& rec, uint8_t batt_perc) {
  if (batt_perc <= BATT_LOW_PERSIST_PERC) {
    DEBUG_PRINTLN("[STORE] push -> FLASH (dusuk pil)");
    flash_push(rec);
    flash_commit();
  } else {
    DEBUG_PRINTLN("[STORE] push -> RTC RAM");
    rtc_push(rec);
  }
}

bool store_peek_oldest(SensorRecord* out) {
  if (flash_count > 0) { *out = flash_buf[flash_head]; return true; }   // flash once
  if (rtc_count > 0)   { *out = rtc_buf[rtc_head];     return true; }
  return false;
}

void store_remove_oldest() {
  if (flash_count > 0) {
    flash_head = (flash_head + 1) % FLASH_BUF_MAX;
    flash_count--;
    flash_dirty = true;
    flash_commit();
    return;
  }
  if (rtc_count > 0) {
    rtc_head = (rtc_head + 1) % RTC_BUF_MAX;
    rtc_count--;
  }
}

void store_migrate_to_flash() {
  if (rtc_count == 0) return;
  DEBUG_PRINT("[STORE] RTC->FLASH migrate: "); DEBUG_PRINTLN(rtc_count);
  while (rtc_count > 0) {
    flash_push(rtc_buf[rtc_head]);
    rtc_head = (rtc_head + 1) % RTC_BUF_MAX;
    rtc_count--;
  }
  flash_commit();
}
