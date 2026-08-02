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
//  Partition: "Default 4MB with spiffs" (OTA app0/app1 + LittleFS buffer).
// =============================================================================
//  SURUM: L1.7.0            (config.h FW_VERSION ile ayni tutulmali)
//  -----------------------------------------------------------------------------
//  DEGISIKLIK GUNLUGU (her degisiklikte en uste yeni satir eklenir):
//   L1.7.0  - Datalogger cekirdek guvence: (1) buffered kayit HER ZAMAN LittleFS'e
//             (guc-kesintisine dayanikli, RTC RAM tier kaldirildi), (2) donanim
//             watchdog (WDT_TIMEOUT_MS) - takilmada otomatik reset + feed noktalari
//   L1.6.0  - EPD alarm: sag panelde UNLEM ucgeni + footer'da alarm BASLANGIC zamani
//             (g_alarm_start_ts). Sag panel: buffer varsa "KAYIT (n)" + son teslim
//             zamani (SON KAYIT sayisi gorunur).
//   L1.5.4  - FIX (appACK=0 kok neden): gateway UNICAST ACK'i LINK SESSION KEY ile
//             sifreliyor; LEAN sadece PMK ile cozuyordu -> ACK/unix okunamiyordu.
//             crypto_decrypt artik once link key sonra PMK dener (deriveLinkSessionKey).
//   L1.5.3  - FIX: kesif sonrasi ogrenilen gateway MAC peer tablosuna eklenmiyordu
//             -> unicast "L2 ACK YOK". send_frame artik ensure_peer ile hedefi ekler.
//             (PAIR_RESP ile s_gw_known=true olunca veri gateway'e ulasip JSON ACK/
//             saat gelir.)
//   L1.5.2  - FIX (saat gorunmuyor kok neden): veri artik MINIFIED JSON ("typ":"D")
//             gonderiliyor. Gateway binary veriye BINARY ACK donuyordu; LEAN sadece
//             JSON ACK parse ettigi icin unix/settings/ota_pending hic okunamiyordu.
//             JSON veri -> JSON ACK -> saat + settings + ota_pending calisir.
//   L1.5.1  - Zaman damgasi: ilerleyen epoch tahmini (uyanista +sleep_sec, ACK'te
//             resync). Buffered kayit OLCUM zamaniyla saklanir; hafizadan gonderilen
//             veri orijinal zamaniyla gider (guncel zamanla YENIDEN damgalanmaz).
//   L1.5.0  - OTA transport JSON'a cevrildi (gateway forward'i JSON: cmd=ota_begin/
//             ota_data hex/ota_end). Binary yol kaldirildi. recv dispatch JSON.
//   L1.4.0  - OTA ota_pending: gateway ACK'te "OTA var" derse LEAN uyumaz, uzun
//             pencerede (OTA_PENDING_WINDOW_MS) BEGIN bekler (uyku modunda OTA)
//   L1.3.2  - FIX: broadcast kesifte gonderim ancak uygulama-ACK ile "gonderildi"
//             sayilir (aksi halde gateway duymadan veri kaybi olabiliyordu)
//   L1.3.1  - EPD saat/tarih: zaman senkronu ilk geldigi cyclede rec.timestamp
//             geriye donuk doldurulur (ilk cycle "--:--" kalmaz)
//   L1.3.0  - Dayaniklilik: broadcast'te L2-ACK zorunlulugu kaldirildi (kesif calisir);
//             ard arda ACK'siz cycle'da otomatik yeniden kesif (broadcast+PAIR,
//             REDISCOVER_AFTER_FAILS); force-broadcast RTC bayragi
//   L1.2.2  - EPD sag panel gercek temp_logger mantigi: SON KAYIT (last_payload_ts) /
//             SON DATA / KAYIT (n); g_last_payload_ts RTC'de takip edilir
//   L1.2.1  - EPD sag panel: gonderilemeyen olcum "SON KAYIT", gonderilen "SON DATA"
//   L1.2.0  - Adaptif TX power (ACK RSSI'sine gore, RTC'de kalici; pil optimizasyonu)
//   L1.1.0  - Pairing/allowlist (PAIR_REQ), ACK settings (esikler+cal_off),
//             alarm mantigi (histerezis) + EPD gosterimi
//           - OTA katmani (ESP-NOW 0x10/0x11/0x12, Update.h + MD5)
//           - Offline buffer NVS -> LittleFS (30+ gun, 10dk periyot)
//           - EPD: GUI_Paint/Fonts vendor, dashboard duzeni, derece simgesi,
//             timezone offset, alt bar bilgi donusumu (HW/FW/UID)
//           - AES-GCM byte-exact (EspNowCrypto vendor), IV/cerceve gateway ile ayni
//           - EPD pin fix (RES/DC OUTPUT), ilk boot FULL refresh
//   L1.0.0  - Ilk LEAN: uyan->olc->gonder->sakla->uyu, ESP-NOW/AES-GCM,
//             RTC/flash tiered buffer, EPD dashboard, brownout guc rayi
// =============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "esp_sleep.h"
#include "esp_mac.h"
#include "esp_task_wdt.h"

#include "config.h"
#include "lean_types.h"
#include "lean_store.h"
#include "lean_crypto.h"
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
RTC_DATA_ATTR static uint32_t g_last_unix    = 0;   // son senkron gateway zamani (UTC)
RTC_DATA_ATTR static int32_t  g_tz_off       = 10800; // timezone offset (sn), ACK 'off' (TR varsayilan +3s)
RTC_DATA_ATTR static uint32_t g_sleep_sec    = DEFAULT_SLEEP_SEC;
RTC_DATA_ATTR static bool     g_paired       = false;  // gateway allowlist eslesmesi (RTC'de kalici)
// ACK settings (gateway'den; RTC'de kalici). Varsayilanlar config alarm limitleri.
RTC_DATA_ATTR static float    g_t_low        = T_LOW_LIMIT;
RTC_DATA_ATTR static float    g_t_high       = T_HIGH_LIMIT;
RTC_DATA_ATTR static float    g_h_low        = H_LOW_LIMIT;
RTC_DATA_ATTR static float    g_h_high       = H_HIGH_LIMIT;
RTC_DATA_ATTR static float    g_cal_off      = 0.0f;
RTC_DATA_ATTR static bool     g_in_alarm     = false;  // histerezis durumu
RTC_DATA_ATTR static uint32_t g_alarm_start_ts = 0;    // alarmin ilk basladigi an (footer BASLANGIC)
RTC_DATA_ATTR static uint32_t g_last_payload_ts = 0;   // son BASARIYLA gonderilen olcum zamani (getLastPayloadUnix karsiligi)
RTC_DATA_ATTR static uint8_t  g_fail_streak     = 0;    // ard arda uygulama-ACK'siz cycle sayisi
RTC_DATA_ATTR static bool     g_force_bcast     = false;// yeniden kesif: broadcast + PAIR

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

  r.temp += g_cal_off;   // kalibrasyon offset (ACK settings)

  // --- Alarm (esik + histerezis). Prob kopuksa alarm degerlendirilmez. ---
  if (r.status == S_STATUS_OK) {
    bool over;
    if (g_in_alarm) {
      // Alarmdan cikis: histerezis kadar iceri girmeli
      over = (r.temp > g_t_high - T_HYSTERESIS) || (r.temp < g_t_low + T_HYSTERESIS) ||
             (r.hum  > g_h_high - H_HYSTERESIS) || (r.hum  < g_h_low + H_HYSTERESIS);
    } else {
      over = (r.temp > g_t_high) || (r.temp < g_t_low) ||
             (r.hum  > g_h_high) || (r.hum  < g_h_low);
    }
    bool was = g_in_alarm;
    g_in_alarm = over;
    if (over) {
      r.status = S_STATUS_OUT_OF_RANGE; r.flags |= 0x01;
      // Alarm ilk basladigi an: baslangic zamanini kaydet (footer'da gosterilir)
      if (!was) g_alarm_start_ts = (r.timestamp > MIN_VALID_UNIX) ? r.timestamp : g_last_unix;
    } else {
      g_alarm_start_ts = 0;   // alarm bitti
    }
  }

  uint16_t mv = read_batt_mv();
  r.batt_mv = mv;
  // Olcum zaman damgasi = ilerleyen epoch tahmini + bu cycle'daki uyanik sure.
  // Gecerli degilse 0 (gateway doldurur). Bu deger kayitla birlikte BUFFER'a yazilir;
  // hafizadan gonderilirken DEGISTIRILMEZ (orijinal olcum zamani korunur).
  r.timestamp = (g_last_unix > MIN_VALID_UNIX) ? (g_last_unix + millis() / 1000) : 0;

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
  g_paired = true;   // ACK aliyorsak gateway bizi kabul ediyor -> tekrar PAIR_REQ gerekmez
  if (ack.unix_time > 0) {
    g_last_unix = ack.unix_time;
    g_tz_off = ack.tz_off;   // yerel saat offset'i (yalniz gecerli zaman senkronunda)
    DEBUG_PRINT("[ACK] time sync="); DEBUG_PRINT(ack.unix_time);
    DEBUG_PRINT(" off="); DEBUG_PRINTLN(ack.tz_off);
  }
  if (ack.sleep_sec  > 0) {
    g_sleep_sec = constrain(ack.sleep_sec, MIN_SLEEP_SEC, MAX_SLEEP_SEC);
    DEBUG_PRINT("[ACK] sleep_sec="); DEBUG_PRINTLN(g_sleep_sec);
  }
  if (ack.has_settings) {   // gateway alarm esikleri + kalibrasyon offset
    g_t_low = ack.t_low; g_t_high = ack.t_high;
    g_h_low = ack.h_low; g_h_high = ack.h_high;
    g_cal_off = ack.cal_off;
    DEBUG_PRINT("[ACK] settings t=["); DEBUG_PRINT(g_t_low); DEBUG_PRINT(",");
    DEBUG_PRINT(g_t_high); DEBUG_PRINT("] cal_off="); DEBUG_PRINTLN(g_cal_off);
  }
  if (ack.special_cmd == 0xBC) {
    char uid[24]; make_uid(uid, sizeof(uid));
    DEBUG_PRINTLN("[ACK] special_cmd=0xBC -> BC gonderiliyor");
    espnow_send_bc(uid, (uint16_t)g_boot_count, ++g_pkt_counter);
  }
}

// -----------------------------------------------------------------------------
//  Uyku
// -----------------------------------------------------------------------------
static void go_to_sleep() {
  esp_task_wdt_delete(NULL);   // uykuya girmeden watchdog'dan cik
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

  // İlerleyen zaman tahmini: uyandiysak, bir onceki cycle'da uyudugumuz sure kadar
  // saati ilerlet. Boylece OFFLINE'da (ACK/senkron yokken) bile olcum zaman damgalari
  // gercekci ve aralikli olur; ACK gelince apply_ack ile otoritatif olarak resync edilir.
  if (g_last_unix > MIN_VALID_UNIX) g_last_unix += g_sleep_sec;

  power_rail_up();

  Serial.begin(115200);
  delay(50);
  DEBUG_PRINT("\n[BOOT] LEAN "); DEBUG_PRINT(FW_VERSION);
  DEBUG_PRINT("  boot#="); DEBUG_PRINTLN(g_boot_count);

  pinMode(WAKE_PIN, INPUT_PULLDOWN);

  // --- Watchdog: bu cycle bir yerde takilirsa (I2C/SPI/ESP-NOW) otomatik reset ---
  // Reset sonrasi buffer LittleFS'te kalici oldugundan veri kaybi olmaz.
  {
    esp_task_wdt_config_t twdt = { .timeout_ms = (uint32_t)WDT_TIMEOUT_MS, .idle_core_mask = 0, .trigger_panic = true };
    if (esp_task_wdt_init(&twdt) == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&twdt);
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();
  }

#if ENABLE_ENCRYPTION
  crypto_selftest();   // PMK'yi gateway kanonik test vektoruyle dogrula
#endif

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

  // 2) RADYO — yeniden kesif gerekiyorsa broadcast'e dus
#if ENABLE_ESPNOW
  espnow_set_force_broadcast(g_force_bcast);
#endif
  bool radio_ok = ENABLE_ESPNOW ? espnow_begin() : false;

  bool current_sent = false;
  bool comm_ok = false;   // bu cyclede en az bir uygulama-ACK alindi mi (kesif sagligi)
  bool ota_pending = false; // gateway ACK'te "sana OTA var" dedi mi
  if (radio_ok) {
    AckResult ack; memset(&ack, 0, sizeof(ack));

    // 2.5) PAIR: henuz eslesmediyse once PAIR_REQ (gateway allowlist binary paketi
    //      dusurmesin). Basarisiz olsa da PAIR_REQ ulastiysa gateway allowlist'ler.
    if (!g_paired) {
      char uid[24]; make_uid(uid, sizeof(uid));
      if (espnow_pair(uid, (uint16_t)g_boot_count, ++g_pkt_counter)) g_paired = true;
    }

    // 3a) Once birikmis kayitlari bosalt (en eski -> yeni). Bir tanesi bile
    //     gonderilemezse dur; kalani buffer'da kalsin (baglanti yok demektir).
    SensorRecord old;
    int drained = 0;
    while (store_peek_oldest(&old)) {
      if (espnow_send_record(old, (uint16_t)g_boot_count, ++g_pkt_counter, &ack)) {
        store_remove_oldest();
        if (ack.got_ack) comm_ok = true;
        if (ack.ota_pending) ota_pending = true;
        apply_ack(ack);
        if (old.timestamp > g_last_payload_ts) g_last_payload_ts = old.timestamp;
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
      current_sent = espnow_send_record(rec, (uint16_t)g_boot_count, ++g_pkt_counter, &ack);
      if (current_sent) { if (ack.got_ack) comm_ok = true;
                          if (ack.ota_pending) ota_pending = true;
                          apply_ack(ack);
                          if (rec.timestamp > g_last_payload_ts) g_last_payload_ts = rec.timestamp; }
      else --g_pkt_counter;
    }
  }

  // 4) Gonderilemeyen guncel olcumu pile gore sakla
  if (!current_sent) {
    DEBUG_PRINTLN("[NET] gonderim yok -> buffer'a alindi");
    store_push(rec, bpct);
  }

  // 4.3) Dayaniklilik: ard arda ACK'siz kalinirsa yeniden kesif (broadcast+PAIR).
#if ENABLE_ESPNOW
  if (radio_ok) {
    if (comm_ok) {
      g_fail_streak = 0;
      if (g_force_bcast) { g_force_bcast = false; DEBUG_PRINTLN("[NET] kesif OK -> unicast'e don"); }
    } else if (++g_fail_streak >= REDISCOVER_AFTER_FAILS) {
      g_force_bcast = true; g_paired = false; g_fail_streak = 0;
      DEBUG_PRINTLN("[NET] ard arda ACK yok -> yeniden kesif (broadcast+PAIR)");
    }
  }
#endif

  // 4.4) Adaptif TX power — bu cyclede alinan ACK RSSI'sine gore ayarla
#if ENABLE_ADAPTIVE_TX
  if (radio_ok) espnow_adapt_tx();
#endif

  // 4.5) OTA penceresi — gateway bu MAC icin OTA push edecsе yakala.
  //      BEGIN gelirse firmware alinip END'de cihaz reboot olur (asagi donmez).
#if ENABLE_OTA
  if (radio_ok) {
    if (ota_pending) DEBUG_PRINTLN("[OTA] ACK ota_pending -> uzun pencere, uyanik kal");
    espnow_ota_listen(ota_pending ? OTA_PENDING_WINDOW_MS : OTA_LISTEN_WINDOW_MS);
  }
#endif

  // Zaman senkronu BU cyclede ilk kez geldiyse rec.timestamp olcum aninda 0'di;
  // ekranda hemen gorunsun diye geriye donuk doldur.
  if (rec.timestamp <= 1000000000UL && g_last_unix > 1000000000UL) {
    rec.timestamp = g_last_unix + millis() / 1000;
    if (current_sent && rec.timestamp > g_last_payload_ts) g_last_payload_ts = rec.timestamp;
  }

  esp_task_wdt_reset();   // uzun ESP-NOW/OTA sonrasi watchdog besle

  // 5) EPD
  // İlk boot(lar)da MUTLAKA FULL: partial refresh onceden FULL ile kurulan 0x26
  // baseline'ina gore calisir; baseline yoksa ekran bos kalir.
#if ENABLE_EPD
  bool full = (g_boot_count <= 1) ||
              (EPD_FULL_REFRESH_EVERY_N_BOOTS > 0 &&
               (g_boot_count % EPD_FULL_REFRESH_EVERY_N_BOOTS) == 0);
  display_show(rec, store_total(), bpct, full, g_tz_off, g_boot_count, current_sent, g_last_payload_ts, g_alarm_start_ts);
#endif

  // 6) UYKU
  go_to_sleep();
}

void loop() { /* bos — tum is setup()'ta */ }
