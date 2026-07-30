#ifndef __GUI_PAINT_H
#define __GUI_PAINT_H

#include <Arduino.h>

/**
 * Image orientation
 **/
#define ROTATE_0 0
#define ROTATE_90 90
#define ROTATE_180 180
#define ROTATE_270 270

/**
 * Image logic
 **/
#define WHITE0 0x00
#define YELLOW0 0x01
#define RED0 0x02
#define BLACK0 0x03

/**
 * Display coordinates
 **/
typedef struct {
  uint16_t Width;
  uint16_t Height;
  uint16_t Rotate;
  uint16_t Mirror;
  uint16_t WidthMemory;
  uint16_t HeightMemory;
  uint16_t Color;
  uint16_t Memory_Width;
  uint16_t Memory_Height;
  uint8_t *Image;
} PAINT;

/**
 * Dot pixel size
 **/
typedef enum {
  DOT_PIXEL_1X1 = 1, // 1x1
  DOT_PIXEL_2X2,     // 2x2
  DOT_PIXEL_3X3,     // 3x3
  DOT_PIXEL_4X4,     // 4x4
  DOT_PIXEL_5X5,     // 5x5
  DOT_PIXEL_6X6,     // 6x6
  DOT_PIXEL_7X7,     // 7x7
  DOT_PIXEL_8X8,     // 8x8
} DOT_PIXEL;

/**
 * Line Style
 **/
typedef enum {
  LINE_STYLE_SOLID = 0,
  LINE_STYLE_DOTTED,
} LINE_STYLE;

/**
 * Rectangle Fill
 **/
typedef enum {
  DRAW_FILL_EMPTY = 0,
  DRAW_FILL_FULL,
} DRAW_FILL;

/**
 * Font Data Structure
 **/
#include "Fonts.h"

// GUI Functions
void Paint_NewImage(unsigned char *image, uint16_t Width, uint16_t Height,
                    uint16_t Rotate, uint16_t Color);
void Paint_SelectImage(unsigned char *image);
void Paint_SetRotate(uint16_t Rotate);
void Paint_SetMirroring(uint8_t mirror);
void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color);

void Paint_Clear(uint16_t Color);
void Paint_ClearWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend,
                        uint16_t Yend, uint16_t Color);
void Paint_DrawPoint(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color,
                     DOT_PIXEL Dot_Pixel, DOT_PIXEL Dot_Fill);
void Paint_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend,
                    uint16_t Yend, uint16_t Color, LINE_STYLE Line_Style,
                    DOT_PIXEL Dot_Pixel);
void Paint_DrawRectangle(uint16_t Xstart, uint16_t Ystart, uint16_t Xend,
                         uint16_t Yend, uint16_t Color, DRAW_FILL Filled,
                         DOT_PIXEL Dot_Pixel);
void Paint_DrawCircle(uint16_t X_Center, uint16_t Y_Center, uint16_t Radius,
                      uint16_t Color, DRAW_FILL Filled, DOT_PIXEL Dot_Pixel);
void Paint_DrawChar(uint16_t Xstart, uint16_t Ystart, const char Ascii_Char,
                    sFONT *Font, uint16_t Color_Background,
                    uint16_t Color_Foreground);
void Paint_DrawString_EN(uint16_t Xstart, uint16_t Ystart, const char *pString,
                         sFONT *Font, uint16_t Color_Background,
                         uint16_t Color_Foreground);
void Paint_DrawNum(uint16_t Xpoint, uint16_t Ypoint, double Nummber,
                   sFONT *Font, uint16_t Digit, uint16_t Color_Background,
                   uint16_t Color_Foreground);

#endif
