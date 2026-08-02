#ifndef LEAN_CAL_H
#define LEAN_CAL_H
// =============================================================================
//  Kalibrasyon — LEAN   (MD/KALIBRASYON_TASARIM.md kontrati)
//
//  Modeller:  0=offset (raw+off), 1=linear (gain*raw+off), 2=piecewise (N-nokta LUT)
//  Katsayilar NVS'te (CAL_NVS_NS) kalicidir -> guc kesintisinde korunur.
//  "PC hesaplar, sensor uygular": PC 5-nokta fit'ler, {"cmd":"cal_set",...} yollar;
//  sensor katsayilari saklar ve olcumde uygular. ACK settings.cal_off ise offset gunceller.
// =============================================================================
#include <Arduino.h>
#include <ArduinoJson.h>

void     cal_init();                       // NVS'ten yukle (idempotent)
float    cal_apply(float raw);             // modele gore duzeltilmis deger
uint32_t cal_ts();                         // kalibrasyon tarihi (unix); 0=tanimsiz
uint16_t cal_valid_days();                 // gecerlilik suresi (gun)

// ACK settings offset gunceli (basit tek-nokta). ts>0 ise cal tarihini de gunceller.
void     cal_update_offset(float off, uint32_t ts);

// {"cmd":"cal_set",...} JSON komutunu isle (PC'den katsayi yukleme). true=uygulandi.
bool     cal_handle_json(const char* json, int len);

// Plaintext bir cal_set komutu mu?
static inline bool cal_is_json(const char* p) { return p && strstr(p, "cal_set"); }

#endif  // LEAN_CAL_H
