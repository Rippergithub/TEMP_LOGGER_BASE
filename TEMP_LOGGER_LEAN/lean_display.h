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
// boot_count: alt bar bilgi donusumu (HW/FW/UID) icin.
// sent: bu olcum gonderildi mi. last_payload_ts: son basariyla gonderilen olcum
// zamani (getLastPayloadUnix karsiligi). Panel: SON KAYIT / SON DATA / KAYIT (n).
void display_show(const SensorRecord& rec, uint16_t pending, uint8_t batt_perc,
                  bool full, int32_t tz_off, uint32_t boot_count, bool sent,
                  uint32_t last_payload_ts);

#endif  // LEAN_DISPLAY_H
