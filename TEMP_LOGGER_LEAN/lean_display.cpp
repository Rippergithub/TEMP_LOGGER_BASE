#include "lean_display.h"
#include "config.h"

#if ENABLE_EPD
#include <SPI.h>
#include <WiFi.h>
#include <string.h>
#include "EPD.h"
#include "GUI_Paint.h"
#include "Fonts.h"

// Faydam_GTW202_TEMP DisplayManager duzeni birebir korunmustur (yonetici
// bagimliliklari olmadan LEAN icin sadelestirildi): 3px cerceve + header bar
// (MAC + sinyal + pil) + sol sicaklik/nem cercevesi + sag "SON DATA" paneli +
// footer bar. 250x122, rotation 270. Renkler GUI_Paint (WHITE0/BLACK0).

static unsigned char BlackImage[EPD_ARRAY];

// DisplayManager.cpp::setActiveSPI ile ayni davranis.
static void setActiveSPI(int activePin) {
  digitalWrite(EPD_CS, HIGH);
#if ENABLE_MAX31865
  digitalWrite(MAX_CS, HIGH);
#endif
  if (activePin != -1) digitalWrite(activePin, LOW);
}

void display_show(const SensorRecord& rec, uint16_t pending, uint8_t batt_perc,
                  bool full, int32_t tz_off, uint32_t boot_count, bool sent,
                  uint32_t last_payload_ts) {
  char buf[24];
  bool probe_ok = (rec.status == S_STATUS_OK || rec.status == S_STATUS_OUT_OF_RANGE);
  bool alarm    = (rec.flags & 0x01) != 0;

  // EPD pin init (Faydam_GTW202_TEMP initDisplay ile birebir).
  // KRITIK: EPD_RES/EPD_DC OUTPUT olmali; RES boşta kalirsa panel resetlenmez.
  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, EPD_CS);
  pinMode(EPD_CS, OUTPUT);  digitalWrite(EPD_CS, HIGH);
  pinMode(EPD_BUSY, INPUT);
  pinMode(EPD_RES, OUTPUT); digitalWrite(EPD_RES, HIGH);  // RST boşta kalmasin
  pinMode(EPD_DC, OUTPUT);

  setActiveSPI(-1);
  delay(20);
  digitalWrite(REG_CTL, HIGH);
  digitalWrite(LDO_CTL, HIGH);
  setActiveSPI(EPD_CS);

  EPD_Init_Custom(!full);
  Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 270, WHITE0);
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE0);

  // --- BOLD GLOBAL FRAME (3px) ---
  for (int f = 0; f < 3; f++)
    Paint_DrawRectangle(f, f, 249 - f, 121 - f, BLACK0, DRAW_FILL_EMPTY, DOT_PIXEL_1X1);

  // --- 1. HEADER BAR ---
  Paint_DrawRectangle(0, 3, 249, 18, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
  // MAC (sol)
  String mac = WiFi.macAddress();
  String cleanMac = "";
  for (uint16_t i = 0; i < mac.length(); i++) if (mac[i] != ':') cleanMac += mac[i];
  Paint_DrawString_EN(5, 3, cleanMac.c_str(), &Font16, BLACK0, WHITE0);

  // Sinyal cubuklari (gateway RSSI yok -> buffer varsa X, yoksa dolu)
  int signalX = 195;
  if (pending > 2) {
    Paint_DrawLine(signalX, 3, signalX + 16, 13, WHITE0, LINE_STYLE_SOLID, DOT_PIXEL_2X2);
    Paint_DrawLine(signalX, 13, signalX + 16, 3, WHITE0, LINE_STYLE_SOLID, DOT_PIXEL_2X2);
  } else {
    for (int b = 0; b < 5; b++) {
      int barH = (b + 1) * 2 + 1;
      Paint_DrawRectangle(signalX + (b * 4), 14 - barH, signalX + (b * 4) + 2, 14, WHITE0,
                          DRAW_FILL_FULL, DOT_PIXEL_1X1);
    }
  }

  // Pil ikonu (sag)
  int batX = 217, batY = 3;
  Paint_DrawRectangle(batX, batY, batX + 22, batY + 11, WHITE0, DRAW_FILL_EMPTY, DOT_PIXEL_1X1);
  Paint_DrawRectangle(batX + 22, batY + 4, batX + 24, batY + 8, WHITE0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
  int bars = (batt_perc > 80) ? 4 : (batt_perc > 50) ? 3 : (batt_perc > 20) ? 2 : (batt_perc > 5) ? 1 : 0;
  for (int j = 0; j < bars; j++)
    Paint_DrawRectangle(batX + 2 + (j * 5), batY + 2, batX + 2 + (j * 5) + 3, batY + 9, WHITE0,
                        DRAW_FILL_FULL, DOT_PIXEL_1X1);

  // --- 2. SOL SICAKLIK/NEM CERCEVESI (5,21)-(140,98) ---
  Paint_DrawRectangle(5, 21, 140, 98, BLACK0, DRAW_FILL_EMPTY, DOT_PIXEL_3X3);

  // Sicaklik "XX.X °C" (Font20). Derece simgesi kucuk daire ile cizilir
  // (referans DisplayManager ile ayni: sayidan sonra, C'den once).
  char tprefix[12];
  if (probe_ok) snprintf(tprefix, sizeof(tprefix), "%.1f", rec.temp);
  else          snprintf(tprefix, sizeof(tprefix), "--.-");
  snprintf(buf, sizeof(buf), "%s  C", tprefix);   // sayi + bosluk(derece yeri) + C
  int tW = strlen(buf) * 12;                       // Font20 width=12
  int tX = 5 + (137 - tW) / 2, tY = 35;
  Paint_DrawString_EN(tX, tY, buf, &Font20, WHITE0, BLACK0);
  // Derece dairesi: sayidan hemen sonra (bosluk konumu)
  Paint_DrawCircle(tX + (int)strlen(tprefix) * 12 + 6, tY + 3, 2, BLACK0, DRAW_FILL_EMPTY, DOT_PIXEL_1X1);

  // Nem "XX% RH" (Font20)
  if (probe_ok) snprintf(buf, sizeof(buf), "%d%% RH", (int)rec.hum);
  else          snprintf(buf, sizeof(buf), "--.-");
  int hW = strlen(buf) * 12;
  Paint_DrawString_EN(5 + (135 - hW) / 2, 68, buf, &Font20, WHITE0, BLACK0);

  // OK durum dairesi (129,34)
  if (probe_ok) {
    const int cX = 129, cY = 34, r = 8;
    Paint_DrawCircle(cX, cY, r, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
    Paint_DrawLine(cX - 4, cY + 1, cX - 1, cY + 4, WHITE0, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
    Paint_DrawLine(cX - 1, cY + 4, cX + 5, cY - 2, WHITE0, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
  }

  // --- 3. SAG PANEL (referans DisplayManager::epd_update_data mantigi) ---
  //  last_payload_ts (getLastPayloadUnix) gecerli  -> "SON KAYIT" + o zaman
  //  degilse: gonderildi ya da buffer bos           -> "SON DATA"  + guncel zaman
  //  degilse (gonderilemedi + buffer dolu)          -> "KAYIT (n)"
  bool hasLast = (last_payload_ts > 1000000000UL);
  const char* label;
  char labelBuf[16];
  if (alarm)        label = "! ALARM !";
  else if (!probe_ok) label = "PROB?";
  else if (hasLast) label = "SON KAYIT";
  else if (sent || pending == 0) label = "SON DATA";
  else { snprintf(labelBuf, sizeof(labelBuf), "KAYIT (%u)", pending); label = labelBuf; }
  Paint_DrawString_EN(145 + (100 - (int)(strlen(label) * 8)) / 2, 32, label, &Font16, WHITE0, BLACK0);

  // Gosterilecek zaman: SON KAYIT ise last_payload_ts, degilse guncel olcum zamani.
  // UTC epoch + tz_off (gateway 'off') -> gmtime_r ile yerel saat.
  uint32_t showTs = hasLast ? last_payload_ts
                            : (rec.timestamp > 1000000000UL ? rec.timestamp : 0);
  char tb[8], db[12];
  if (showTs > 1000000000UL) {
    struct tm ti; time_t t = (time_t)((int64_t)showTs + tz_off); gmtime_r(&t, &ti);
    strftime(tb, sizeof(tb), "%H:%M", &ti);
    strftime(db, sizeof(db), "%d.%m.%Y", &ti);
  } else { strcpy(tb, "--:--"); strcpy(db, "--.--.----"); }
  Paint_DrawString_EN(145 + (100 - (int)(strlen(tb) * 16)) / 2, 48, tb, &Font24, WHITE0, BLACK0);
  Paint_DrawString_EN(145 + (100 - (int)(strlen(db) * 8)) / 2, 80, db, &Font16, WHITE0, BLACK0);

  // --- 4. FOOTER BAR (bilgi donusumlu, referans DisplayManager mantigi) ---
  // Oncelik: prob/pil/baglanti uyarilari; normalde HW/FW/UID bilgisi boot_count%3
  // ile donusumlu gosterilir.
  Paint_DrawRectangle(0, 103, 249, 120, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
  char fbuf[28];
  const char* footer;
  if (alarm)                 footer = "! ! ALARM ! !";
  else if (!probe_ok)        footer = "PROB KONTROL EDINIZ";
  else if (batt_perc <= 20)  footer = "BATARYA ZAYIF";
  else if (pending > 2)      footer = "BAGLANTI YOK";
  else {
    switch (boot_count % 3) {
      case 0:  snprintf(fbuf, sizeof(fbuf), "HW: %s", HARDWARE_MODEL); break;
      case 1:  snprintf(fbuf, sizeof(fbuf), "FW: %s", FW_VERSION);     break;
      default: // UID = MAC son 4 byte (cleanMac header'da hesaplandi)
        snprintf(fbuf, sizeof(fbuf), "UID:FYDM26%s", cleanMac.substring(4).c_str());
        break;
    }
    footer = fbuf;
  }
  Paint_DrawString_EN((250 - (int)(strlen(footer) * 8)) / 2, 105, footer, &Font16, BLACK0, WHITE0);

  PIC_display(BlackImage, !full);
  EPD_DeepSleep();
  setActiveSPI(-1);
  DEBUG_PRINTLN("[EPD] dashboard guncellendi");
}

#else  // ENABLE_EPD == false
void display_show(const SensorRecord&, uint16_t, uint8_t, bool, int32_t, uint32_t, bool, uint32_t) {
  DEBUG_PRINTLN("[EPD] devre disi (ENABLE_EPD=false)");
}
#endif
