# CLAUDE.md — TEMP_LOGGER_LEAN (hafif firmware)

Bu klasör bağımsız bir Arduino sketch'idir (uyan→ölç→gönder→sakla→uyu).
Mimari/kripto/buffer ayrıntıları için `README_LEAN.md`.

> Tüm komutlar **repo kökünden** (`TEMP_LOGGER_BASE/`) çalıştırılır; arduino-cli
> sketch adını klasör yoluyla alır. Branch: `claude/esp32-c6-brownout-fix-dvon4z`.

## ⚠️ KOŞUL: Sürüm + değişiklik günlüğü (her değişiklikte zorunlu)

**Her kod değişikliğinden sonra**, commit'ten ÖNCE:
1. `TEMP_LOGGER_LEAN.ino` başındaki **DEGISIKLIK GUNLUGU** bloğuna en üste yeni satır ekle.
2. Anlamlı değişikliklerde **SURUM**'u artır (semver: LMAJOR.MINOR.PATCH).
3. `config.h → FW_VERSION`'ı `.ino` başlığındaki sürümle **aynı** yap (ikisi senkron).

> Sürüm şeması: `L<major>.<minor>.<patch>` — küçük düzeltme=patch, yeni özellik=minor.

## Her değişiklik sonrası çalıştırılacak terminal komutları

```bash
# 1) Değişeni gör
git status --short
git diff

# 2) Stage + commit
git add TEMP_LOGGER_LEAN/        # veya değişen tek dosya
git commit -m "<ne değişti kısa açıklama>"

# 3) Push (ağ hatasında 2s,4s,8s,16s backoff ile 4 deneme)
git push -u origin claude/esp32-c6-brownout-fix-dvon4z
```

Tek satır (commit + retry'li push):

```bash
git add -A && git commit -m "<mesaj>" && \
for i in 1 2 3 4; do git push -u origin claude/esp32-c6-brownout-fix-dvon4z && break || sleep $((2**i)); done
```

## Derleme / upload (arduino-cli)

```bash
FQBN="esp32:esp32:esp32c6:FlashFreq=40,FlashMode=dio,CPUFreq=80,EraseFlash=none"

arduino-cli compile --fqbn "$FQBN" TEMP_LOGGER_LEAN            # derle
arduino-cli upload  -p /dev/ttyACM0 --fqbn "$FQBN" TEMP_LOGGER_LEAN   # yükle
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200        # seri log
```

Bağımlılıklar: `ArduinoJson`. EPD kütüphanesi (`GUI_Paint.*`, `Fonts.*`, `EPD.*`,
`EPD_SPI.h`) klasörde vendor'lı — harici kurulum gerekmez. Kapatmak için
`config.h` → `ENABLE_EPD false`.

## Bu klasördeki dosyalar

| Dosya | İş |
|---|---|
| `TEMP_LOGGER_LEAN.ino` | Orkestrasyon (uyan→ölç→gönder→sakla→uyu) |
| `config.h` | Lean config (pin/CH6/buffer/sensör flag) |
| `lean_types.h` | `SensorRecord` + `SensorDataMessage` |
| `lean_store.*` | Tiered RTC/FLASH buffer |
| `lean_crypto.*` + `EspNowCrypto.*` | AES-128-GCM (gateway ile byte-exact) |
| `lean_espnow.*` | init / send+ACK / BC |
| `lean_display.*` + `GUI_Paint.* Fonts.* EPD.*` | EPD dashboard |
| `TH09C.* Boardoza_MAX31865.*` | sensör sürücüleri |
