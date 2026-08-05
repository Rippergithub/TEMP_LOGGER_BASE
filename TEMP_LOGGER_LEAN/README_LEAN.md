# TEMP_LOGGER_LEAN — Hafif Firmware

FAYDAM GTW202 / ESP32-C6. Tam sürümün (WORM/vault/audit/MKT/wizard/OTA)
hafif türevi. Tek iş: **Uyan → Ölç → Gönder → (olmazsa) Sakla → Uyu**.

## Akış (setup içinde, loop boş)
1. Güç rayı kur — uyanık: buck; uyku: LDO-only (`BUCK_ON_IN_DEEP_SLEEP=false`, TEMP_LOGGER_BASE ile aynı)
2. TH09C oku (+ops. DS18B20/MAX31865), batarya oku
3. ESP-NOW başlat (CH6, AES-128-GCM) — `WiFi.mode(STA)` = brownout tanısındaki *step1c*
4. Önce buffer'daki birikmiş kayıtları gönder (en eski→yeni), sonra bu ölçümü
5. Gönderilemeyen kaydı **pile göre sakla**: normal→RTC RAM, ≤%20→FLASH (NVS)
6. ACK'ten `unix` zaman-sync + `sleep_time_sec` al; `special_cmd=0xBC` ise BC gönder
7. EPD güncelle (her N boot'ta full, arada partial), radyo kapat, deep sleep

## Tiered offline buffer (kullanıcı kararı)
- **RTC RAM ring** (`RTC_BUF_MAX=120`) → hızlı tampon; deep-sleep'te korunur, flash aşınması yok
- **LittleFS dosyası** (`FLASH_CAP_RECORDS=5000`) → kalıcı tampon, **30+ gün** (10dk periyot → 34.7 gün); güç kesilse de kalır
- RTC dolunca tüm RTC **batch** halinde LittleFS'e taşınır (online'da flash'a hiç yazılmaz → aşınma yok). Pil ≤%20 ise kayıt doğrudan LittleFS'e
- Drain sırası: önce LittleFS (eski), sonra RTC (yeni)
- Okuma imleci NVS'te (`frd`); dosya bitince truncate, imleç büyüyünce compaction

### Kapasite hesabı (10 dk periyot)
| Süre | Kayıt | Boyut (16B/kayıt) |
|---|---|---|
| 1 gün | 144 | 2.3 KB |
| 30 gün | 4320 | ~68 KB |
| CAP (5000) | 5000 | ~78 KB (~34.7 gün) |

⚠️ **Partition:** LittleFS/SPIFFS içeren şema seçilmeli — Arduino IDE Tools →
"Partition Scheme: Default 4MB with spiffs" (varsayılan ~1.5MB LittleFS; 78KB rahat sığar).

## Şifreleme (kullanıcı kararı: KALSIN) — Gateway `Faydam_GTW202_ESPGTW` ile eşlendi
Gateway kaynağından (espnow_security.cpp + docs/GATEWAY_SPEC.md + SECURITY.md) **doğrulanan** kontrat:
- AES-128-GCM, `PMK = SHA-256("FYDM-BOARD-01" + "_FYDM_PMK_SALT_V1")[:16]`
  - Test vektörü: `1680e9159118feb685a24c7e1e78bba3` (cihazda `crypto_selftest()` doğrular)
- **IV (12B) = `[Sender_MAC:6][boot_cnt:2 BE][nonce:4 BE]`** ✓
- `boot_cnt` = uint16 (NVS, her boot++), `nonce` = uint32 (her paket++)
- 16B TAG; `SensorDataMessage` struct gateway `espnow_protocol.h` ile **birebir** ✓
- Anti-replay: `boot_cnt<son` veya (eşit boot_cnt && `nonce<=son`) → gateway DROP

### Çerçeve düzeni — BYTE-EXACT (gerçek kütüphane vendor'landı)
Tahmin kaldırıldı: ortak `EspNowCrypto.h/.cpp` bu klasöre kopyalandı ve
`lean_crypto.cpp` doğrudan onu çağırıyor. Gateway + sensör ile aynı kaynak:
```
Çerçeve:  [0xAE magic:1][nonce_ctr LE:4][boot_cnt LE:2][CIPHERTEXT:N][GCM TAG:16]   (overhead 23B)
IV (12B): [sender_mac:6][boot_cnt LE:2][nonce_ctr LE:4]
AAD:      yok (NULL)
```
`crypto_selftest()` PMK'yi kanonik vektörle (`1680e9…bba3`) doğrular. `lean_crypto`
artık yalnızca PMK türetimi + kendi MAC'i + kolay imza sağlayan ince bir sarmalayıcı.

## OTA (ESP-NOW üzerinden, gateway `Faydam_GTW202_ESPGTW` ile uyumlu)
Gateway'in gönderdiği **binary** OTA paketleri (AES-GCM şifreli) ile firmware güncelleme:
- `BEGIN 0x10`: `[type1][file_size 4 LE][md5 32]` → `Update.begin` + `Update.setMD5`
- `DATA 0x11`: `[type1][index 2][total 2][data_len 1][data ≤200]` → `Update.write`
- `END 0x12`: `Update.end()` başarılıysa `ESP.restart()`

Akış: her cycle'da send+ACK sonrası **kısa OTA dinleme penceresi** (`OTA_LISTEN_WINDOW_MS`).
Gateway bu MAC için OTA kuyruğa aldıysa BEGIN gönderir; sensör OTA döngüsüne girip
(idle timeout `OTA_IDLE_TIMEOUT_MS` ile korumalı) tüm chunk'ları alır, END'de reboot eder.
BEGIN gelmezse pencere bitince normal akış (EPD + uyku) sürer. `ENABLE_OTA=false` ile kapatılır.

⚠️ **Partition:** OTA iki app slotu ister. `"Default 4MB with spiffs"` (app0+app1+spiffs)
hem OTA hem 30-gün LittleFS buffer'ı karşılar.

## Dosyalar
| Dosya | İş |
|---|---|
| `TEMP_LOGGER_LEAN.ino` | Orkestrasyon (7 adım) |
| `config.h` | Lean config (~150 satır) |
| `lean_types.h` | `SensorRecord` (16B) + `SensorDataMessage` (binary paket) |
| `lean_store.*` | Tiered RTC/FLASH buffer |
| `lean_crypto.*` | PMK türetimi + `EspNowCrypto` sarmalayıcı |
| `EspNowCrypto.*` | Ortak AES-128-GCM kütüphanesi (gateway/sensör ile aynı, vendor'landı) |
| `lean_espnow.*` | init / send+ACK / BC / OTA dinleme |
| `lean_ota.*` | ESP-NOW OTA alıcısı (Update.h + MD5) |
| `lean_display.*` | EPD dashboard (Waveshare GUI_Paint) |
| `EPD.*`, `TH09C.*`, `Boardoza_MAX31865.*` | dev sürümden kopyalanan sürücüler |

## Bağımlılıklar
- ArduinoJson (ACK parse)
- **EPD kütüphanesi vendor'landı** — `GUI_Paint.*`, `Fonts.*`, `EPD.*`, `EPD_SPI.h`
  `Faydam_GTW202_TEMP` reposundan birebir kopyalandı (harici kütüphaneye gerek yok).
  Ekran düzeni (dashboard) o repodaki `DisplayManager::epd_update_data` layout'una göre:
  3px çerçeve + header (MAC/sinyal/pil) + sol sıcaklık/nem çerçevesi (Font20) +
  sağ "SON DATA"/saat paneli (Font24) + footer bar — yönetici bağımlılıkları olmadan.
- ESP32 Arduino core 3.x (ESP32-C6, IDF5 ESP-NOW recv API)

## Derleme ayarları
Flash 40MHz/DIO, CPU 80MHz, Erase Flash Disabled.

## Sahada doğrulanacaklar
- `BAT_ADC_DIVIDER` (donanım gerilim bölücü oranı)
- GCM çerçeve düzeninin gateway ile uyumu (yukarıdaki interop notu)
- `step1c` (WiFi radyo açılış darbesi) kondansatör rework'ü sonrası brownout testi
