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

void display_show(const SensorRecord& rec, uint16_t pending, uint8_t batt_perc, bool full) {
  char buf[24];
  bool probe_ok = (rec.status == S_STATUS_OK || rec.status == S_STATUS_OUT_OF_RANGE);

  setActiveSPI(-1);
  delay(20);
  digitalWrite(REG_CTL, HIGH);
  digitalWrite(LDO_CTL, HIGH);
  pinMode(EPD_BUSY, INPUT);
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

  // Sicaklik "XX.X C" (Font20)
  if (probe_ok) snprintf(buf, sizeof(buf), "%.1f C", rec.temp);
  else          snprintf(buf, sizeof(buf), "--.- C");
  int tW = strlen(buf) * 12;                 // Font20 width=12
  Paint_DrawString_EN(5 + (137 - tW) / 2, 35, buf, &Font20, WHITE0, BLACK0);

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

  // --- 3. SAG PANEL: "SON DATA" + saat/buffer ---
  const char* label = probe_ok ? "SON DATA" : "PROB?";
  Paint_DrawString_EN(145 + (100 - (int)(strlen(label) * 8)) / 2, 32, label, &Font16, WHITE0, BLACK0);

  // Saat (zaman senkron varsa) yoksa buffer sayisi
  if (rec.timestamp > 1000000000UL) {
    struct tm ti; time_t t = (time_t)rec.timestamp; localtime_r(&t, &ti);
    char tb[8]; strftime(tb, sizeof(tb), "%H:%M", &ti);
    Paint_DrawString_EN(145 + (100 - (int)(strlen(tb) * 16)) / 2, 48, tb, &Font24, WHITE0, BLACK0);
    char db[12]; strftime(db, sizeof(db), "%d.%m.%Y", &ti);
    Paint_DrawString_EN(145 + (100 - (int)(strlen(db) * 8)) / 2, 80, db, &Font16, WHITE0, BLACK0);
  } else {
    snprintf(buf, sizeof(buf), "BUF:%u", pending);
    Paint_DrawString_EN(145 + (100 - (int)(strlen(buf) * 16)) / 2, 55, buf, &Font24, WHITE0, BLACK0);
  }

  // --- 4. FOOTER BAR ---
  Paint_DrawRectangle(0, 103, 249, 120, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
  const char* footer;
  if (!probe_ok)                    footer = "PROB KONTROL EDINIZ";
  else if (batt_perc <= 20)         footer = "BATARYA ZAYIF";
  else if (pending > 2)             footer = "BAGLANTI YOK";
  else                              footer = "FAYDAM LEAN";
  Paint_DrawString_EN((250 - (int)(strlen(footer) * 8)) / 2, 105, footer, &Font16, BLACK0, WHITE0);

  PIC_display(BlackImage, !full);
  EPD_DeepSleep();
  setActiveSPI(-1);
  DEBUG_PRINTLN("[EPD] dashboard guncellendi");
}

#else  // ENABLE_EPD == false
void display_show(const SensorRecord&, uint16_t, uint8_t, bool) {
  DEBUG_PRINTLN("[EPD] devre disi (ENABLE_EPD=false)");
}
#endif
