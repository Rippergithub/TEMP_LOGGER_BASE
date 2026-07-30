# LEAN vs TEMP_LOGGER (tam firmware) — Eksik/Fark Listesi

Karşılaştırma: `TEMP_LOGGER_LEAN/` ↔ `Faydam_GTW202_TEMP` (tam sürüm, ~16.3K satır).
LEAN bilinçli olarak hafiftir; aşağıdaki liste "neyin olmadığını" ve önem derecesini gösterir.

> **Durum (L1.1.0):** 🔴 kritik maddelerin 2'si + 🟡'dan alarm **TAMAMLANDI** (aşağıda ✅).

## 🔴 İnterop açısından KRİTİK (gateway ile tam uyum için gerekebilir)

| Konu | Tam firmware | LEAN | Durum |
|---|---|---|---|
| **Pairing / allowlist** | `PAIR_REQ/RESP/CONFIRM` mutual auth | `espnow_pair()` PAIR_REQ (pmk_fpr+pid_h+pmk_ver), RTC `g_paired` | ✅ **EKLENDİ (L1.1.0)** — CONFIRM/mutual auth atlandı (gateway PAIR_REQ'te allowlist'liyor) |
| **ACK `settings` uygulama** | `t_low/t_high/h_low/h_high`, `cal_off`, `cal_ts` | `AckResult` + RTC'ye alınır, `cal_off` ölçüme uygulanır | ✅ **EKLENDİ (L1.1.0)** |
| **Anti-replay boot_cnt kalıcılığı** | boot_cnt NVS'te kalıcı | ⚠️ `g_boot_count` RTC'de (güç kesilince sıfırlanır) | ⬜ açık — grace period genelde kurtarır (istenirse NVS'e alınır) |

## 🟡 Fonksiyonel eksik (isteğe bağlı — kullanım senaryona göre)

| Modül / özellik | Tam firmware | LEAN |
|---|---|---|
| **Alarm mantığı** | eşik aşımı + histerezis + footer "ALARM" + buzzer + snooze (`AlarmNotificationFlow`) | ✅ **EKLENDİ (L1.1.0)** — eşik+histerezis, EPD `! ALARM !`/footer, status=OUT_OF_RANGE. (buzzer/snooze yok) |
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

## Öneri (öncelik sırası) — durum
1. ✅ **Pairing/allowlist** — EKLENDİ (L1.1.0)
2. ✅ **ACK settings** (eşikler + cal_off) — EKLENDİ (L1.1.0)
3. ✅ **Alarm mantığı** (eşik+histerezis, EPD) — EKLENDİ (L1.1.0)
4. ⬜ Kalanlar senaryona göre: smart sleep, kademeli güç (limp/shutdown), bulk data,
     boot_cnt NVS kalıcılığı, buzzer/snooze, çok dil, ekstra EPD ekranları.
