# TEMP_LOGGER_LEAN — Hafif Firmware

FAYDAM GTW202 / ESP32-C6. Tam sürümün (WORM/vault/audit/MKT/wizard/OTA)
hafif türevi. Tek iş: **Uyan → Ölç → Gönder → (olmazsa) Sakla → Uyu**.

## Akış (setup içinde, loop boş)
1. Güç rayı kur — buck+LDO make-before-break, `BUCK_ON_IN_DEEP_SLEEP` (brownout fix aynen korundu)
2. TH09C oku (+ops. DS18B20/MAX31865), batarya oku
3. ESP-NOW başlat (CH6, AES-128-GCM) — `WiFi.mode(STA)` = brownout tanısındaki *step1c*
4. Önce buffer'daki birikmiş kayıtları gönder (en eski→yeni), sonra bu ölçümü
5. Gönderilemeyen kaydı **pile göre sakla**: normal→RTC RAM, ≤%20→FLASH (NVS)
6. ACK'ten `unix` zaman-sync + `sleep_time_sec` al; `special_cmd=0xBC` ise BC gönder
7. EPD güncelle (her N boot'ta full, arada partial), radyo kapat, deep sleep

## Tiered offline buffer (kullanıcı kararı)
- **Pil normal** → `RTC_DATA_ATTR` ring buffer (`RTC_BUF_MAX=120`), deep-sleep'te korunur, flash aşınması yok
- **Pil bitmeye yakın (≤%20)** → NVS/FLASH (`FLASH_BUF_MAX=400`), güç kesilse de kalıcı; ayrıca eşiğin altına düşünce RTC'deki kayıtlar flash'a taşınır
- Drain sırası: önce FLASH (kalıcı/eski), sonra RTC

## Şifreleme (kullanıcı kararı: KALSIN)
- AES-128-GCM, `PMK = SHA-256("FYDM-BOARD-01" + "_FYDM_PMK_SALT_V1")[:16]`
- Çerçeve: `[IV:12][TAG:16][CIPHERTEXT]`, AAD = `PROJECT_ID_HASH`
- ⚠️ **İnterop:** Bu çerçeve/AAD düzeni Gateway'in decrypt tarafıyla birebir aynı olmalı.
  Gateway farklı düzen kullanıyorsa yalnızca `lean_crypto.cpp` + `config.h GCM_*` değişir.

## Dosyalar
| Dosya | İş |
|---|---|
| `TEMP_LOGGER_LEAN.ino` | Orkestrasyon (7 adım) |
| `config.h` | Lean config (~150 satır) |
| `lean_types.h` | `SensorRecord` (16B) + `SensorDataMessage` (binary paket) |
| `lean_store.*` | Tiered RTC/FLASH buffer |
| `lean_crypto.*` | AES-128-GCM (mbedtls) |
| `lean_espnow.*` | init / send+ACK / BC |
| `lean_display.*` | EPD dashboard (Waveshare GUI_Paint) |
| `EPD.*`, `TH09C.*`, `Boardoza_MAX31865.*` | dev sürümden kopyalanan sürücüler |

## Bağımlılıklar
- ArduinoJson (ACK parse)
- Waveshare `GUI_Paint.h` + `fonts.h` (EPD; yoksa `ENABLE_EPD=false`)
- ESP32 Arduino core 3.x (ESP32-C6, IDF5 ESP-NOW recv API)

## Derleme ayarları
Flash 40MHz/DIO, CPU 80MHz, Erase Flash Disabled.

## Sahada doğrulanacaklar
- `BAT_ADC_DIVIDER` (donanım gerilim bölücü oranı)
- GCM çerçeve düzeninin gateway ile uyumu (yukarıdaki interop notu)
- `step1c` (WiFi radyo açılış darbesi) kondansatör rework'ü sonrası brownout testi
