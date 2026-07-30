#include "GUI_Paint.h"
#include <math.h>
#include <string.h>

PAINT Paint;

void Paint_NewImage(unsigned char *image, uint16_t Width, uint16_t Height,
                    uint16_t Rotate, uint16_t Color) {
  Paint.Image = (uint8_t *)image;
  Paint.WidthMemory = Width;
  Paint.HeightMemory = Height;
  Paint.Color = Color;
  Paint.Width = Width;
  Paint.Height = Height;
  Paint.Rotate = Rotate;
  Paint.Mirror = 0;

  if (Rotate == ROTATE_0 || Rotate == ROTATE_180) {
    Paint.Width = Width;
    Paint.Height = Height;
  } else {
    Paint.Width = Height;
    Paint.Height = Width;
  }
}

void Paint_SelectImage(unsigned char *image) { Paint.Image = (uint8_t *)image; }

void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color) {
  if (Xpoint >= Paint.Width || Ypoint >= Paint.Height) {
    return;
  }
  uint16_t X, Y;

  switch (Paint.Rotate) {
  case 0:
    X = Xpoint;
    Y = Ypoint;
    break;
  case 90:
    X = Paint.WidthMemory - Ypoint - 1;
    Y = Xpoint;
    break;
  case 180:
    X = Paint.WidthMemory - Xpoint - 1;
    Y = Paint.HeightMemory - Ypoint - 1;
    break;
  case 270:
    X = Ypoint;
    Y = Paint.HeightMemory - Xpoint - 1;
    break;
  default:
    return;
  }

  uint32_t Addr = (Y * Paint.WidthMemory + X) / 4;
  uint8_t Remaid = (Y * Paint.WidthMemory + X) % 4;

  // We have 2 bits per pixel. 4 pixels per byte.
  // Pixel 0: Bits 7,6
  // Pixel 1: Bits 5,4
  // Pixel 2: Bits 3,2
  // Pixel 3: Bits 1,0
  uint8_t Shift = (3 - Remaid) * 2;
  Paint.Image[Addr] &= ~(0x03 << Shift);
  Paint.Image[Addr] |= (Color & 0x03) << Shift;
}

void Paint_Clear(uint16_t Color) {
  uint8_t c = (Color & 0x03);
  uint8_t full_byte = (c << 6) | (c << 4) | (c << 2) | c;
  memset(Paint.Image, full_byte, Paint.WidthMemory * Paint.HeightMemory / 4);
}

void Paint_DrawPoint(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color,
                     DOT_PIXEL Dot_Pixel, DOT_PIXEL Dot_Fill) {
  if (Xpoint >= Paint.Width || Ypoint >= Paint.Height) {
    return;
  }

  int16_t XDir_Num, YDir_Num;
  if (Dot_Fill == DOT_PIXEL_1X1) {
    for (XDir_Num = 0; XDir_Num < Dot_Pixel; XDir_Num++) {
      for (YDir_Num = 0; YDir_Num < Dot_Pixel; YDir_Num++) {
        Paint_SetPixel(Xpoint + XDir_Num, Ypoint + YDir_Num, Color);
      }
    }
  } else {
    for (XDir_Num = -(Dot_Pixel - 1) / 2; XDir_Num < Dot_Pixel / 2 + 1;
         XDir_Num++) {
      for (YDir_Num = -(Dot_Pixel - 1) / 2; YDir_Num < Dot_Pixel / 2 + 1;
           YDir_Num++) {
        Paint_SetPixel(Xpoint + XDir_Num, Ypoint + YDir_Num, Color);
      }
    }
  }
}

void Paint_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend,
                    uint16_t Yend, uint16_t Color, LINE_STYLE Line_Style,
                    DOT_PIXEL Dot_Pixel) {
  if (Xstart >= Paint.Width || Ystart >= Paint.Height || Xend >= Paint.Width ||
      Yend >= Paint.Height) {
    return;
  }

  uint16_t Xpoint = Xstart;
  uint16_t Ypoint = Ystart;
  int dx = (int)Xend - (int)Xstart >= 0 ? Xend - Xstart : Xstart - Xend;
  int dy = (int)Yend - (int)Ystart <= 0 ? Yend - Ystart : Ystart - Yend;

  int xstep = Xstart < Xend ? 1 : -1;
  int ystep = Ystart < Yend ? 1 : -1;

  int eps = dx + dy;
  int i = 0;
  while (1) {
    i++;
    if (Line_Style == LINE_STYLE_DOTTED && i % 3 == 0) {
      // Skip for dotted
    } else {
      Paint_DrawPoint(Xpoint, Ypoint, Color, Dot_Pixel, DOT_PIXEL_1X1);
    }
    if (Xpoint == Xend && Ypoint == Yend)
      break;
    int eps2 = 2 * eps;
    if (eps2 >= dy) {
      eps += dy;
      Xpoint += xstep;
    }
    if (eps2 <= dx) {
      eps += dx;
      Ypoint += ystep;
    }
  }
}

void Paint_DrawRectangle(uint16_t Xstart, uint16_t Ystart, uint16_t Xend,
                         uint16_t Yend, uint16_t Color, DRAW_FILL Filled,
                         DOT_PIXEL Dot_Pixel) {
  if (Xstart >= Paint.Width || Ystart >= Paint.Height || Xend >= Paint.Width ||
      Yend >= Paint.Height) {
    return;
  }

  if (Filled) {
    for (uint16_t Ypoint = Ystart; Ypoint <= Yend; Ypoint++) {
      Paint_DrawLine(Xstart, Ypoint, Xend, Ypoint, Color, LINE_STYLE_SOLID,
                     Dot_Pixel);
    }
  } else {
    Paint_DrawLine(Xstart, Ystart, Xend, Ystart, Color, LINE_STYLE_SOLID,
                   Dot_Pixel);
    Paint_DrawLine(Xstart, Ystart, Xstart, Yend, Color, LINE_STYLE_SOLID,
                   Dot_Pixel);
    Paint_DrawLine(Xend, Ystart, Xend, Yend, Color, LINE_STYLE_SOLID,
                   Dot_Pixel);
    Paint_DrawLine(Xstart, Yend, Xend, Yend, Color, LINE_STYLE_SOLID,
                   Dot_Pixel);
  }
}

void Paint_DrawChar(uint16_t Xstart, uint16_t Ystart, const char Ascii_Char,
                    sFONT *Font, uint16_t Color_Background,
                    uint16_t Color_Foreground) {
  if (Xstart >= Paint.Width || Ystart >= Paint.Height) {
    return;
  }

  uint16_t Page, Column;
  uint32_t Char_Offset = (Ascii_Char - ' ') * Font->Height *
                         (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
  const uint8_t *ptr = &Font->table[Char_Offset];

  for (Page = 0; Page < Font->Height; Page++) {
    for (Column = 0; Column < Font->Width; Column++) {
      if (*ptr & (0x80 >> (Column % 8))) {
        Paint_SetPixel(Xstart + Column, Ystart + Page, Color_Foreground);
      } else {
        Paint_SetPixel(Xstart + Column, Ystart + Page, Color_Background);
      }
      if (Column % 8 == 7) {
        ptr++;
      }
    }
    if (Font->Width % 8 != 0) {
      ptr++;
    }
  }
}

void Paint_DrawString_EN(uint16_t Xstart, uint16_t Ystart, const char *pString,
                         sFONT *Font, uint16_t Color_Background,
                         uint16_t Color_Foreground) {
  uint16_t Xpoint = Xstart;
  uint16_t Ypoint = Ystart;

  if (Xstart >= Paint.Width || Ystart >= Paint.Height) {
    return;
  }

  while (*pString != '\0') {
    if ((Xpoint + Font->Width) > Paint.Width) {
      Xpoint = Xstart;
      Ypoint += Font->Height;
    }
    if ((Ypoint + Font->Height) > Paint.Height) {
      break;
    }
    Paint_DrawChar(Xpoint, Ypoint, *pString, Font, Color_Background,
                   Color_Foreground);
    pString++;
    Xpoint += Font->Width;
  }
}
void Paint_DrawCircle(uint16_t X_Center, uint16_t Y_Center, uint16_t Radius,
                      uint16_t Color, DRAW_FILL Filled, DOT_PIXEL Dot_Pixel) {
  if (X_Center >= Paint.Width || Y_Center >= Paint.Height) {
    return;
  }

  int16_t XCurrent, YCurrent;
  XCurrent = 0;
  YCurrent = Radius;
  int16_t Esp = 3 - (Radius << 1);

  if (Filled == DRAW_FILL_FULL) {
    while (XCurrent <= YCurrent) {
      Paint_DrawLine(X_Center - XCurrent, Y_Center - YCurrent,
                     X_Center + XCurrent, Y_Center - YCurrent, Color,
                     LINE_STYLE_SOLID, Dot_Pixel);
      Paint_DrawLine(X_Center - YCurrent, Y_Center - XCurrent,
                     X_Center + YCurrent, Y_Center - XCurrent, Color,
                     LINE_STYLE_SOLID, Dot_Pixel);
      Paint_DrawLine(X_Center - XCurrent, Y_Center + YCurrent,
                     X_Center + XCurrent, Y_Center + YCurrent, Color,
                     LINE_STYLE_SOLID, Dot_Pixel);
      Paint_DrawLine(X_Center - YCurrent, Y_Center + XCurrent,
                     X_Center + YCurrent, Y_Center + XCurrent, Color,
                     LINE_STYLE_SOLID, Dot_Pixel);

      if (Esp < 0) {
        Esp += 4 * XCurrent + 6;
      } else {
        Esp += 10 + 4 * (XCurrent - YCurrent);
        YCurrent--;
      }
      XCurrent++;
    }
  } else {
    while (XCurrent <= YCurrent) {
      Paint_DrawPoint(X_Center + XCurrent, Y_Center + YCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center - XCurrent, Y_Center + YCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center - YCurrent, Y_Center + XCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center - YCurrent, Y_Center - XCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center - XCurrent, Y_Center - YCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center + XCurrent, Y_Center - YCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center + YCurrent, Y_Center - XCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);
      Paint_DrawPoint(X_Center + YCurrent, Y_Center + XCurrent, Color,
                      Dot_Pixel, DOT_PIXEL_1X1);

      if (Esp < 0) {
        Esp += 4 * XCurrent + 6;
      } else {
        Esp += 10 + 4 * (XCurrent - YCurrent);
        YCurrent--;
      }
      XCurrent++;
    }
  }
}
