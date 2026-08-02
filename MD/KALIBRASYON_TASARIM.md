# Çok Noktalı Kalibrasyon + Vade Takibi — Tasarım Dokümanı

**Kapsam:** Sensör (LEAN + tam firmware), Gateway (ESPGTW), PC/Donanım Merkezi.
**Amaç:** Girilen referans değerler ile cihaz okumaları arasındaki sapmadan
düzeltme katsayıları üretmek (örn. 5 noktalı), "kalibre et" komutuyla tetiklemek,
ve kalibrasyon **vadesini** izleyip ekranda/uyarıda göstermek.

> Bu doküman bir **kontrat/spec**'tir; her repo kendi tarafını buna göre uygular.

---

## 1. Kalibrasyon modelleri

Girdi: N adet `(ref_i, raw_i)` çifti — `ref` = referans/etalon değeri (°C),
`raw` = cihazın o noktada okuduğu ham değer.

| Model | Nokta | Düzeltme | Kullanım |
|---|---|---|---|
| **Offset (1-nokta)** | 1 | `corr = raw + b`, `b = ref - raw` | Basit; mevcut `cal_off` |
| **Linear (2+ nokta)** | ≥2 | `corr = a·raw + b` (en-küçük-kareler) | Kazanç+offset hatası; **önerilen varsayılan** |
| **Piecewise (N-nokta)** | ≥3 | Ardışık noktalar arası doğrusal enterpolasyon (LUT) | Doğrusal olmayan sensör; 5-nokta ideal |

### 1.1 Linear regresyon (least squares)
```
a = (N·Σ(raw·ref) − Σraw·Σref) / (N·Σ(raw²) − (Σraw)²)
b = (Σref − a·Σraw) / N
corr(x) = a·x + b
```
- `a` (gain) ≈ 1.0, `b` (offset) ≈ 0.0 beklenir.
- Kalite metriği: `R²` ve max artık (residual). `R² < 0.99` veya `|residual| > eşik` → kalibrasyon REDdedilir (hatalı giriş koruması).

### 1.2 Piecewise (5-nokta LUT)
- Noktalar `ref`e göre sıralanır. Ölçüm `x` için:
  - `x <= raw_0` → ilk segment eğimiyle ekstrapolasyon (veya clamp)
  - `raw_k <= x <= raw_{k+1}` → `corr = ref_k + (ref_{k+1}-ref_k)·(x-raw_k)/(raw_{k+1}-raw_k)`
  - `x >= raw_{N-1}` → son segment
- Avantaj: doğrusal olmayan davranışı yakalar. Dezavantaj: LUT saklama.

---

## 2. Nerede hesaplanır? (mimari karar)

**Öneri: PC HESAPLAR, SENSÖR UYGULAR.**
- Kullanıcı Donanım Merkezi'nde 5 `(ref, raw)` çiftini girer.
- **PC**, model katsayılarını (`a`, `b` veya LUT) + kalite metriklerini hesaplar,
  kalite yeterliyse sensöre **gönderir**.
- **Sensör** yalnızca katsayıları NVS'e yazar ve ölçümde uygular → LEAN hafif kalır,
  ağır matematik/fit gateway/sensöre yüklenmez.

> Alternatif (sensör-hesaplar): "kalibre et" komutu op_mode=1 (kalibrasyon) ile
> sensörü referans noktalarında ham okuma toplamaya alır, sonra kendi fit'ler.
> Daha karmaşık; LEAN için önerilmez. Bu doküman **PC-hesaplar** akışını standartlar.

---

## 3. Veri modeli (sensör NVS: namespace `cal_data`)

| Alan | Tip | Açıklama |
|---|---|---|
| `cal_model` | uint8 | 0=offset, 1=linear, 2=piecewise |
| `cal_gain` (a) | float | linear kazanç (model≥1) |
| `cal_off`  (b) | float | offset (model 0/1) — **mevcut alanla uyumlu** |
| `cal_n` | uint8 | LUT nokta sayısı (model 2) |
| `cal_raw[N]` | float[] | LUT ham noktalar (sıralı) |
| `cal_ref[N]` | float[] | LUT referans noktalar |
| `cal_ts` | uint32 | kalibrasyon tarihi (unix) — **mevcut** |
| `cal_valid_days` | uint16 | geçerlilik süresi (gün), örn. 180 |
| `cal_cert` | str | sertifika/etiket ("CAL-2026-001") |
| `cal_by` | str | operatör kimliği (ALCOA+ Attributable) |
| `cal_r2` | float | fit kalitesi (denetim izi) |

`cal_expiry = cal_ts + cal_valid_days·86400`.

---

## 4. Protokol eklemeleri (kontrat)

### 4.1 "Kalibre et" / katsayı gönderimi — PC → Gateway → Sensör
OTA gibi **JSON forward** (mac çıkarılıp AES-GCM ile sensöre iletilir):

```json
{
  "cmd": "cal_set",
  "model": 1,
  "gain": 1.002,
  "off": -0.35,
  "n": 0,
  "raw": [], "ref": [],
  "ts": 1785600000,
  "valid_days": 180,
  "cert": "CAL-2026-001",
  "by": "OP-ADMIN",
  "r2": 0.9997
}
```
- `model=2` (piecewise) ise `n`, `raw[]`, `ref[]` doldurulur; `gain/off` yok sayılır.
- Sensör: doğrula (n≤MAX, r2≥0.99 vb.) → NVS'e yaz → ACK/BC ile teyit.

Piecewise LUT büyükse (5 nokta × 2 float = 40B + JSON) tek pakette sığar (<250B enc);
daha fazla nokta gerekirse OTA'daki 0xFE chunk mekanizması kullanılır.

### 4.2 Ham okuma toplama (opsiyonel, sensör-hesaplar modu)
- `{"cmd":"cal_point","idx":k,"ref":25.00}` → sensör o an ham okur, `cal_raw[k]` saklar, ham değeri ACK ile PC'ye döner.
- 5 nokta toplanınca PC `cal_set` ile katsayıları geri yazar. (Toplama + hesap ayrımı.)

### 4.3 ACK ile mevcut alan uyumu
- Bugün ACK `settings.cal_off` + `cal_ts` gönderiyor (tek-nokta). Bu spec onu genişletir:
  `settings` içine opsiyonel `cal_gain`, `cal_model`, `cal_valid_days` eklenebilir
  (geriye uyumlu: yoksa offset modu).

### 4.4 special_cmd katalogu (öneri)
| Değer | Anlam |
|---|---|
| 0xCA | Kalibrasyon modu başlat (op_mode=1) / cal_point toplama tetikle |
| (mevcut 0xBC/0xCC/0xFF/0xAD) | değişmez |

---

## 5. Ölçümde uygulama (sensör)

```c
float apply_cal(float raw) {
  switch (cal_model) {
    case 0: return raw + cal_off;               // offset
    case 1: return cal_gain * raw + cal_off;    // linear
    case 2: return piecewise_interp(raw);       // LUT
  }
}
```
- Sıcaklık/nem/PT1000 için ayrı katsayı setleri tutulabilir (`cal_data_t`, `cal_data_h`...).
- LEAN'de şu an sadece `cal_off` uygulanıyor (`rec.temp += g_cal_off`); bu spec
  `cal_gain` ve LUT'u ekler.

---

## 6. Vade takibi + ekran

- Her cycle: `now = g_last_unix`; `remaining = cal_expiry − now`.
- Durumlar:
  - `remaining > CAL_WARN_DAYS·86400` → normal
  - `0 < remaining ≤ CAL_WARN_DAYS` → **EPD footer/köşe "KAL: <tarih>" uyarısı** (mevcut alt-bar dönüşümünde "KAL" slotu)
  - `remaining ≤ 0` → **"KALIBRASYON VADESI DOLDU"** — footer kırmızı/uyarı; opsiyonel ölçüm "UNCAL" bayrağı (status param 98 bit)
- Tam firmware'deki `epd_show_service_mode` mantığı referans.

---

## 7. ALCOA+ / denetim notları
- Kalibrasyon olayı **audit** kaydı olmalı: `who (by)`, `when (ts)`, `what (model, katsayılar, r2)`, `cert`.
- Katsayılar NVS'te; değişiklik BC/ACK ile Pi'ye raporlanır (tam firmware `cal_cert` BC alanı).
- Downgrade/yanlış-giriş koruması: `r2` ve residual eşiği; sağlanmazsa REDdet + logla.

---

## 8. Repo bazında yapılacaklar (özet)

| Repo | Yapılacak |
|---|---|
| **PC (faydam_ai_ready / pc_ai_ready-main)** | Donanım merkezinde 5-nokta giriş formu; least-squares/piecewise fit + R²; `cal_set` JSON'u gateway'e serial ile gönder; sonucu göster/logla |
| **Gateway (ESPGTW)** | `cal_set`/`cal_point` komutlarını sensöre JSON forward (OTA gibi); ACK `settings`'e `cal_gain/cal_model/cal_valid_days` ekle; per-MAC cal state |
| **Sensör (LEAN)** | `cal_model/cal_gain/LUT/valid_days/cert` NVS; `apply_cal()`; `cal_set` JSON handler; vade takibi + EPD uyarısı; BC'ye `cal_cert`+`cal_ts` |

---

## 9. LEAN'e minimal entegrasyon (öneri sırası)
1. `cal_gain` + `cal_model` (offset/linear) — küçük ek, en çok fayda.
2. Vade takibi + EPD "KAL" uyarısı (footer slotu zaten var).
3. `cal_set` JSON komutu (PC'den katsayı yükleme).
4. (Opsiyonel) piecewise LUT (5-nokta) — doğrusal olmayan problarda.

> Not: 1-2 adımları LEAN'i az büyütür; piecewise sadece gerekirse.
