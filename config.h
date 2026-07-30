#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// FEATURE TOGGLES
// ============================================
#define ENABLE_AI_MODULES false  // ← AI modülleri kapalı

// --- Project Security & Handshake (v5.12.0) ---
#define PROJECT_ID "FYDM-BOARD-01"
#define PROJECT_ID_HASH 0x7C9246A1

// --- Uygulama Modları (Standartlar) ---
#define MODE_BLOOD_BANK 0  // Kan Bankacılığı (AABB/EN12830)
#define MODE_EN12830 1     // Gıda/Lojistik Standartları

#define SELECTED_APP_MODE MODE_BLOOD_BANK

// --- Termal Tepki ($T_{90}$) & Damping Yönetimi ---
#define MEASUREMENT_MODE_AIR 0   // Hızlı tepki (T90 <= 150s)
#define MEASUREMENT_MODE_CORE 1  // Yazılımsal Damping (Kan Torbası Simülasyonu)
#define SELECTED_MEAS_MODE MEASUREMENT_MODE_CORE

// EMA (Exponential Moving Average) Faktörü: 1.0 = Filtresiz, 0.1 = Yüksek
// Damping
#define EMA_ALPHA_AIR 0.8f
#define EMA_ALPHA_CORE 0.15f  // Core simülasyonu için ağır damping

// --- Standartlara Göre Yapılandırma ---
#if SELECTED_APP_MODE == MODE_BLOOD_BANK
#define SAMPLING_INTERVAL_SEC 120  // 2 Dakika
#define APP_NAME "BLOOD BANK"
#elif SELECTED_APP_MODE == MODE_EN12830
#define SAMPLING_INTERVAL_SEC 120  // 10 Dakika
#define APP_NAME "EN12830 LOG"
#endif

#define EVENT_BASED_INTERVAL_SEC 60  // Alarm durumunda zorunlu kayıt periyodu
#define CAL_T90_SEC 150              // Onaylı Termal Tepki Süresi (Saniye)

// --- Sistem Versiyonu ve Kimlik ---
#define FW_VERSION "1.1.3"                      // Semantic version (Gateway sync)
#define FW_VERSION_NUM 10103                     // Numeric version
#define BC_FORMAT_VERSION 2                      // Birth Certificate JSON schema version
#define FW_CODENAME "PROBE-RESILIENT"            // Build codename
#define FW_BUILD_TAG "v5.17.15-PROBE-RESILIENT"  // Legacy internal reference
#define HARDWARE_MODEL "DS11-C6"

// --- Log level infrastructure ---
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3

// Default log level (can be overridden via build flags)
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#if ENABLE_SERIAL_DEBUG
#define LOG_ERROR(...) Serial.println(__VA_ARGS__)
#define LOG_WARN(...)                                             \
  do {                                                            \
    if (LOG_LEVEL >= LOG_LEVEL_WARN) Serial.println(__VA_ARGS__); \
  } while (0)
#define LOG_INFO(...)                                             \
  do {                                                            \
    if (LOG_LEVEL >= LOG_LEVEL_INFO) Serial.println(__VA_ARGS__); \
  } while (0)
#define LOG_DEBUG(...)                                             \
  do {                                                             \
    if (LOG_LEVEL >= LOG_LEVEL_DEBUG) Serial.println(__VA_ARGS__); \
  } while (0)
#else
#define LOG_ERROR(...)
#define LOG_WARN(...)
#define LOG_INFO(...)
#define LOG_DEBUG(...)
#endif

#define ENABLE_SERIAL_DEBUG true
#define ENABLE_BOOT_DEBUG true
#define ENABLE_API_DEBUG true
#define ENABLE_DEBUG_EPD true
#define ENABLE_DEBUG_SLEEP true   // TANI: [SLEEP]/[WAKEUP] loglarını aç (pil UART tanısı için). Üretimde false yapılabilir.

// --- Granular Debug Macros ---
#if ENABLE_SERIAL_DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)

#define DEBUG_FLUSH() Serial.flush()

#if ENABLE_BOOT_DEBUG
#define BOOT_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define BOOT_DEBUG_PRINTLN(...)
#endif

#if ENABLE_API_DEBUG
#define API_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define API_DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#else
#define API_DEBUG_PRINTLN(...)
#define API_DEBUG_PRINT(...)
#endif
#else
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_FLUSH()
#define BOOT_DEBUG_PRINTLN(...)
#define API_DEBUG_PRINTLN(...)
#define API_DEBUG_PRINT(...)
#endif

// --- Buffer & WiFi Sync Constraints (v4.30.70) ---
// WiFi, tamponda bu kadar kayıt biriktğinde açılır.
// 6 kayıt × ~10 dk örnekleme = ~1 saat birikmiş veri
// ⚠️ Alarm durumunda bu eşik devre dışıdır, Wi-Fi hemen açılır.
#define BUFFER_THRESHOLD_FORCE_WIFI 6  // Normal: 6 kayıt (~1 saat)
#define WIFI_BACKFILL_ENABLE true
#define WIFI_BACKFILL_MIN_BUFFER_COUNT 3
#define BACKFILL_TARGET_SECONDS 3600
#define BACKFILL_BATCH_MIN 1
#define BACKFILL_BATCH_MAX 12
#define PAYLOAD_SCHEMA_VERSION 1
// v5.15.3: Akıllı Zaman Güven Penceresi (ALCOA+)
// Eğer Gateway'den (ESP-NOW) zaman damgası geliyorsa WiFi açılmaz.
// Sadece son başarılı senkronizasyonun üzerinden bu süre geçerse WiFi zorlanır.
#define WIFI_MAX_TRUST_WINDOW_SEC 86400  // 24 Saat (Default)

// --- Adaptive WiFi Strategy (v4.30.75-95) ---
#define WIFI_MAX_ATTEMPTS 3           // Ardışık başarısızlık limiti
#define WIFI_BACKOFF_DURATION 3600    // 1 saat WiFi kapalı kal (save power)
#define WIFI_BASE_TIMEOUT_MS 15000    // Increased to 15s for slower routers
#define WIFI_TIMEOUT_INC_MS 1000      // +1s per retry
#define WIFI_HARD_TIMEOUT_SEC 20      // Absolute max 20s
#define WIFI_AUTO_SETUP_THRESHOLD 10  // Enter setup if 10 consecutive fails (new location detected)

// --- ESP-Now L7 Timing (Excel #Latency) ---
// Tuned for SIMPLE_LINK_MODE: measured L7 RTT is 15-40ms on local ESP-NOW link.
// 600ms gives ~15x headroom; failure worst case = 600×2 + 80ms backoff ≈ 1.3s (was 3.1s).
#define ESPNOW_L7_TIMEOUT_NORMAL 600   // Normal retry timeout (ms)
#define ESPNOW_L7_TIMEOUT_MAX 1000     // Max wait before buffering (ms) - Starvation Prevention
// SIMPLE_LINK_MODE: Keep ESP-NOW send path minimal and predictable.
// true  -> L2 ACK only, 2 retries, skip complex reboot-sync/L7 wait branches.
// false -> full advanced state machine.
#define SIMPLE_LINK_MODE true

// --- Smart Sleep Cycle (Dinamik Uyku Aralığı) ---
#define SMART_SLEEP_ENABLED true
#define SMART_SLEEP_MAX_SEC 120            // TANI: LS14500 pasivasyon/brownout testi için 600→120 (2 dk). Kondansatör eklendikten sonra 600'e geri alınabilir.
#define SMART_SLEEP_RISK_MARGIN 2.0f       // 2C yaklaşıldığında sık okuma
#define SMART_SLEEP_SAFE_MARGIN 5.0f       // 5C uzaklıktaysa seyrek okuma
#define ENABLE_SMART_SLEEP_HUMIDITY false  // Nem opsiyonel (v5.16.8)

// --- V&V: Anomaly Detection (Excel #VV) ---
#define ANOMALY_WINDOW_SIZE 5
#define MAX_TEMP_CHG_RATE 0.5f        // Max physically possible jump (C/sec)
#define ANOMALY_SIGMA_THRESHOLD 3.0f  // 3-Sigma Outlier Detection

// ⚠️ TEK SEFERLİK FORMAT: true yap → Upload → Boot → false yap → tekrar Upload
// LittleFS'i tamamen silerek hash zinciri sorunlarını kökten çözer.
#define FORCE_FORMAT_ONCE false  // ← İşim bitti, artık false!

#define PQ_TARGET_PDR 0.98            // 98% Packet Delivery Ratio Target
#define PQ_MIN_SAMPLES 500            // Minimum samples for PQ validation
#define SAMPLING_INTERVAL_15M 900     // 15-min interval for 1-year battery life
#define MAX_ALLOWED_DRIFT_SEC 300     // 5-min threshold for Time Integrity
#define HW_REVISION "BRD-V1.1-ESP32"  // Donanım Revizyonu
#define EVENT_DATA 0                  // Normal veri kaydı
#define EVENT_ALARM 1                 // Limit aşımı alarmı
#define EVENT_AUDIT 2                 // Sistem/Yapılandırma değişikliği
#define EVENT_LIMP 3                  // Kritik Düşük Pil (Limp Mode)
#define EVENT_MUTE 4                  // Yerel Alarm Susturma (Snooze)
#define EVENT_CORRUPTION 5            // Veri bozulması veya manipülasyon
#define EVENT_HW_FAULT 6              // Kritik Donanım Hatası (Sensör Kaybı vb.)
#define EVENT_SAFE_MODE 7             // Flash bozulması / FS hatası
#define EVENT_WORM_VIOLATION 8        // İzinsiz dosya silme/değiştirme girişimi
#define EVENT_SECURITY_BREACH 9       // Sertifika mismatch / MITM saldırısı
#define EVENT_IQ 10                   // Installation Qualification (İlk Kurulum Onayı)
#define EVENT_URGENT_ALARM 11         // Kritik Müdahale Gerektiren Acil Durum (v4.30.30)

// --- Sensor Health & Maintenance ---
#define HEALTH_THRESHOLD_CRITICAL 80  // %80 altı PRE_FAILURE uyarısı verir
#define HEALTH_HYSTERESIS 5           // %5 histerezis (stabilite için v4.30.65)

// --- SSL/TLS Certificate Pinning (ALCOA+ Originality) ---
// Server Certificate SHA-256 Fingerprint (Must match for transmission)
#define SERVER_CERT_PIN "6B 41 8C 4D 41 08 34 22 28 C9 41 80 43 14 0C 4D 2E 10 0C 4D"  // Example

const char FAYDAM_AWS_ROOT_CA[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
    "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
    "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
    "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
    "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
    "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
    "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
    "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
    "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
    "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
    "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
    "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
    "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
    "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
    "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
    "-----END CERTIFICATE-----\n";

// --- Mutual TLS (G-4) ---
// If defined, these will be used for client authentication with the server.
// In production, these should be provisioned to NVS.
const char FAYDAM_CLIENT_CERT[] = "";
const char FAYDAM_CLIENT_KEY[] = "";

// --- Donanım Watchdog ---
#define WDT_TIMEOUT_MS 60000  // 60 sn donma = yeniden başlatma

// --- Setup Wizard ---
#define WIZARD_INACTIVITY_TIMEOUT_MS 40000  // AP modunda bekleme süresi (ms)

// --- Felaket Kurtarma & Safe Mode ---
#define SAFE_MODE_ENABLED true        // Flash bozulmasında ekran+sensor fallback
#define FS_MAX_RETRY 2                // initFS: max yeniden deneme sayısı
#define BOOT_SLOT_NS "boot_ctrl"      // NVS namespace: A/B boot takibi
#define BOOT_SLOT_KEY "active_slot"   // NVS key: "A" veya "B"
#define BOOT_FAIL_CNT_KEY "fail_cnt"  // NVS key: başarısız boot sayacı
#define BOOT_MAX_FAILURES 3           // Bu kadar başarısız boot = Safe Mode
#define SAFE_MODE_AUTO_RESET_SEC 300  // Safe mode'da otomatik toparlanma süresi (5dk)
#define INTEGRITY_CHECK_INTERVAL 500  // Her 500 boot'ta bir tam bütünlük kontrolü yap

// --- WORM (Write Once, Read Many) & Secure Vault (Excel #Vault) ---
#define ENABLE_SECURE_VAULT (SELECTED_APP_MODE == MODE_BLOOD_BANK)
#define STORAGE_WARN_THRESHOLD 0.70f  // %70 dolulukta uyarı ver (Aşınma dengeleme koruması)
#define VAULT_MEMORY_HARD_STOP 0.80f  // %80 dolulukta yazmayı durdur (En az %20 boş alan garanti edilir)
#define VAULT_ENCRYPTION_KEY_NS "vault_sec"

// --- Veri Yedekleme ve Arşiv (The Vault) ---
#define ENABLE_AUDIT_LOG true  // Master switch for ALCOA+ Audit Trail
#define ENFORCE_HTTPS \
  false  // M-5: Force TLS for all cloud communications.
         // NOTE: Setting this to 'false' enables [TURBO] HTTP mode
         // which reduces sync time by ~800ms but introduces MITM risks.
         // Use 'false' ONLY in physically secured internal networks.
#define ENABLE_OFFLINE_BUFFER true
#define BUFFER_TARGET_DAYS 33
#define BUFFER_HARD_STOP_DROP_OLDEST true
#define MAX_BUFFER_RECORDS 3000        // 10dk periyotta ~20 gün hedef (Flash aşınma koruması için azaltıldı)
#define BUFFER_MAX_FILE_BYTES 800000   // LittleFS buffer dosya boyutu hard cap (800KB)
#define MAX_ARCHIVE_RECORDS 23760      // ~33 Günlük veri (2 dk aralıkla)

#define BUFFER_FILE_PATH "/data_buffer.json"
#define ARCHIVE_FILE_PATH "/archive.csv"
#define AUDIT_LOG_PATH "/audit_log.csv"
#define AUDIT_BAK_PATH "/audit_bak.csv"
#define AUDIT_SYNC_THRESHOLD_KB 50   // 50KB'ı geçince otomatik buluta aktar ve sil
#define AUDIT_ROTATE_MAX_LINES 1000  // Hash chain verification WDT-safe limit

// Audit Log Contract v1.1 — EVT= formatı için yeni dosya
#define AUDIT_V11_LOG_PATH   "/audit_v11.log"
#define AUDIT_V11_BAK_PATH   "/audit_v11.bak"
#define AUDIT_V11_MAX_LINES  2000     // Rotation eşiği
#define AUDIT_LINE_MAX_BYTES 180      // Contract §3.3: satır uzunluk limiti

// RISK-2 FIX: Gerçek credential'lar kaynak kodda BULUNMAZ.
// Üretimde Web Wizard veya Serial provisioning ile NVS'e yazılır.
// Bu boş makrolar yalnızca NVS'te değer yoksa derleme uyumluluğu için fallback'tir.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// --- Eskalasyon ve Uzaktan Onay ---
#define ACK_CHECK_ENABLED true
#define SERVER_URL "http://datalogger.faydam.com"
#define LEGAL_SERIAL_NO "BRD-2026-X1"  // Yasal/Etiket Seri Numarası

// RISK-2 FIX: API kimlik bilgileri kaynak kodda BULUNMAZ.
// Üretimde NVS SETTINGS_NS: faydam_user, faydam_pass, faydam_apikey ile sağlanır.
// Web Wizard veya Serial komut ile provisioning yapılır.
#define FAYDAM_AWS_USER ""
#define FAYDAM_AWS_PASS ""
#define FAYDAM_AWS_KEY ""

// --- Zaman ve Güvenlik Sunucuları ---
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 10800  // Turkey GMT+3 (Standardized)
#define DAYLIGHT_OFFSET_SEC 0

// Minimum geçerli Unix zaman damgası — bu değerin altındaki her timestamp geçersiz sayılır.
// 1767225600 = 2026-01-01 00:00:00 UTC. Cihaz 2026'da üretildiğinden daha eski timestamp imkansız.
#define MIN_VALID_UNIX 1767225600UL

// --- ESP-Now & Hybrid Network Security (ALCOA+ Originality) ---
#define CHUNK_MAGIC_BYTE 0xFE
#define CHUNK_HEADER_SIZE 5
#define CHUNK_MAX_DATA_LEN 210

#define ENABLE_ESPNOW true
#define DEFAULT_ESPNOW_CHANNEL 6  // Requested fixed channel
// Security profile selector:
// 2 = STRICT   (field production default)
// 1 = BALANCED (recommended for controlled pilot deployments)
// 0 = LAB      (debug/bench speed priority)
#define SECURITY_PROFILE 1

#if SECURITY_PROFILE == 2
#define ESPNOW_REBOOT_SYNC_MODE_STRICT true
#elif SECURITY_PROFILE == 1
#define ESPNOW_REBOOT_SYNC_MODE_STRICT false
#else
#define ESPNOW_REBOOT_SYNC_MODE_STRICT false
#endif
#define ESPNOW_ENCRYPT_ACTIVE \
  false // HW LMK şifreleme devre dışı — uygulama katmanı AES-128-GCM yeterli.
        // HW encryption, Wizard olmayan deployment'larda LMK senkron sorununa
        // neden oluyor (NVS drift): Gateway rx_cb tetiklenmiyor, sensor L7 ACK
        // alamıyor, "healthy" ama data görünmüyor. AES-GCM double-encryption'a
        // gerek yok. NVS boot sync: syncEncryptionNvs() her cold-boot'ta çalışır.
// Bu makrolar YALNIZCA NVS boşsa ve türetme başarısız olursa geliştirme fallback'idir.
#define ESPNOW_PMK_DEV_FALLBACK "pmk2361936199800"
#define ESPNOW_LMK_DEV_FALLBACK "LMK_v4_LOCAL_X01"
#define ESPNOW_PMK_DERIVE_SALT "_FYDM_PMK_SALT_V1"
#define ESPNOW_PMK_DERIVATION_VERSION 1
#define ESPNOW_USE_DYNAMIC_KEYS true  // Chip-specific key derivation (Gateway ile sync)
// Gateway/Master MAC (Cihazın bağlı olduğu ana ünite)
// 00:00:00:00:00:00 olarak bırakıldığında cihaz otomatik olarak Gateway arar ve
// bulduğunda kaydeder.
#define GATEWAY_MAC {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
// --- Adaptive TX Power Control (PQ Optimization) ---
#define ESPNOW_MAX_TX_POWER 80  // 20dBm (Max)
#define ESPNOW_MIN_TX_POWER 28  // 7dBm (Min for stability)
#define ADAPTIVE_POWER_STEP 4   // 1dB change per cycle
#define TARGET_RSSI_HIGH -55    // Below this, we can reduce power
#define TARGET_RSSI_LOW -75     // Above this, we must increase power

// --- FDA 21 CFR Part 11 & ALCOA+ Kayıt Kimlikleri ---
#define OPERATOR_ID "OP-ADMIN"

#define DEVICE_ROLE "MASTER_LOG"

// --- Envanter ve Demirbaş Bilgileri ---
#define ASSET_ID "DEMIRBAS-000"                      // Demirbaş Numarası
#define INSTALL_DATE "2026-02-24"                    // Kurulum Tarihi
#define DEVICE_LOCATION "DEFAULT_LOC"                // Cihazın Bulunduğu Yer (Örn: Dolap-04)
#define SIGNED_OTA_ACTIVE true                       // İmzalı OTA aktif
#define HMAC_KEY "5D8E4A0B6C2E1D9F8A7B5C4F3E2D1C0B"  // HMAC doğrulama anahtarı (G-5)

// Dynamic hardware-bound secret (G-3)
// DEVICE_SECRET removed to prevent static analysis theft. Secret is derived from Efuse once.
#define ENABLE_DS18B20 false
#define ENABLE_TH09C true
#define ENABLE_MAX31865 false
#define ENABLE_EPD false
// Ghosting temizliği: her N uyanışta bir tam (blocking) yenileme; arada partial async.
// 10 = daha az tam ekran, biraz daha ghosting riski (sahada STATUS.md ile onaylanır).
#define EPD_FULL_REFRESH_EVERY_N_BOOTS 10
#define EPD_MODEL_E0213A373 false // Original model (no longer active)
#define EPD_MODEL_DIE02213S true  // Active: DIE02213S (SSD1680Z/JD79661)
#define EPD_CS 20
#define EPD_DC 5
#define EPD_RES 19
#define EPD_BUSY 14
#define ENABLE_WIFI true  // WiFi & Cloud gönderimi (v4.30.20+ Smart Fallback)

// --- MKT (Mean Kinetic Temperature) Parametreleri ---
#define MKT_DH 83.144   // kJ/mol (Tipik ilaç kararlılık değeri)
#define MKT_R 0.008314  // kJ/(mol*K) (Gaz sabiti)
#define MKT_K_OFFSET 273.15
#define MKT_SAMPLE_WINDOW 720  // 24 saatlik pencere (2 dk aralıkla)
// --- Logging & Debugging (Cleaned for v5.15.13) ---
#define DEBUG_NO_SLEEP false  // Development: Disable deep sleep (prevents locking serial)

// 1: Derin uyku zamanlayıcısı = örnek aralığı (sn) − bu döngüde uyanık geçen süre;
//    böylece ardışık örnekler ~sample_int sn aralıklı kalır (ayar: SettingsManager).
#define SLEEP_SUBTRACT_AWAKE_TIME 1

// --- Sensör Kalibrasyon Katsayıları ---
#define DS18B20_OFFSET 0.0
#define TH09C_T_OFFSET 0.0
#define TH09C_H_OFFSET 0.0
#define MAX31865_OFFSET 0.0

// --- Standart sensör durum kodları (tek kaynak; GATEWAY_PROTOCOL.md §4 ile aynı) ---
#define S_STATUS_OK 0            // Çalışıyor
#define S_STATUS_UNCAL 1         // Kalibrasyon gerekli / süresi dolmuş
#define S_STATUS_DISCONNECTED 2  // Prob algılanmadı
#define S_STATUS_HW_FAULT 3      // I2C/SPI vb. donanım hatası
#define S_STATUS_OUT_OF_RANGE 4  // Eşik alarmı (ölçüm limit dışı)

// --- Alarm Limitleri ---
#define T_HIGH_LIMIT 100.0  // Üst Sıcaklık Limiti
#define T_LOW_LIMIT -100.0  // Alt Sıcaklık Limiti
#define T_HYSTERESIS 0.5    // Sıcaklık Histerezis (Salınım Önleme)
#define H_HIGH_LIMIT 100.0  // Üst Nem Limiti
#define H_LOW_LIMIT 0.0     // Alt Nem Limiti
#define H_HYSTERESIS 2.0    // Nem Histerezis

// --- Kademeli Güç Yönetimi (Tiered Power Management) ---
#define BAT_LOW_PERC 20      // %20: Düşük Pil Uyarısı
#define BAT_LIMP_PERC 10     // %10: Limp Mode
#define BAT_SHUTDOWN_PERC 5  // %5: Güvenli Kapanış

// --- Heartbeat & Downtime Partition ---
#define HEARTBEAT_NS "sys_heartbeat"
#define HEARTBEAT_TS_KEY "last_ts"
#define HEARTBEAT_REASON_KEY "last_reason"

// --- Armor: Güç ve Fiziksel Güvenlik ---
#define LIMP_MODE_SAMPLING_MULT 4  // Limp modda uyku süresi çarpanı
#define LIMP_MODE_EPD_SKIP 4       // Limp modda kaç döngüde bir ekran güncellenecek
#define LIMP_MODE_WIFI_SKIP 2      // Limp modda kaç döngüde bir WiFi veri gönderilecek
#define PHYSICAL_SEAL_ACTIVE true  // IP65 ve Void Seal kontrolü için mantıksal marker

// --- Pil Parametreleri ---
#define BAT_ADC_PIN \
  3                        // Pil Voltaj Okuma Pini (Hardware Rev 1.1: Moved to Pin 3 to avoid Pin 1
                           // LDO mismatch)
#define BAT_MAX_VOLT 3.65  // Saft LS14500 Pik Voltaj (Yük Altında)
#define BAT_MIN_VOLT 3.35  // Kritik Limit (3.3V Rail İçin Son Nokta)

// DEPRECATED: Battery threshold check removed - Gateway monitors "bt" field in data packets
// #define BATTERY_CAPA_THRESHOLD_MV 3300  // 3.3V = Low battery threshold

// --- Pin Tanımları ---
#define REG_CTL 0       // Buck Regulater Enable Pin
#define LDO_CTL 1       // LDO Enable Pin
#define IO4 4           // IO4 LP Wake Pin
#define EPD_DC 5        // E-Paper Data/Commannd Pin
#define EPD_BUSY 14     // E-Paper Busy Pin
#define EPD_RES 19      // E-Paper Reset Pin
#define EPD_CS 20       // E-Paper SPI Chip Select Pin
#define DS18_PIN 15     // DS18B20 Data Pin
#define MAX_CS 21       // MAX31865 SPI Chip Select Pin
#define USB_DET_PIN 11  // Boardoza v1.1 USB Detection Pin

#define WAKE_PIN GPIO_NUM_4  // IO4

#define SPI_MISO 2  // SPI MISO Pin
#define SPI_CLK 6   // SPI Clock Pin
#define SPI_MOSI 7  // SPI MOSI Pin

#define I2C_SDA 22  // I2C Serial Data Pin (Manufacturer Default)
#define I2C_SCL 23  // I2C Serial Clock Pin (Manufacturer Default)

// --- MAX31865 Ayarları ---
#define MAX31865_RREF 4000.0  // 4k for PT1000 Precision
#define MAX31865_RNOM 1000.0

// --- Kalibrasyon Yönetimi (ALCOA+ Accurate) ---
#define CAL_NS "cal_data"
// Kalibrasyon bitiş tarihi (Unix Epoch - Örn: 2026-08-01 = 1753920000)
#define CAL_EXPIRY_TS 1753920000UL  // 2026-08-01
#define CAL_VERIFY_DAYS 180         // 6 aylık ara kontrol (doğrulama) periyodu
#define CAL_WARN_DAYS 30            // Son X günde uyarı başlar
#define CAL_WARN_SECS (CAL_WARN_DAYS * 86400UL)
// --- Kalibrasyon Kimlik Yönetimi (Dinamik) ---
#define CAL_CERT_MAX31865 "CAL-MAX-2026"
#define CAL_CERT_TH09C "CAL-TH01-2026"
#define CAL_CERT_DS18B20 "CAL-DS15-2026"
#define CAL_CERT_DEFAULT "CAL-GEN-2026"

// K-6 FIX: Define a single default CAL_CERT_ID fallback without compile-time elif chains
#define CAL_CERT_ID CAL_CERT_DEFAULT

#define SHIFT_RESET_INTERVAL_SEC (8 * 3600)   // 8 Saatlik Vardiya
#define DAILY_RESET_INTERVAL_SEC (24 * 3600)  // 24 Saatlik Günlük

// --- Metrological Uncertainty Budget (Excel #Uncertainty) ---
#define UNCERTAINTY_UA 0.05             // Type A: ADC Noise/Statistical (k=1)
#define UNCERTAINTY_UB_SENSOR 0.10      // Type B: Sensor Base Tolerance (k=1)
#define UNCERTAINTY_UB_CABLE 0.03       // Type B: Cable Resistance/Offset (k=1)
#define UNCERTAINTY_UB_HEATING 0.02     // Type B: Self-Heating (k=1)
#define UNCERTAINTY_AGING_COEFF 0.0001  // Drift per day
#define UNCERTAINTY_K_FACTOR 2.0        // Coverage Factor (k=2) for 95% confidence
#define UNCERTAINTY_WARN_LIMIT 0.5      // Threshold for Metrological Warning

// --- Sesli Alarm ve Snooze ---
#define BUZZER_PIN 8            // Sesli Alarm (Buzzer) Pini
#define SNOOZE_DURATION_MIN 15  // Susturma Süresi (Dakika)
// Snooze: operatör susturduğunda RTC snooze_end_ts güncellenir; handleBuzzer(1)
// içinde now < snooze_end_ts ise bip çalmaz (SensorManager.cpp).
#define BUZZER_PROFILE 1

// --- Early EPD Headroom Management (v5.0.0) ---
#define EPD_HEADROOM_MS_USB 200
#define EPD_HEADROOM_MS_BATTERY 200
#define EPD_HEADROOM_MS_BAT_LOW 250
#define EPD_HEADROOM_MS_LIMP 200

// --- EPD Safe Mode Screen Layout ---
#define EPD_SAFE_ALERT_BAR_H 30
#define EPD_SAFE_TITLE_X 10
#define EPD_SAFE_TITLE_Y 5
#define EPD_SAFE_BOX_X1 8
#define EPD_SAFE_BOX_Y1 42
#define EPD_SAFE_BOX_X2 (EPD_WIDTH - 8)
#define EPD_SAFE_BOX_Y2 78
#define EPD_SAFE_CENTER_TITLE_X 20
#define EPD_SAFE_CENTER_TITLE_Y 48
#define EPD_SAFE_REASON_X 18
#define EPD_SAFE_REASON_Y 64
#define EPD_SAFE_RECOVERY_TITLE_X 10
#define EPD_SAFE_RECOVERY_TITLE_Y 86
#define EPD_SAFE_RECOVERY_ROW_X 20
#define EPD_SAFE_RECOVERY_ROW_START_Y 104
#define EPD_SAFE_RECOVERY_ROW_GAP_Y 25

// --- ESP-NOW Pairing Self-Heal ---
#define ESPNOW_PAIRING_DECRYPT_FAIL_SELF_HEAL_THRESHOLD 3

// --- Çok Seviyeli Erişim Kontrolü (ALCOA+ Attributable) ---
#define ACCESS_MAX_ATTEMPTS 3        // Max başarısız deneme sayısı
#define ACCESS_LOCKOUT_SEC 300       // Kilitleme süresi (5 dakika)
#define ACCESS_LOG_NS "access_ctrl"  // NVS namespace
#define SETTINGS_NS "device_cfg"     // NVS namespace for global settings

// Varsayılan şifre hash'leri (SHA-256)
#define USER_HASH_DEFAULT \
  "a92f03657738f656094254c03b11801c8019e072a2e896f2e22c0702d8da0904"  // user:1234
#define ADMIN_HASH_DEFAULT \
  "1cd44a04d2c88439e6dfa0ce3e4b78809a5b3e60f0658fa998fa7297e6e5a639"  // admin:admin
#define MAINTAINER_HASH_DEFAULT \
  "8c77b1ff76ca327f537630a6223134984d4829cc80f539717d27e3fb65a6be1b"  // maintainer:masterpass

// Erişim rolleri
#define ACCESS_ROLE_NONE 0
#define ACCESS_ROLE_USER 1
#define ACCESS_ROLE_ADMIN 2
#define ACCESS_ROLE_MAINTAINER 3

// Macros moved to top of file for reliability.

// --- Specialized Component Debugging ---
#if ENABLE_DEBUG_EPD && ENABLE_SERIAL_DEBUG
#define EPD_DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define EPD_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define EPD_DEBUG_PRINT(...)
#define EPD_DEBUG_PRINTLN(...)
#endif

#if ENABLE_DEBUG_SLEEP && ENABLE_SERIAL_DEBUG
#define SLEEP_DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define SLEEP_DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define SLEEP_DEBUG_PRINT(...)
#define SLEEP_DEBUG_PRINTLN(...)
#endif

#define ENABLE_PERF_PROFILER 1
#define ENABLE_NET_PHASE_PROFILER ENABLE_PERF_PROFILER

// --- Engineering & Metrology Constants (D-1 Cleanup) ---
// Kart tasarımcısı spec'i: REG↔LDO geçişlerinde HER İKİ yönde 300 ms
// make-before-break örtüşmesi (buck soft-start / LDO devri için).
#define LDO_SETTLE_TIME_MS 300
// Deep sleep boyunca buck (REG_CTL) açık kalsın → uyanışta ray güçlü olur, boot
// inrush'ında brownout (E BOD) önlenir. Zayıf LDO tek başına boot akımını
// karşılayamıyordu. Trade-off: uyku akımı artar (primer pil ömrü düşer). Bulk
// kondansatör eklenince veya düşük empedanslı pil ile false yapılıp LDO-only
// (düşük güç) moduna dönülebilir.
#define BUCK_ON_IN_DEEP_SLEEP true
// DENENDİ ve REGRESYON: uyanışta 150ms sürekli CPU yükü ("buck PFM warm-up")
// ölüm noktasını İLERİ değil GERİYE taşıdı (E BOD banner'dan bile önce, ~229ms).
// Sürekli yük, pasive LS14500 hücresinin kaynak voltajını daha da çökertti →
// sorun regülatör modu değil, PİL İÇ DİRENCİ/PASİVASYONU. 0 = kapalı bırak.
#define BUCK_PWM_WARMUP_MS 0

// Brownout Detector. KAPALI DENENDİ ve yanlış çıktı: BOD kapalıyken cihaz aynı
// noktada (ilk flash yazımı) resetlenmek yerine SESSİZCE KİLİTLENDİ → çöküş
// gerçek, BOD işini yapıyormuş. Açık kalmalı — hem flash'ı korur hem "E BOD"
// mesajı net tanı sinyali verir. Asıl çözüm: uykuda LDO+buck birlikte açık
// (aşağıdaki BUCK_ON_IN_DEEP_SLEEP bloğu — buck burst-mode çıkış gecikmesini
// LDO köprüler).
#define DISABLE_BROWNOUT_DETECTOR false
#define DS18B20_CONV_TIMEOUT_MS 400
#define DS18B20_RESOLUTION_BITS 11
#define ESPNOW_L2_ACK_TIMEOUT_MS 2000
#define ESPNOW_L7_RECOVERY_MS 800
#define FS_CHAIN_CHECK_MIN_SIZE 4096
#define FS_LOCK_TIMEOUT_MS 5000
#define EPD_BUSY_TIMEOUT_MS 5000
#define EPD_SHUTDOWN_DELAY_MS 100
#define POLLING_DELAY_MS 10
#define MAX_SENSORS 3
#define MAX_DIAG_CODES 10

#endif  // CONFIG_H