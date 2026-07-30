#include "lean_display.h"
#include "config.h"

#if ENABLE_EPD
#include <SPI.h>
#include "EPD.h"
#include "GUI_Paint.h"   // Waveshare — libraries klasorunde olmali
#include "fonts.h"

static unsigned char BlackImage[EPD_ARRAY];

void display_show(const SensorRecord& rec, uint16_t pending, uint8_t batt_perc, bool full) {
  char line[24];

  digitalWrite(EPD_CS, LOW);            // SPI CS EPD'ye
  digitalWrite(MAX_CS, HIGH);

  EPD_Init_Custom(!full);               // full=false -> partial init
  Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 270, white);
  Paint_SelectImage(BlackImage);
  Paint_Clear(white);

  // Baslik
  Paint_DrawString_EN(5, 8, "FAYDAM  LEAN", &Font16, black, white);
  Paint_DrawLine(0, 30, 250, 30, black, LINE_STYLE_SOLID, DOT_PIXEL_1X1);

  // Sicaklik
  snprintf(line, sizeof(line), "T: %.1f C", rec.temp);
  Paint_DrawString_EN(5, 40, line, &Font24, black, white);

  // Nem
  snprintf(line, sizeof(line), "H: %.1f %%", rec.hum);
  Paint_DrawString_EN(5, 70, line, &Font16, black, white);

  // Alt bilgi: pil + bekleyen kayit
  snprintf(line, sizeof(line), "Bat:%u%%  Buf:%u", batt_perc, pending);
  Paint_DrawString_EN(5, 100, line, &Font12, black, white);

  PIC_display(BlackImage, !full);       // full=false -> partial refresh
  EPD_DeepSleep();

  digitalWrite(EPD_CS, HIGH);
  DEBUG_PRINTLN("[EPD] guncellendi");
}

#else  // ENABLE_EPD == false

void display_show(const SensorRecord&, uint16_t, uint8_t, bool) {
  DEBUG_PRINTLN("[EPD] devre disi (ENABLE_EPD=false)");
}

#endif
