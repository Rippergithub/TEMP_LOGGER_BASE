#ifndef LEAN_DISPLAY_H
#define LEAN_DISPLAY_H
// =============================================================================
//  E-Paper gosterim — LEAN
//  Orijinal calisan koddaki gibi Waveshare GUI_Paint (Paint_*) API'sini kullanir.
//  ⚠️ GUI_Paint.h + fonts.h Arduino "libraries" klasorunde olmali (senin
//     mevcut projendeki gibi). Yoksa ENABLE_EPD=false yapip derlenebilir.
// =============================================================================
#include <Arduino.h>
#include "lean_types.h"

// Dashboard'u ciz. full=true tam yenileme (ghosting temizligi), false partial.
// tz_off: gateway ACK'ten gelen timezone offset (sn, TR=10800) — saat yerel gosterilir.
void display_show(const SensorRecord& rec, uint16_t pending, uint8_t batt_perc,
                  bool full, int32_t tz_off);

#endif  // LEAN_DISPLAY_H
