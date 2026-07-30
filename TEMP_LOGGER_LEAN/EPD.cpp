#include "EPD.h"
#include "config.h"
#include "EPD_SPI.h"
#include <Arduino.h>
#include <string.h>

// v5.7.1: 2-bit GUI → 1-bit EPD piksel dönüşüm tablosu.
// Her byte 4 adet 2-bit piksel içerir (GUI formatı).
// Çıkış: 4-bit (her bit = 1 EPD pikseli).
// GUI: BLACK0=0x03 → EPD: 0 (siyah), WHITE0=0x00 → EPD: 1 (beyaz)
// lut[byte] = (b7'den b0'a) her çift bitin terslenmiş yüksek biti
static uint8_t _pix_lut[256];
static bool _pix_lut_ready = false;

static void _build_pix_lut() {
  if (_pix_lut_ready) return;
  // Orijinal döngü mantığı — iç döngü yerine açık (unrolled) hesaplama.
  // Her byte 4 adet 2-bit piksel içerir: bit7:6, bit5:4, bit3:2, bit1:0
  // Piksel 0x03 → siyah (EPD bit=0), diğer → beyaz (EPD bit=1)
  for (int i = 0; i < 256; i++) {
    uint8_t out = 0;
    if (((i >> 6) & 0x03) != 0x03) out |= 0x08; // piksel 0 → bit3
    if (((i >> 4) & 0x03) != 0x03) out |= 0x04; // piksel 1 → bit2
    if (((i >> 2) & 0x03) != 0x03) out |= 0x02; // piksel 2 → bit1
    if (((i >> 0) & 0x03) != 0x03) out |= 0x01; // piksel 3 → bit0
    _pix_lut[i] = out;
  }
  _pix_lut_ready = true;
}

void delay_xms(unsigned int xms) { delay(xms); }

void lcd_chkstatus(void) {
  uint32_t timeout = millis();
  pinMode(EPD_W21_BUSY, INPUT); 
  while (digitalRead(EPD_W21_BUSY) == 1) { // SSD1680: 1 is Busy
    if (millis() - timeout > 10000) {
      EPD_DEBUG_PRINTLN("[EPD] Busy Timeout (10s)! Forcing Hardware Reset (M-12)");
      EPD_W21_RST_0; delay(20); EPD_W21_RST_1; delay(20);
      break;
    }
    delay(1);
  }
  uint32_t waited = millis() - timeout;
  if (waited > 5) {
    EPD_DEBUG_PRINT("[EPD] Busy waited: ");
    EPD_DEBUG_PRINT(waited);
    EPD_DEBUG_PRINTLN("ms");
  }
}

void EPD_Init(void) { EPD_Init_Custom(false); }

void EPD_Init_Custom(bool partial) {
  if (partial) {
    EPD_Init_E0213A373_Partial();
  } else {
    EPD_Init_E0213A373_Full();
  }
}

void EPD_Init_E0213A373_Full(void) {
  EPD_W21_RST_0;
  delay(50);
  EPD_W21_RST_1;
  delay(50);
  lcd_chkstatus();

  EPD_W21_WriteCMD(0x12); // SW Reset
  lcd_chkstatus();

  EPD_W21_WriteCMD(0x0C);  // Booster Soft Start
  EPD_W21_WriteDATA(0x8B); // Standard safe boost
  EPD_W21_WriteDATA(0x9C);
  EPD_W21_WriteDATA(0x96);
  EPD_W21_WriteDATA(0x0F);

  EPD_W21_WriteCMD(0x01);  // Driver output control
  EPD_W21_WriteDATA(0xF9); // 250-1
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0x11); // Data entry mode 3: Horizontal increment
  EPD_W21_WriteDATA(0x03);

  EPD_W21_WriteCMD(0x44); // RAM-X range
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x0F); // 16 bytes = 128 pixels

  EPD_W21_WriteCMD(0x45); // RAM-Y range
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0xF9);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0x3C);  // Border Waveform
  EPD_W21_WriteDATA(0x01); // Standardized white border

  EPD_W21_WriteCMD(0x2C);  // VCOM Voltage
  EPD_W21_WriteDATA(0x36); // Optimized for E0213A373

  EPD_W21_WriteCMD(0x21); // Display Update Control 1
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x80);

  EPD_W21_WriteCMD(0x18); // Internal Temp Sensor
  EPD_W21_WriteDATA(0x80);

  // Set RAM Pointer to 0,0
  EPD_W21_WriteCMD(0x4E);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteCMD(0x4F);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);

  lcd_chkstatus();
  EPD_DEBUG_PRINTLN("[EPD] E0213A373 (SSD1680) Full Init complete.");
}

void EPD_update(void) {
  EPD_W21_WriteCMD(0x22);
  EPD_W21_WriteDATA(
      0xF7); // Full update: Enable CP, Load LUT, Display, Disable CP
  EPD_W21_WriteCMD(0x20);
  lcd_chkstatus();
}

void EPD_update_Partial(void) {
  EPD_W21_WriteCMD(0x22);
  // 0xC7 = Enable Clk+Analog, skip Temp+LUT load, Display Mode 2 only, Disable Analog+Clk.
  // Mode 1 bit (bit3) intentionally off — setting both Mode1+Mode2 simultaneously is undefined.
  EPD_W21_WriteDATA(0xC7);
  EPD_W21_WriteCMD(0x20);
  lcd_chkstatus();
}

// Non-blocking version: triggers update and returns immediately.
// Caller MUST call lcd_chkstatus() before any further SPI writes (esp. 0x26 baseline).
void EPD_update_Partial_Async(void) {
  EPD_W21_WriteCMD(0x22);
  EPD_W21_WriteDATA(0xC7);
  EPD_W21_WriteCMD(0x20);  // Trigger — but do NOT wait for BUSY
  // Panel will self-complete. No lcd_chkstatus() here.
}

void PIC_display(const unsigned char *picData, bool partial) {
  unsigned int k;
  static unsigned char fastBuf[4000];

  EPD_DEBUG_PRINT("[EPD] PIC_display Mode: ");
  EPD_DEBUG_PRINTLN(partial ? "PARTIAL" : "FULL");
  uint32_t pic_t0 = millis();

  // --- STEP 1: CONVERT IMAGE (2bpp GUI buffer → 1bpp EPD) ---
  _build_pix_lut();
  uint16_t bufIdx = 0;
  for (k = 0; k < 8000; k += 2) {
    fastBuf[bufIdx++] = (_pix_lut[picData[k]] << 4) | _pix_lut[picData[k + 1]];
  }

  EPD_DEBUG_PRINT("[EPD] Image converted: ");
  EPD_DEBUG_PRINT(millis() - pic_t0);
  EPD_DEBUG_PRINTLN("ms");

  if (!partial) {
    // --- FULL REFRESH ---
    // Write the current image to 0x26 (Old RAM) as the baseline for the next
    // partial refresh. Previously this wrote all-zeros, which caused the
    // controller to see a full "black→image" diff on the first partial update
    // and drive all pixels with the wrong waveform.
    EPD_DEBUG_PRINTLN("[EPD] Writing baseline to Old RAM (0x26)...");
    EPD_W21_WriteCMD(0x4E); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x4F); EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x26);
    EPD_W21_WriteBatchDATA(fastBuf, 4000);

    EPD_DEBUG_PRINTLN("[EPD] Writing New RAM (0x24)...");
    EPD_W21_WriteCMD(0x4E); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x4F); EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x24);
    EPD_W21_WriteBatchDATA(fastBuf, 4000);

    EPD_DEBUG_PRINTLN("[EPD] Triggering Full Update (blocking)...");
    EPD_update();
    EPD_DEBUG_PRINT("[EPD] Full update done. Total PIC_display: ");
    EPD_DEBUG_PRINT(millis() - pic_t0);
    EPD_DEBUG_PRINTLN("ms");

  } else {
    // --- PARTIAL REFRESH ---
    // Write new image to 0x24. 0x26 holds the previous frame — controller
    // compares them and only drives changed pixels.
    EPD_DEBUG_PRINTLN("[EPD] Writing New RAM (0x24)...");
    EPD_W21_WriteCMD(0x4E); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x4F); EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x24);
    EPD_W21_WriteBatchDATA(fastBuf, 4000);

    EPD_DEBUG_PRINTLN("[EPD] Triggering Partial Update...");
    EPD_update_Partial_Async();

    // MUST wait for BUSY=LOW before writing 0x26. The controller reads 0x26
    // immediately after 0x20 to compute the diff — writing during BUSY corrupts
    // the comparison and causes ghosting or missed pixel updates.
    lcd_chkstatus();

    EPD_DEBUG_PRINTLN("[EPD] Updating Old RAM (0x26) baseline for next cycle...");
    EPD_W21_WriteCMD(0x4E); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x4F); EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);
    EPD_W21_WriteCMD(0x26);
    EPD_W21_WriteBatchDATA(fastBuf, 4000);

    EPD_DEBUG_PRINT("[EPD] Partial done. Total PIC_display: ");
    EPD_DEBUG_PRINT(millis() - pic_t0);
    EPD_DEBUG_PRINTLN("ms");
  }
}

void Display_All_White(void) {
  EPD_W21_WriteCMD(0x24);
  for (int i = 0; i < 4000; i++)
    EPD_W21_WriteDATA(0xFF);
  EPD_W21_WriteCMD(0x26);
  for (int i = 0; i < 4000; i++)
    EPD_W21_WriteDATA(0x00);
  EPD_update();
}

void Display_All_Black(void) {
  EPD_W21_WriteCMD(0x24);
  for (int i = 0; i < 4000; i++)
    EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteCMD(0x26);
  for (int i = 0; i < 4000; i++)
    EPD_W21_WriteDATA(0x00);
  EPD_update();
}

void EPD_DeepSleep(void) {
  EPD_W21_WriteCMD(0x10);
  EPD_W21_WriteDATA(0x01);
}

// Writes only physical X bytes 12-15 (GUI Y 96-127, the footer bar strip) to 0x24 and 0x26.
// The rest of 0x24/0x26 is left untouched, so the dashboard remains visible everywhere
// except the footer strip. Called from epd_show_reset_countdown for clean partial updates.
void PIC_display_bar_partial(const unsigned char* picData) {
  // GUI Y 96..127 → Physical X 96..127 → bytes 12..15 (8px per byte)
  // 4 bytes/row × 250 rows = 1000 bytes
  static uint8_t barBuf[1000];
  _build_pix_lut();

  for (int row = 0; row < 250; row++) {
    int picBase = row * 32; // 2bpp: 128px / 4px per byte = 32 bytes per physical row
    for (int xb = 12; xb <= 15; xb++) {
      int k = picBase + xb * 2;
      barBuf[row * 4 + (xb - 12)] = (_pix_lut[picData[k]] << 4) | _pix_lut[picData[k + 1]];
    }
  }

  // Restrict X range to bytes 12-15 only
  EPD_W21_WriteCMD(0x44);
  EPD_W21_WriteDATA(0x0C);
  EPD_W21_WriteDATA(0x0F);

  EPD_W21_WriteCMD(0x45);
  EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0xF9); EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0x4E); EPD_W21_WriteDATA(0x0C);
  EPD_W21_WriteCMD(0x4F); EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);

  // Write new bar content to 0x24 (rest of 0x24 unchanged = dashboard)
  EPD_W21_WriteCMD(0x24);
  EPD_W21_WriteBatchDATA(barBuf, 1000);

  EPD_update_Partial_Async();

  // Wait for BUSY=LOW before writing 0x26 — same race condition as PIC_display().
  lcd_chkstatus();

  // Update 0x26 baseline so next partial cycle has correct reference
  EPD_W21_WriteCMD(0x4E); EPD_W21_WriteDATA(0x0C);
  EPD_W21_WriteCMD(0x4F); EPD_W21_WriteDATA(0x00); EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteCMD(0x26);
  EPD_W21_WriteBatchDATA(barBuf, 1000);
}

void EPD_init_Fast(void) { EPD_Init(); }
void Display_All_Red(void) {}
unsigned char Color_get(unsigned char color) { return color; }
void EPD_init_180(void) { EPD_Init(); }
void EPD_Init_E0213A373_Partial(void) {
  // Hardware RST resets registers only — does NOT wipe RAM banks.
  // Never send SW Reset (0x12) here: it would destroy the 0x26 baseline
  // that the controller needs to compare against for differential refresh.
  EPD_W21_RST_0;
  delay_xms(10);
  EPD_W21_RST_1;
  delay_xms(10);
  lcd_chkstatus();

  EPD_W21_WriteCMD(0x0C);  // Booster Soft Start
  EPD_W21_WriteDATA(0x8B); // High current
  EPD_W21_WriteDATA(0x9C);
  EPD_W21_WriteDATA(0x96);
  EPD_W21_WriteDATA(0x0F);

  EPD_W21_WriteCMD(0x01); // Driver output control
  EPD_W21_WriteDATA(0xF9);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0x11); // Data entry mode 3
  EPD_W21_WriteDATA(0x03);

  EPD_W21_WriteCMD(0x44); // RAM-X range
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x0F);

  EPD_W21_WriteCMD(0x45); // RAM-Y range
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0xF9);
  EPD_W21_WriteDATA(0x00);

  EPD_W21_WriteCMD(0x3C);  // Border Waveform
  EPD_W21_WriteDATA(0x01); // White Border

  EPD_W21_WriteCMD(0x2C);  // VCOM Voltage
  EPD_W21_WriteDATA(0x36); // High contrast restored

  EPD_W21_WriteCMD(0x21); // Display Update Control 1
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x80);

#if EPD_MODEL_E0213A373
  // 0x18 partial'da kasıtlı atlanıyor: sıcaklık fazını tetikler → +1-2sn gecikme
#elif EPD_MODEL_DIE02213S
  // DIE02213S (SSD1680Z/JD79661): 2270ms gecikme kaynağı bu olabilir.
  // Kaldırmak için önce E0213A373'te doğrula, sonra burayı da kaldır.
  EPD_W21_WriteCMD(0x18);
  EPD_W21_WriteDATA(0x80);
#endif

  EPD_W21_WriteCMD(0x4E); // Set RAM Pointer
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteCMD(0x4F);
  EPD_W21_WriteDATA(0x00);
  EPD_W21_WriteDATA(0x00);

  lcd_chkstatus();
  Serial.println("[EPD] E0213A373 Partial Init Fast-Pass.");
}