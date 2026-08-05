#ifndef CONFIG_H
#define CONFIG_H
// =============================================================================
//  FAYDAM GTW202 / ESP32-C6  —  LEAN Firmware Config
//  "Uyan -> Olc -> Gonder -> (olmazsa) Sakla -> Uyu"
//
//  Bu dosya bilincli olarak MINIMALDIR. Tam surumun (dev config.h ~600 satir)
//  WORM/vault/audit/MKT/setup-wizard/OTA katmanlari LEAN'de YOKTUR.
//  Sadece: sensor okuma + ESP-NOW (AES-GCM) gonderim + offline buffer + EPD.
// =============================================================================

// -----------------------------------------------------------------------------
//  Proje kimligi (Gateway ile AYNI olmali — espnow-protocol v2.1)
// -----------------------------------------------------------------------------
#define PROJECT_ID              "FYDM-BOARD-01"
#define PROJECT_ID_HASH         0x7C9246A1UL       // SHA-256(PROJECT_ID)[0:4]
#define PROTOCOL_VERSION        2                  // v2.1
#define FW_VERSION              "L1.9.19"         // LEAN — .ino basligindaki changelog ile ayni
#define HARDWARE_MODEL          "GTW202-C6"

// -----------------------------------------------------------------------------
//  Ozellik anahtarlari
// -----------------------------------------------------------------------------
#define ENABLE_TH09C            false
#define ENABLE_DS18B20          true
#define ENABLE_MAX31865         false
#define ENABLE_EPD              true
#define ENABLE_ESPNOW           true
#define ENABLE_ENCRYPTION       true    // <-- Kullanici karari: sifreleme KALSIN (AES-128-GCM)
#define ENABLE_OTA              true    // ESP-NOW uzerinden OTA (gateway Faydam_GTW202_ESPGTW ile uyumlu)

// --- OTA (binary 0x10/0x11/0x12, AES-GCM sifreli) ---
// ⚠️ Partition semasi IKI app slotu (app0+app1) + littlefs icermeli.
//    Arduino IDE Tools -> "Default 4MB with spiffs" (app0/app1 1.2MB + spiffs 1.5MB)
//    hem OTA hem 30-gun LittleFS buffer'i karsilar.
#define OTA_LISTEN_WINDOW_MS    1500    // normal: send+ACK sonrasi BEGIN'i yakalama penceresi
#define OTA_PENDING_WINDOW_MS   30000   // ACK 'ota_pending' ise: BEGIN'i beklerken uyanik kal (30 sn)
#define OTA_IDLE_TIMEOUT_MS     30000   // chunk gelmezse OTA iptal (PC retry ~2.5s×3; 6s yetmiyordu)
#define OTA_MAX_FW_BYTES        (2u * 1024u * 1024u)  // gecerli firmware boyut ust siniri

// -----------------------------------------------------------------------------
//  Uyku / donguo
// -----------------------------------------------------------------------------
#define DEFAULT_SLEEP_SEC       120UL   // ACK'ten sleep_time_sec gelmezse varsayilan
#define MIN_VALID_UNIX          1767225600UL  // 2026-01-01; altindaki epoch gecersiz
#define WDT_TIMEOUT_MS          30000UL       // donanim watchdog: takilma -> otomatik reset
#define MIN_SLEEP_SEC           10UL
#define MAX_SLEEP_SEC           3600UL

// -----------------------------------------------------------------------------
//  ESP-NOW
// -----------------------------------------------------------------------------
#define ESPNOW_CHANNEL          6
#define ESPNOW_L2_ACK_TIMEOUT_MS  2000  // send_cb (donanim ACK) bekleme
#define ESPNOW_APP_ACK_TIMEOUT_MS 800   // uygulama ACK (JSON/binary) bekleme
#define ESPNOW_SEND_RETRIES     2
// Gateway MAC'i sifir ise broadcast ile aranir; ACK gelen adres peer olarak kaydedilir.
#define GATEWAY_MAC             {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
// Dayaniklilik: ard arda bu kadar uygulama-ACK'siz cycle -> otomatik yeniden kesif
// (broadcast + PAIR). Gateway MAC degisse/tasinsa cihaz kendini bulur.
#define REDISCOVER_AFTER_FAILS  3

// --- Adaptif TX Power (ACK RSSI'sine gore; tam firmware ile ayni esikler) ---
#define ENABLE_ADAPTIVE_TX      true
#define ESPNOW_MAX_TX_POWER     80      // 20 dBm (birim: 0.25 dBm)
#define ESPNOW_MIN_TX_POWER     28      // 7 dBm
#define ADAPTIVE_POWER_STEP     4       // dongu basina 1 dB
#define TARGET_RSSI_HIGH        (-55)   // bu ustundeyse gucu azalt
#define TARGET_RSSI_LOW         (-75)   // bu altindaysa gucu artir

// AES-GCM anahtar turetme (Gateway ile AYNI — espnow-protocol §AES-GCM)
//   PMK = SHA-256(PROJECT_ID + PMK_SALT)[:16]
#define ESPNOW_PMK_SALT         "_FYDM_PMK_SALT_V1"
#define ESPNOW_PMK_DERIVATION_VERSION 1   // gateway ile AYNI (PAIR_REQ pmk_ver)
#define GCM_IV_LEN              12
#define GCM_TAG_LEN             16

// -----------------------------------------------------------------------------
//  Offline Buffer (Tiered): RTC RAM (hizli) + LittleFS (kalici, 30+ gun)
//  RTC dolunca / pil dusukce batch halinde LittleFS'e tasinir.
//  Hedef: 10 dk periyotta 30 gun = 4320 kayit (~68KB). CAP 5000 ~ 34.7 gun.
//  ⚠️ Partition semasi LittleFS/SPIFFS icermeli: Arduino IDE Tools ->
//     "Partition Scheme: Default 4MB with spiffs" (veya littlefs'li herhangi biri).
// -----------------------------------------------------------------------------
#define RTC_BUF_MAX             120     // RTC RAM ring (hizli tampon; 10dk'da ~20 saat)
#define FLASH_CAP_RECORDS       5000    // LittleFS kalici tampon kapasitesi (~34.7 gun)
#define FLASH_BUF_PATH          "/lbuf.bin"        // LittleFS ikili kayit dosyasi
#define BUFFER_NVS_NAMESPACE    "lean_buf"         // okuma imleci (frd) burada
#define BATT_LOW_PERSIST_PERC   20      // <= %20 ise kayit dogrudan LittleFS'e (guc-kesilirse guvenli)

// -----------------------------------------------------------------------------
//  Batarya (Saft LS14500) — BAT_ADC_PIN üzerinden okuma
// -----------------------------------------------------------------------------
#define BAT_ADC_PIN             3
#define BAT_ADC_DIVIDER         2.0f    // Donanim gerilim bolucu orani (sahada dogrula!)
#define BAT_MAX_VOLT            3.65f   // %100
#define BAT_MIN_VOLT            3.35f   // %0 (3.3V ray son nokta)

// -----------------------------------------------------------------------------
//  Guc rayi — TEMP_LOGGER_BASE.ino ile ayni uyku politikasi:
//  uyanik=buck, uyku=LDO-only (buck kapali) → dusuk uyku akimi.
// -----------------------------------------------------------------------------
#define LDO_SETTLE_TIME_MS      300     // REG<->LDO make-before-break ortusmesi
#define BUCK_ON_IN_DEEP_SLEEP   false   // false = BASE ile ayni: uykuda buck OFF (LDO-only)

// -----------------------------------------------------------------------------
//  Pin tanimlari (dev config.h ile ayni)
// -----------------------------------------------------------------------------
#define REG_CTL   0
#define LDO_CTL   1
#define IO4       4
#define EPD_DC    5
#define EPD_BUSY  14
#define EPD_RES   19
#define EPD_CS    20
#define DS18_PIN  15
#define MAX_CS    21
#define WAKE_PIN  GPIO_NUM_4

// DS18B20: 12-bit ~750ms, 11-bit ~375ms. 100ms bekleme 85.00°C (POR) uretir.
#define DS18B20_RESOLUTION_BITS 11
#define DS18B20_CONV_TIMEOUT_MS 400
#define DS18B20_POR_TEMP        85.0f   // donusum bitmeden okunan gecerli-degil deger

#define SPI_MISO  2
#define SPI_CLK   6
#define SPI_MOSI  7
#define I2C_SDA   22
#define I2C_SCL   23

// MAX31865
#define MAX31865_RREF   4000.0
#define MAX31865_RNOM   1000.0

// -----------------------------------------------------------------------------
//  Sensor durum kodlari (GATEWAY_PROTOCOL §4)
// -----------------------------------------------------------------------------
#define S_STATUS_OK             0
#define S_STATUS_UNCAL          1
#define S_STATUS_DISCONNECTED   2
#define S_STATUS_HW_FAULT       3
#define S_STATUS_OUT_OF_RANGE   4

// --- Alarm limitleri (ACK settings gelmezse varsayilan) ---
#define T_LOW_LIMIT   -100.0f
#define T_HIGH_LIMIT   100.0f
#define H_LOW_LIMIT    0.0f
#define H_HIGH_LIMIT   100.0f
#define T_HYSTERESIS   0.5f     // salinim onleme
#define H_HYSTERESIS   2.0f

// --- Kalibrasyon vade takibi (TEMP_LOGGER referansi) ---
#define CAL_VALID_DAYS 180      // kalibrasyon gecerlilik suresi (gun, varsayilan)
#define CAL_WARN_DAYS  30       // son X gunde "K" uyarisi baslar
#define CAL_MAX_POINTS 8        // piecewise LUT max nokta (5-nokta kalibrasyon icin yeterli)
#define CAL_NVS_NS     "cal_data"  // kalibrasyon NVS namespace (guc-kesintisine dayanikli)

// Parametre ID'leri (espnow-protocol)
#define PARAM_TEMP   2
#define PARAM_HUM    3
#define PARAM_BATT   97
#define PARAM_STATUS 98

// -----------------------------------------------------------------------------
//  EPD model secimi (driver EPD.cpp bu makrolari bekler)
// -----------------------------------------------------------------------------
#define EPD_MODEL_E0213A373     false
#define EPD_MODEL_DIE02213S     true
// LEAN L1.9.19+: deep-sleep wake'te her zaman FULL (ino). Bu makro sakli; partial
// ayni-boot senaryosu icin EPD.cpp'de duruyor.
#define EPD_FULL_REFRESH_EVERY_N_BOOTS  1

// -----------------------------------------------------------------------------
//  Debug / log
// -----------------------------------------------------------------------------
#define ENABLE_SERIAL_DEBUG     true
#define ENABLE_DEBUG_EPD        true

#if ENABLE_SERIAL_DEBUG
  #define DEBUG_PRINT(...)    Serial.print(__VA_ARGS__)
  #define DEBUG_PRINTLN(...)  Serial.println(__VA_ARGS__)
#else
  #define DEBUG_PRINT(...)
  #define DEBUG_PRINTLN(...)
#endif

#if ENABLE_DEBUG_EPD && ENABLE_SERIAL_DEBUG
  #define EPD_DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
  #define EPD_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
  #define EPD_DEBUG_PRINT(...)
  #define EPD_DEBUG_PRINTLN(...)
#endif

#endif  // CONFIG_H
