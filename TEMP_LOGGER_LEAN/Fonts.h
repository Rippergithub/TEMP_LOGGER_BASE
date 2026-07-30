#ifndef __FONTS_H
#define __FONTS_H

#include <stdint.h>

// Font data structure
typedef struct _tFont {
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
} sFONT;

extern sFONT Font24; // Now used for Large Temperature
extern sFONT Font22; // New optimized measurement font
extern sFONT Font20;
extern sFONT Font16; // Standard UI font
extern sFONT Font12;
extern sFONT Font10;
extern sFONT Font8;
extern sFONT Font7Seg;
extern sFONT FontInter24;

#endif
