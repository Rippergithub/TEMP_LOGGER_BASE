# CLAUDE.md — TEMP_LOGGER_BASE / Faydam GTW202 (ESP32-C6)

Bu dosya, bu repoda çalışan Claude Code oturumları için proje rehberidir.

## Proje

FAYDAM GTW202 sıcaklık/nem logger. **ESP32-C6**, Saft LS14500 primer pil, e-paper
(SSD1680 / DIE02213S), TH09C (I2C), ops. DS18B20 + MAX31865 (PT1000). Veri **ESP-NOW**
(CH6, AES-128-GCM) ile XIAO C3 gateway'e → oradan UART ile Pi'ye gider.

**Bilinen donanım sorunu (brownout / E BOD):** Pil ile derin-uyku sonrası uyanışta
buck burst/PFM + 200mA LDO + Schottky düşümü akım darbesinde 3.3V rayı çökertiyor.
Firmware doğru. Çözüm donanımsal: buck+LDO make-before-break, `BUCK_ON_IN_DEEP_SLEEP`,
bulk kondansatör; kalıcı çözüm ideal-diyot / forced-PWM (R0.2).

## Repo yapısı

| Yol | Açıklama |
|---|---|
| `TEMP_LOGGER_BASE.ino` | Ana/çalışan sketch (tam sürüm giriş noktası) |
| `config.h` | Tam sürüm konfigürasyonu (~600 satır) |
| `EPD.*`, `EPD_SPI.h` | SSD1680 e-paper sürücüsü |
| `TH09C.*` | TH09C sıcaklık/nem sürücüsü |
| `Boardoza_MAX31865.*` | PT1000 sürücüsü |
| `TEMP_LOGGER_LEAN/` | **LEAN firmware** — hafif türev (uyan→ölç→gönder→sakla→uyu). Kendi `README_LEAN.md`'si var |

LEAN, ESP-NOW/AES-GCM için gateway ile ortak `EspNowCrypto.*` kütüphanesini vendor'lar;
protokol kontratı gateway repo'su `Faydam_GTW202_ESPGTW` ile birebir eşlenmiştir.

## Derleme ayarları (Arduino IDE / arduino-cli)

- Kart: **ESP32-C6**, Arduino core **3.x** (IDF5 — ESP-NOW recv API)
- Flash **40MHz / DIO**, CPU **80MHz**, **Erase Flash: Disabled**
- Kütüphaneler: `ArduinoJson`; EPD için Waveshare `GUI_Paint.h` + `fonts.h`

## Git akışı

- Geliştirme branch'i: `claude/esp32-c6-brownout-fix-dvon4z` (base'den ayrı push etme)
- Commit'ler açıklayıcı; PR yalnızca kullanıcı isterse.

```bash
# Branch oluştur/geç
git checkout -B claude/<branch-adi>

# Değişiklikleri push et (upstream ayarıyla)
git push -u origin claude/<branch-adi>
```

## Repo upload komutları (yeni repo oluştur + ilk push)

Yerelde yeni bir repo başlatıp GitHub'a yüklemek için:

```bash
# 1) Yeni repo klasörü oluştur ve içine gir
mkdir <yeni-repo-adi> && cd <yeni-repo-adi>

# 2) Git başlat + ilk commit
git init
git add .
git commit -m "İlk commit"

# 3) GitHub'da repo oluştur ve uzak bağla + push (gh CLI ile)
gh repo create Rippergithub/<yeni-repo-adi> --private --source=. --remote=origin --push

# gh yoksa: önce GitHub web'de repo aç, sonra:
git remote add origin https://github.com/Rippergithub/<yeni-repo-adi>.git
git branch -M main
git push -u origin main
```

> Not: Bu remote oturumda `gh`/GitHub API yerine **GitHub MCP araçları**
> (`mcp__github__create_repository`, `mcp__github__push_files`) kullanılır.
> Oturuma yeni bir repo eklemek için `add_repo` (owner/repo) ile ekle, sonra klonla.

## Yeni sketch klasörü (LEAN gibi ayrı firmware) oluşturma

Arduino, `.ino` dosyasının bulunduğu klasörle **aynı ada** sahip olmasını ister ve
o klasördeki tüm `.cpp/.h`'leri birlikte derler. Yeni bir bağımsız firmware için:

```bash
# 1) Klasör aç (klasör adı = .ino adı)
mkdir TEMP_LOGGER_<VARYANT>

# 2) Ortak sürücüleri kopyala (paylaşımlı derlenir)
cp EPD.cpp EPD.h EPD_SPI.h TH09C.cpp TH09C.h Boardoza_MAX31865.cpp Boardoza_MAX31865.h TEMP_LOGGER_<VARYANT>/

# 3) Ana sketch'i oluştur
:> TEMP_LOGGER_<VARYANT>/TEMP_LOGGER_<VARYANT>.ino
# (EPD.cpp ve TH09C.cpp `config.h` include eder → o klasörde bir config.h bulunmalı)
```

## arduino-cli ile komut satırından derleme / upload

```bash
# 0) Kurulum (bir kez)
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32          # Arduino core 3.x (ESP32-C6)

# Kütüphaneler
arduino-cli lib install ArduinoJson
# Waveshare GUI_Paint + fonts: kütüphane yöneticisinde yok → elle
#   ~/Arduino/libraries/ altına kopyalanır (EPD kullanılıyorsa gerekli)

# 1) FQBN — derleme ayarlarıyla (Flash 40MHz/DIO, CPU 80MHz, Erase Disabled)
FQBN="esp32:esp32:esp32c6:FlashFreq=40,FlashMode=dio,CPUFreq=80,EraseFlash=none"

# 2) Derle (LEAN sketch)
arduino-cli compile --fqbn "$FQBN" TEMP_LOGGER_LEAN

# 3) Upload (portu kendine göre ayarla: ls /dev/ttyACM* /dev/ttyUSB*)
arduino-cli upload -p /dev/ttyACM0 --fqbn "$FQBN" TEMP_LOGGER_LEAN

# 4) Seri monitör (log takibi)
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

> Tam sürüm için sketch adını `TEMP_LOGGER_BASE` yap. `--verbose` derleme
> hatalarını ayrıntılandırır; `--clean` önbelleği temizler.

## Belgeler

- `TEMP_LOGGER_LEAN/README_LEAN.md` — LEAN mimarisi, buffer/kripto kontratı
- Gateway/protokol referansı: `Faydam_GTW202_ESPGTW` (espnow_protocol.h, SECURITY.md, docs/GATEWAY_SPEC.md)
