#ifndef LEAN_STORE_H
#define LEAN_STORE_H
// =============================================================================
//  Tiered Offline Buffer — LEAN
//
//  RTC RAM ring   -> hizli tampon (deep-sleep'te korunur, asinma yok)
//  LittleFS file  -> kalici tampon (30+ gun; guc kesilse de kalir)
//
//  Kayitlar once RTC'ye; RTC dolunca TUM RTC batch halinde LittleFS'e tasinir.
//  Pil <= BATT_LOW_PERSIST_PERC ise kayit dogrudan LittleFS'e gider (guc-kesilme
//  guvenligi). Drain sirasi: once LittleFS (eski), sonra RTC (yeni).
//
//  Kapasite: 10 dk periyotta 30 gun = 4320 kayit; CAP=FLASH_CAP_RECORDS (5000).
// =============================================================================
#include <Arduino.h>
#include "lean_types.h"

void     store_init();                              // RTC + flash mirror'i hazirla
uint16_t store_total();                             // bekleyen toplam kayit (rtc+flash)
uint16_t store_rtc_count();
uint16_t store_flash_count();

// Yeni kaydi pile gore dogru katmana yaz. Katman doluysa en eski dusurulur (drop-oldest).
void     store_push(const SensorRecord& rec, uint8_t batt_perc);

// En eski kaydi kopyala (silmeden). false = buffer bos.
bool     store_peek_oldest(SensorRecord* out);
// En eski kaydi kaldir (basarili gonderimden SONRA cagir).
void     store_remove_oldest();

// RTC'deki tum kayitlari flash'a tasi (pil esigin altina dustugunde).
void     store_migrate_to_flash();

#endif  // LEAN_STORE_H
