#ifndef LEAN_STORE_H
#define LEAN_STORE_H
// =============================================================================
//  Tiered Offline Buffer — LEAN
//
//  Pil NORMAL (> BATT_LOW_PERSIST_PERC)  -> RTC RAM ring buffer (hizli, asinma yok)
//  Pil BITMEYE YAKIN (<= esik)           -> FLASH (NVS), guc kesilse de kalici
//
//  RTC RAM deep-sleep boyunca korunur ama guc TAMAMEN kesilirse silinir; bu yuzden
//  pil dusukken kayitlar flash'a yazilir. Ayrica pil esigin altina dustugunde
//  RTC'deki birikmis kayitlar da flash'a tasinir (store_migrate_to_flash).
//
//  Drain sirasi: once FLASH (kalici, muhtemelen daha eski), sonra RTC.
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
