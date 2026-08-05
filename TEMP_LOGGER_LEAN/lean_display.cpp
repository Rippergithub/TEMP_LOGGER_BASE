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
// (MAC + sinyal + pil) + sol sicaklik cercevesi (TH09C: Font20+RH; tek sensor:
// buyuk Font24) + sag "SON DATA" paneli + footer bar. 250x122, rotation 270.

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
                  uint32_t last_payload_ts, uint32_t alarm_start_ts,
                  uint8_t cal_state, uint32_t cal_expiry) {
  char buf[24];
  bool probe_ok = (rec.status == S_STATUS_OK || rec.status == S_STATUS_OUT_OF_RANGE);
  bool alarm    = (rec.flags & 0x01) != 0;
  bool doFull   = full;

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

  // GTW DisplayManager::epd_wait_for_busy_clear: deep sleep / RES float sonrasi
  // BUSY=HIGH ise 0x26 baseline kaybolmus → partial ghost veya "guncellenmedi".
  if (digitalRead(EPD_BUSY) == HIGH) {
    doFull = true;
    DEBUG_PRINTLN("[EPD] BUSY=HIGH at wake: forcing FULL (0x26 baseline lost)");
  }

  EPD_Init_Custom(!doFull);
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

  // --- 2. SOL SICAKLIK CERCEVESI (5,21)-(140,98) ---
  // DisplayManager: TH09C → Font20 temp+RH; tek sensor (DS18/MAX) → buyuk Font24, nem yok.
  Paint_DrawRectangle(5, 21, 140, 98, BLACK0, DRAW_FILL_EMPTY, DOT_PIXEL_3X3);

#if ENABLE_TH09C
  // --- HIBRID (Temp + Hum, Font20) ---
  {
    char tprefix[12];
    if (probe_ok) snprintf(tprefix, sizeof(tprefix), "%.1f", rec.temp);
    else          snprintf(tprefix, sizeof(tprefix), "--.-");
    snprintf(buf, sizeof(buf), "%s  C", tprefix);
    int tempY = alarm ? 27 : 35;
    int humY  = alarm ? 54 : 68;
    int tW = (int)strlen(buf) * 12;
    int tX = 5 + (137 - tW) / 2;
    Paint_DrawString_EN(tX, tempY, buf, &Font20, WHITE0, BLACK0);
    if (probe_ok) {
      Paint_DrawCircle(tX + (int)strlen(tprefix) * 12 + 3, tempY + 3, 2, BLACK0,
                       DRAW_FILL_EMPTY, DOT_PIXEL_2X2);
    }
    if (probe_ok) snprintf(buf, sizeof(buf), "%d%% RH", (int)rec.hum);
    else          snprintf(buf, sizeof(buf), "--.-");
    int hW = (int)strlen(buf) * 12;
    Paint_DrawString_EN(5 + (135 - hW) / 2, humY, buf, &Font20, WHITE0, BLACK0);
  }
#else
  // --- TEK SENSOR (sicaklik only): buyuk Font24, cift cizim (bold), nem yok ---
  {
    int tempY = alarm ? 50 : 48;
    if (!probe_ok) {
      // "--.-" kalin cizgi — font tire e-paper'da cok ince (DisplayManager ile ayni)
      int cx = 5 + (137 / 2) - 4;
      int ly = tempY + 10;
      Paint_DrawLine(cx - 42, ly, cx - 26, ly, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_3X3);
      Paint_DrawLine(cx - 20, ly, cx - 4,  ly, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_3X3);
      Paint_DrawCircle(cx + 2, ly + 1, 3, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
      Paint_DrawLine(cx + 8, ly, cx + 24, ly, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_3X3);
    } else {
      char tprefix[12];
      snprintf(tprefix, sizeof(tprefix), "%.1f", rec.temp);
      int totalWidth = ((int)strlen(tprefix) + 2) * 16;  // Font24 width = 16
      int startX = 5 + (137 - totalWidth) / 2 - 4;
      Paint_DrawString_EN(startX, tempY, tprefix, &Font24, WHITE0, BLACK0);
      Paint_DrawString_EN(startX + 1, tempY, tprefix, &Font24, WHITE0, BLACK0);
      int nextX = startX + (int)strlen(tprefix) * 16 + 16;
      Paint_DrawString_EN(nextX, tempY, "C", &Font24, WHITE0, BLACK0);
      Paint_DrawString_EN(nextX + 1, tempY, "C", &Font24, WHITE0, BLACK0);
      Paint_DrawCircle(nextX - 8, tempY + 2, 2, BLACK0, DRAW_FILL_EMPTY, DOT_PIXEL_2X2);
    }
  }
#endif

  // OK durum dairesi (129,34) — DisplayManager ile ayni
  if (probe_ok) {
    const int cX = 129, cY = 34, r = 8;
    Paint_DrawCircle(cX, cY, r, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
    Paint_DrawLine(cX - 4, cY + 1, cX - 1, cY + 4, WHITE0, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
    Paint_DrawLine(cX - 1, cY + 4, cX + 5, cY - 2, WHITE0, LINE_STYLE_SOLID, DOT_PIXEL_1X1);
  }

  // Kalibrasyon uyarisi "K" dairesi (111,34)
  if (cal_state >= 1) {
    const int kX = 111, kY = 34, r = 8;
    Paint_DrawCircle(kX, kY, r, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
    Paint_DrawString_EN(kX - 3, kY - 7, "K", &Font16, BLACK0, WHITE0);
  }

  // --- 3. SAG PANEL (x=145..245) ---
  if (alarm) {
    // Alarm: buyuk UNLEM (ucgen + "!") — saat/etiket yerine dikkat cekici ikon.
    const int tx = 195, ty = 26, hw = 24, by = 70;   // tepe (tx,ty), taban y=by
    Paint_DrawLine(tx, ty, tx - hw, by, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_2X2);
    Paint_DrawLine(tx, ty, tx + hw, by, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_2X2);
    Paint_DrawLine(tx - hw, by, tx + hw, by, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_2X2);
    Paint_DrawLine(tx, ty + 14, tx, by - 14, BLACK0, LINE_STYLE_SOLID, DOT_PIXEL_3X3);      // "!" govde
    Paint_DrawRectangle(tx - 1, by - 10, tx + 2, by - 7, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_2X2); // "!" nokta
    Paint_DrawString_EN(145 + (100 - (int)(9 * 8)) / 2, 30, "! ALARM !", &Font16, WHITE0, BLACK0);
  } else {
    // last_payload_ts = son basariyla teslim; rec.timestamp = bu tur olcum.
    // Saat: bu turun olcumunu onceliklendir (eski last_payload yuzunden bir tur
    // geride kalmasin). Yoksa son teslim / gosterilemez.
    bool hasLast = (last_payload_ts > 1000000000UL);
    bool hasRec  = (rec.timestamp > 1000000000UL);
    char labelBuf[16];
    const char* label;
    if (!probe_ok)            label = "PROB?";
    else if (pending > 0)   { snprintf(labelBuf, sizeof(labelBuf), "KAYIT (%u)", pending); label = labelBuf; }
    else if (sent)            label = "SON DATA";
    else if (hasLast)         label = "SON KAYIT";
    else                      label = "SON DATA";
    Paint_DrawString_EN(145 + (100 - (int)(strlen(label) * 8)) / 2, 32, label, &Font16, WHITE0, BLACK0);

    // showTs: bu tur olcum > son teslim (sent parametresi artik etikette kullaniliyor)
    uint32_t showTs = hasRec ? rec.timestamp : (hasLast ? last_payload_ts : 0);
    char tb[8], db[12];
    if (showTs > 1000000000UL) {
      struct tm ti; time_t t = (time_t)((int64_t)showTs + tz_off); gmtime_r(&t, &ti);
      strftime(tb, sizeof(tb), "%H:%M", &ti);
      strftime(db, sizeof(db), "%d.%m.%Y", &ti);
    } else { strcpy(tb, "--:--"); strcpy(db, "--.--.----"); }
    Paint_DrawString_EN(145 + (100 - (int)(strlen(tb) * 16)) / 2, 48, tb, &Font24, WHITE0, BLACK0);
    Paint_DrawString_EN(145 + (100 - (int)(strlen(db) * 8)) / 2, 80, db, &Font16, WHITE0, BLACK0);
  }

  // --- 4. FOOTER BAR (bilgi donusumlu, referans DisplayManager mantigi) ---
  // Oncelik: prob/pil/baglanti uyarilari; normalde HW/FW/UID bilgisi boot_count%3
  // ile donusumlu gosterilir.
  Paint_DrawRectangle(0, 103, 249, 120, BLACK0, DRAW_FILL_FULL, DOT_PIXEL_1X1);
  char fbuf[28];
  const char* footer;
  if (alarm) {
    // Alarm footer: baslangic zamani "BASLANGIC: dd/mm/yy HH:MM" (referans ile ayni)
    if (alarm_start_ts > 1000000000UL) {
      struct tm ai; time_t at = (time_t)((int64_t)alarm_start_ts + tz_off); gmtime_r(&at, &ai);
      strftime(fbuf, sizeof(fbuf), "BASLANGIC: %d/%m/%y %H:%M", &ai);
    } else {
      snprintf(fbuf, sizeof(fbuf), "! ! ALARM ! !");
    }
    footer = fbuf;
  }
  else if (!probe_ok)        footer = "PROB KONTROL EDINIZ";
  else if (cal_state == 2)   footer = "KALIBRASYON VADESI DOLDU";
  else if (batt_perc <= 20)  footer = "BATARYA ZAYIF";
  else if (pending > 2)      footer = "BAGLANTI YOK";
  else {
    // KAL vade uyarisi varsa (state=1) bilgi slotunu KAL tarihine ayir.
    if (cal_state == 1 && cal_expiry > 1000000000UL) {
      struct tm ce; time_t ct = (time_t)((int64_t)cal_expiry + tz_off); gmtime_r(&ct, &ce);
      strftime(fbuf, sizeof(fbuf), "KAL: %d.%m.%Y", &ce);
      footer = fbuf;
    } else
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

  PIC_display(BlackImage, !doFull);

  // GTW: full → hemen DeepSleep; partial → panel bitince PowerManager DeepSleep eder.
  // LEAN tek update/boot: partial sonrasi da DeepSleep (piksel kilidi), ama once BUSY.
  if (doFull) {
    EPD_DeepSleep();
  } else {
    uint32_t t0 = millis();
    while (digitalRead(EPD_BUSY) == HIGH && (millis() - t0) < 500) delay(5);
    EPD_DeepSleep();
  }
  setActiveSPI(-1);
  DEBUG_PRINTLN(doFull ? "[EPD] dashboard guncellendi (FULL)" : "[EPD] dashboard guncellendi (PARTIAL)");
}

#else  // ENABLE_EPD == false
void display_show(const SensorRecord&, uint16_t, uint8_t, bool, int32_t, uint32_t, bool, uint32_t, uint32_t, uint8_t, uint32_t) {
  DEBUG_PRINTLN("[EPD] devre disi (ENABLE_EPD=false)");
}
#endif
