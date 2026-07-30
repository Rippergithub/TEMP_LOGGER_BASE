# LEAN vs TEMP_LOGGER (tam firmware) — Eksik/Fark Listesi

Karşılaştırma: `TEMP_LOGGER_LEAN/` ↔ `Faydam_GTW202_TEMP` (tam sürüm, ~16.3K satır).
LEAN bilinçli olarak hafiftir; aşağıdaki liste "neyin olmadığını" ve önem derecesini gösterir.

## 🔴 İnterop açısından KRİTİK (gateway ile tam uyum için gerekebilir)

| Konu | Tam firmware | LEAN | Not |
|---|---|---|---|
| **Pairing / allowlist** | `PAIR_REQ/RESP/CONFIRM` mutual auth, MAC allowlist'e eklenir | ❌ yok (broadcast/fixed peer) | ⚠️ Gateway allowlist **zorunlu** ise, bilinmeyen MAC'ten gelen **binary** paket DROP edilir. LEAN'in MAC'i gateway'de allowlist'e elle eklenmeli **veya** PAIR_REQ akışı eklenmeli. (Allowlist boşsa sorun yok.) |
| **ACK `settings` uygulama** | `t_low/t_high/h_low/h_high`, `cal_off`, `cal_ts` ACK'ten alınır | ❌ sadece `sleep`/`unix`/`off`/`special_cmd` | Alarm eşikleri ve kalibrasyon offset'i gelmiyor |
| **Anti-replay boot_cnt kalıcılığı** | boot_cnt NVS'te kalıcı | ⚠️ `g_boot_count` RTC'de (güç kesilince sıfırlanır) | Güç tam kesilirse boot_cnt sıfırlanır → gateway ilk paketleri "stale" görebilir (grace period genelde kurtarır) |

## 🟡 Fonksiyonel eksik (isteğe bağlı — kullanım senaryona göre)

| Modül / özellik | Tam firmware | LEAN |
|---|---|---|
| **Alarm mantığı** | eşik aşımı + histerezis + footer "ALARM" + buzzer + snooze (`AlarmNotificationFlow`) | ❌ yok (footer'da statik) |
| **Smart sleep** | dinamik uyku aralığı (sıcaklık riskine göre) | ❌ sabit `sleep_sec` |
| **Kademeli güç** | düşük pil / limp mode / güvenli kapanış %'leri (`PowerManager`) | ⚠️ sadece %20 flash eşiği |
| **EMA damping / core sim** | kan torbası simülasyonu, T90 (`SensorManager`) | ❌ ham ölçüm |
| **Anomaly detection** | 3-sigma, max değişim hızı | ❌ yok |
| **Sensör sağlığı** | health score, pre-failure | ❌ yok |
| **Bulk data** | `MSG_BULK_DATA` (10 kayıt tek pakette) | ❌ tek tek gönderim |
| **Adaptive TX power** | RSSI'ye göre güç ayarı | ❌ sabit |
| **Ekstra EPD ekranları** | service/shutdown/safe-mode/bootsplash/QR/setup | ❌ sadece dashboard |
| **Çok dil** | TR/EN (`SettingsManager`) | ❌ TR sabit |

## 🟢 Bilinçli atıldı (LEAN felsefesi — geri almak istemezsin muhtemelen)

| Modül | İş | Neden atıldı |
|---|---|---|
| `NetworkManager` | WiFi/HTTP/TLS/NTP/cloud upload | LEAN sadece ESP-NOW |
| `WebConfigManager` | AP setup wizard, web UI provisioning | Saha provisioning yok |
| `StorageManager` (WORM/Vault/audit/hash-chain) | ALCOA+ kalıcı arşiv | LEAN'de RTC+LittleFS ring yeterli |
| `CalibrationManager` | kalibrasyon sertifika/vade | Metroloji katmanı |
| `IQ/OQ/PQProtocol` | ALCOA+ kalifikasyon | Regülasyon katmanı |
| `AuditDumpManager` | audit dump (`special_cmd 0xAD`) | Denetim katmanı |
| `AccessManager` / `SerialAuth` | rol tabanlı erişim, seri provisioning | Yerel yönetim |
| `Maintenance/Predictive/ai_impl` | kestirimci bakım / AI | Ağır analitik |
| `NetPhaseProfiler` | performans profili | Tanı aracı |

## 🔵 LEAN'de zaten karşılığı olanlar

| Tam firmware | LEAN karşılığı |
|---|---|
| `ESPNowManager` (send+ACK) | `lean_espnow` |
| `EspNowCrypto` (AES-GCM) | `EspNowCrypto` (birebir vendor) + `lean_crypto` |
| `DisplayManager` (dashboard) | `lean_display` (layout korundu) |
| `StorageManager` (offline buffer) | `lean_store` (RTC + LittleFS, 30+ gün) |
| OTA (`ESPNowManager` binary OTA) | `lean_ota` |
| `PowerManager` (rail + sleep) | `.ino` power_rail + go_to_sleep |
| `SensorManager` (TH09C okuma) | `.ino` measure() |
| BC (Birth Certificate) | `espnow_send_bc` (minimal) |

## Öneri (öncelik sırası)
1. **Pairing/allowlist** — gateway allowlist açıksa LEAN'in verisi düşer; ya MAC'i gateway'e ekle ya PAIR_REQ akışını LEAN'e ekleyelim.
2. **ACK settings** — alarm eşikleri + cal_off'u ACK'ten alıp uygula (küçük ek).
3. **Alarm mantığı** — eşik aşımında footer/flag (buzzer opsiyonel).
4. Diğerleri senaryona göre.
