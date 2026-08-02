#include "lean_cal.h"
#include "config.h"
#include <Preferences.h>

// RAM ayna (NVS'ten yuklenir)
static uint8_t  s_model = 0;                 // 0=offset,1=linear,2=piecewise
static float    s_gain  = 1.0f;
static float    s_off   = 0.0f;
static uint8_t  s_n     = 0;
static float    s_raw[CAL_MAX_POINTS];
static float    s_ref[CAL_MAX_POINTS];
static uint32_t s_ts    = 0;
static uint16_t s_valid = CAL_VALID_DAYS;
static bool     s_loaded = false;

void cal_init() {
  if (s_loaded) return;
  s_loaded = true;
  Preferences p;
  if (!p.begin(CAL_NVS_NS, true)) return;   // yoksa varsayilanlar kalir
  s_model = p.getUChar("model", 0);
  s_gain  = p.getFloat("gain", 1.0f);
  s_off   = p.getFloat("off", 0.0f);
  s_n     = p.getUChar("n", 0);
  if (s_n > CAL_MAX_POINTS) s_n = 0;
  if (s_n > 0) {
    p.getBytes("raw", s_raw, s_n * sizeof(float));
    p.getBytes("ref", s_ref, s_n * sizeof(float));
  }
  s_ts    = p.getULong("ts", 0);
  s_valid = p.getUShort("valid", CAL_VALID_DAYS);
  p.end();
  DEBUG_PRINT("[CAL] load model="); DEBUG_PRINT(s_model);
  DEBUG_PRINT(" gain="); DEBUG_PRINT(s_gain);
  DEBUG_PRINT(" off="); DEBUG_PRINT(s_off);
  DEBUG_PRINT(" n="); DEBUG_PRINT(s_n);
  DEBUG_PRINT(" ts="); DEBUG_PRINTLN(s_ts);
}

static void cal_persist() {
  Preferences p;
  if (!p.begin(CAL_NVS_NS, false)) return;
  p.putUChar("model", s_model);
  p.putFloat("gain", s_gain);
  p.putFloat("off", s_off);
  p.putUChar("n", s_n);
  if (s_n > 0) {
    p.putBytes("raw", s_raw, s_n * sizeof(float));
    p.putBytes("ref", s_ref, s_n * sizeof(float));
  }
  p.putULong("ts", s_ts);
  p.putUShort("valid", s_valid);
  p.end();
}

uint32_t cal_ts()         { cal_init(); return s_ts; }
uint16_t cal_valid_days() { cal_init(); return s_valid; }

// N-nokta piecewise dogrusal enterpolasyon (s_raw sirali varsayilir).
static float piecewise(float x) {
  if (s_n < 2) return x + s_off;
  if (x <= s_raw[0])
    return s_ref[0] + (s_ref[1] - s_ref[0]) * (x - s_raw[0]) / (s_raw[1] - s_raw[0]);
  for (uint8_t k = 0; k + 1 < s_n; k++) {
    if (x <= s_raw[k + 1]) {
      float dr = s_raw[k + 1] - s_raw[k];
      if (dr == 0) return s_ref[k];
      return s_ref[k] + (s_ref[k + 1] - s_ref[k]) * (x - s_raw[k]) / dr;
    }
  }
  uint8_t k = s_n - 2;   // son segment ekstrapolasyon
  float dr = s_raw[k + 1] - s_raw[k];
  if (dr == 0) return s_ref[k + 1];
  return s_ref[k] + (s_ref[k + 1] - s_ref[k]) * (x - s_raw[k]) / dr;
}

float cal_apply(float raw) {
  cal_init();
  switch (s_model) {
    case 1: return s_gain * raw + s_off;   // linear
    case 2: return piecewise(raw);         // piecewise LUT
    default: return raw + s_off;           // offset
  }
}

void cal_update_offset(float off, uint32_t ts) {
  cal_init();
  // LUT (model 2) tam kalibrasyon otoriterdir; ACK offset'i onu EZMEZ, sadece
  // cal tarihini gunceller. Offset yalnizca model 0/1'de uygulanir.
  bool changed = (ts > 0 && ts != s_ts);
  if (s_model <= 1) { if (s_off != off) { s_off = off; changed = true; } }
  if (ts > 0) s_ts = ts;
  if (changed) cal_persist();
}

bool cal_handle_json(const char* json, int len) {
  cal_init();
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json, len) != DeserializationError::Ok) return false;
  if (strcmp(doc["cmd"] | "", "cal_set") != 0) return false;

  uint8_t model = doc["model"] | 0;
  // Kalite kapisi (yanlis-giris korumasi): r2 verildiyse >=0.99 olmali.
  float r2 = doc["r2"] | 1.0f;
  if (r2 < 0.99f) { DEBUG_PRINT("[CAL] REJECT r2="); DEBUG_PRINTLN(r2); return false; }

  if (model == 2) {
    JsonArray raw = doc["raw"], ref = doc["ref"];
    uint8_t n = raw.size();
    if (n < 2 || n > CAL_MAX_POINTS || ref.size() != n) { DEBUG_PRINTLN("[CAL] REJECT LUT n"); return false; }
    for (uint8_t i = 0; i < n; i++) { s_raw[i] = raw[i]; s_ref[i] = ref[i]; }
    s_n = n; s_model = 2;
  } else if (model == 1) {
    s_gain = doc["gain"] | 1.0f;
    s_off  = doc["off"]  | 0.0f;
    s_n = 0; s_model = 1;
  } else {
    s_off = doc["off"] | 0.0f;
    s_n = 0; s_model = 0;
  }
  s_ts    = doc["ts"] | s_ts;
  s_valid = doc["valid_days"] | s_valid;
  cal_persist();
  DEBUG_PRINT("[CAL] cal_set uygulandi model="); DEBUG_PRINT(s_model);
  DEBUG_PRINT(" n="); DEBUG_PRINT(s_n); DEBUG_PRINT(" ts="); DEBUG_PRINTLN(s_ts);
  return true;
}
