// =============================================================================
//  FAYDAM GTW202 / ESP32-C6  —  LEAN Firmware
//  "Uyan -> Olc -> Gonder(ESP-NOW/AES-GCM) -> (olmazsa) Sakla -> Uyu"
//
//  Tam surumun (WORM/vault/audit/MKT/wizard/OTA) hafif tureviidir. Tek amac:
//   1) Uyan, guc rayini brownout-guvenli kur (buck+LDO make-before-break)
//   2) TH09C oku (+ops. DS18B20/MAX31865)
//   3) RTC/FLASH buffer'daki birikmis kayitlari + bu olcumu ESP-NOW ile gonder
//   4) Gonderilemeyen kaydi pile gore RTC RAM (normal) veya FLASH (dusuk) sakla
//   5) ACK'ten zaman + uyku suresini al; special_cmd=0xBC ise BC gonder
//   6) EPD guncelle, radyoyu kapat, deep sleep
//
//  Ayarlar: Flash 40MHz/DIO, CPU 80MHz, Erase Flash Disabled.
// =============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "esp_sleep.h"
#include "esp_mac.h"

#include "config.h"
#include "lean_types.h"
#include "lean_store.h"
#include "lean_espnow.h"
#include "lean_display.h"
#include "TH09C.h"

#if ENABLE_DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
  static OneWire s_ow(DS18_PIN);
  static DallasTemperature s_ds18(&s_ow);
#endif
#if ENABLE_MAX31865
  #include <Adafruit_MAX31865.h>
  static Adafruit_MAX31865 s_max = Adafruit_MAX31865(MAX_CS);
#endif

// ---- RTC-korumali durum (deep-sleep boyunca yasar) -------------------------
RTC_DATA_ATTR static uint32_t g_boot_count   = 0;
RTC_DATA_ATTR static uint32_t g_pkt_counter  = 0;
RTC_DATA_ATTR static uint32_t g_last_unix    = 0;   // son senkron gateway zamani
RTC_DATA_ATTR static uint32_t g_sleep_sec    = DEFAULT_SLEEP_SEC;

static TH09C s_th09c;

// -----------------------------------------------------------------------------
//  Guc rayi — brownout fix (dev firmware ile ayni mantik). DOKUNMA.
// -----------------------------------------------------------------------------
static void power_rail_up() {
  gpio_hold_dis(GPIO_NUM_0);
  gpio_hold_dis(GPIO_NUM_1);
  pinMode(REG_CTL, OUTPUT);
  pinMode(LDO_CTL, OUTPUT);
  digitalWrite(REG_CTL, HIGH);          // buck aktif
  delay(LDO_SETTLE_TIME_MS);
  digitalWrite(LDO_CTL, LOW);           // LDO'yu buck besliyor (make-before-break)
  pinMode(MAX_CS, OUTPUT); digitalWrite(MAX_CS, HIGH);
  pinMode(EPD_CS, OUTPUT); digitalWrite(EPD_CS, HIGH);
}

static void power_rail_sleep() {
  digitalWrite(LDO_CTL, HIGH);
#if BUCK_ON_IN_DEEP_SLEEP
  // buck uykuda ACIK kalir -> uyanista ray guclu, boot inrush brownout'u onlenir
#else
  digitalWrite(REG_CTL, LOW);
#endif
  gpio_hold_en(GPIO_NUM_0);
  gpio_hold_en(GPIO_NUM_1);
  gpio_sleep_sel_en(GPIO_NUM_0);
  gpio_sleep_sel_en(GPIO_NUM_1);
}

// -----------------------------------------------------------------------------
//  Batarya
// -----------------------------------------------------------------------------
static uint16_t read_batt_mv() {
  analogReadResolution(12);
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) { acc += analogReadMilliVolts(BAT_ADC_PIN); delay(2); }
  return (uint16_t)((acc / 8) * BAT_ADC_DIVIDER);
}

static uint8_t batt_percent(uint16_t mv) {
  float v = mv / 1000.0f;
  if (v >= BAT_MAX_VOLT) return 100;
  if (v <= BAT_MIN_VOLT) return 0;
  return (uint8_t)((v - BAT_MIN_VOLT) / (BAT_MAX_VOLT - BAT_MIN_VOLT) * 100.0f);
}

// -----------------------------------------------------------------------------
//  Sensor okuma -> SensorRecord
// -----------------------------------------------------------------------------
static SensorRecord measure() {
  SensorRecord r = {};
  r.status = S_STATUS_OK;

#if ENABLE_TH09C
  float t = 0, h = 0;
  s_th09c.readTempHum(t, h);
  if (t == 0 && h == 0) { r.status = S_STATUS_DISCONNECTED; }
  r.temp = t; r.hum = h;
#endif
#if ENABLE_DS18B20
  s_ds18.requestTemperatures(); delay(100);
  float d = s_ds18.getTempCByIndex(0);
  if (d > -120.0f) r.temp = d;
#endif
#if ENABLE_MAX31865
  digitalWrite(MAX_CS, LOW);
  float m = s_max.temperature(MAX31865_RNOM, MAX31865_RREF);
  digitalWrite(MAX_CS, HIGH);
  if (m > -120.0f) r.temp = m;
#endif

  uint16_t mv = read_batt_mv();
  r.batt_mv = mv;
  // Zaman: senkron varsa yaklasik simdiki epoch, yoksa 0 (gateway doldurur)
  r.timestamp = (g_last_unix > 0) ? (g_last_unix + millis() / 1000) : 0;

  DEBUG_PRINT("[MEAS] T="); DEBUG_PRINT(r.temp);
  DEBUG_PRINT(" H="); DEBUG_PRINT(r.hum);
  DEBUG_PRINT(" Vbat="); DEBUG_PRINT(mv); DEBUG_PRINTLN("mV");
  return r;
}

// -----------------------------------------------------------------------------
//  UID (BC icin) — efuse MAC'ten
// -----------------------------------------------------------------------------
static void make_uid(char* out, size_t cap) {
  uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(out, cap, "FYDM-26-%02X%02X%02X%02X", mac[2], mac[3], mac[4], mac[5]);
}

// -----------------------------------------------------------------------------
//  ACK direktiflerini uygula
// -----------------------------------------------------------------------------
static void apply_ack(const AckResult& ack) {
  if (!ack.got_ack) return;
  if (ack.unix_time > 0) { g_last_unix = ack.unix_time; DEBUG_PRINT("[ACK] time sync="); DEBUG_PRINTLN(ack.unix_time); }
  if (ack.sleep_sec  > 0) {
    g_sleep_sec = constrain(ack.sleep_sec, MIN_SLEEP_SEC, MAX_SLEEP_SEC);
    DEBUG_PRINT("[ACK] sleep_sec="); DEBUG_PRINTLN(g_sleep_sec);
  }
  if (ack.special_cmd == 0xBC) {
    char uid[24]; make_uid(uid, sizeof(uid));
    DEBUG_PRINTLN("[ACK] special_cmd=0xBC -> BC gonderiliyor");
    espnow_send_bc(uid);
  }
}

// -----------------------------------------------------------------------------
//  Uyku
// -----------------------------------------------------------------------------
static void go_to_sleep() {
  espnow_end();
  power_rail_sleep();
  esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_PIN, ESP_GPIO_WAKEUP_GPIO_HIGH);
  esp_sleep_enable_timer_wakeup((uint64_t)g_sleep_sec * 1000000ULL);
  DEBUG_PRINT("[SLEEP] "); DEBUG_PRINT(g_sleep_sec); DEBUG_PRINTLN("s...");
  Serial.flush();
  delay(50);
  esp_deep_sleep_start();
}

// =============================================================================
//  setup() — tum is burada; loop() bos.
// =============================================================================
void setup() {
  g_boot_count++;

  power_rail_up();

  Serial.begin(115200);
  delay(50);
  DEBUG_PRINT("\n[BOOT] LEAN "); DEBUG_PRINT(FW_VERSION);
  DEBUG_PRINT("  boot#="); DEBUG_PRINTLN(g_boot_count);

  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // Bus'lar
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI);
  Wire.begin(I2C_SDA, I2C_SCL);
#if ENABLE_TH09C
  s_th09c.begin();
#endif
#if ENABLE_DS18B20
  s_ds18.begin();
#endif
#if ENABLE_MAX31865
  s_max.begin(MAX31865_4WIRE);
#endif

  store_init();

  // 1) OLC
  SensorRecord rec = measure();
  uint8_t bpct = batt_percent(rec.batt_mv);

  // Pil esigin altina dustuyse RTC'deki birikmis kayitlari kalici flash'a tasi
  if (bpct <= BATT_LOW_PERSIST_PERC) store_migrate_to_flash();

  // 2) RADYO
  bool radio_ok = ENABLE_ESPNOW ? espnow_begin() : false;

  bool current_sent = false;
  if (radio_ok) {
    AckResult ack; memset(&ack, 0, sizeof(ack));

    // 3a) Once birikmis kayitlari bosalt (en eski -> yeni). Bir tanesi bile
    //     gonderilemezse dur; kalani buffer'da kalsin (baglanti yok demektir).
    SensorRecord old;
    int drained = 0;
    while (store_peek_oldest(&old)) {
      if (espnow_send_record(old, g_boot_count, ++g_pkt_counter, &ack)) {
        store_remove_oldest();
        apply_ack(ack);
        drained++;
        delay(20);
      } else {
        --g_pkt_counter;   // gonderilemedi, nonce'u geri al
        break;
      }
    }
    if (drained) { DEBUG_PRINT("[SYNC] buffer bosaltildi: "); DEBUG_PRINTLN(drained); }

    // 3b) Bu dongunun olcumunu gonder
    if (store_total() == 0) {
      current_sent = espnow_send_record(rec, g_boot_count, ++g_pkt_counter, &ack);
      if (current_sent) apply_ack(ack); else --g_pkt_counter;
    }
  }

  // 4) Gonderilemeyen guncel olcumu pile gore sakla
  if (!current_sent) {
    DEBUG_PRINTLN("[NET] gonderim yok -> buffer'a alindi");
    store_push(rec, bpct);
  }

  // 5) EPD
#if ENABLE_EPD
  bool full = (g_boot_count % EPD_FULL_REFRESH_EVERY_N_BOOTS) == 0;
  display_show(rec, store_total(), bpct, full);
#endif

  // 6) UYKU
  go_to_sleep();
}

void loop() { /* bos — tum is setup()'ta */ }
