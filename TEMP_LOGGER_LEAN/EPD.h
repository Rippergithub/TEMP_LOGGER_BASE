#ifndef _EPD_H_
#define _EPD_H_

#define EPD_WIDTH 128
#define EPD_VISIBLE_WIDTH 122
#define EPD_HEIGHT 250
#define EPD_ARRAY ((EPD_WIDTH * EPD_HEIGHT) / 4) // 2-bit per pixel (SSD1680 mapping)

// 2bit GUI colors
#define white 0x01
#define black 0x00
#define yellow 0x02
#define red 0x03

// E0213A373 Models
void EPD_Init_E0213A373_Full(void);
void EPD_Init_E0213A373_Partial(void);

// General Wrapper
void EPD_Init(void); // Matches DisplayManager.cpp call
void EPD_Init_Custom(bool partial);

void Display_All_Black(void);
void Display_All_White(void);
void Display_All_Yellow(void);
void Display_All_Red(void);

void Acep_color(unsigned char color);
void EPD_init_Fast(void);

// GUI display
void EPD_HW_Init_GUI(void);
void EPD_Display(unsigned char *datasBW, unsigned char *datasRW);
// EPD Update
void EPD_update(void);
void EPD_update_Partial(void);
void EPD_update_Partial_Async(void); // Non-blocking: fire and forget
void EPD_DeepSleep(void);
void PIC_display(const unsigned char *picData, bool partial = false);
// Partial update: only physical X bytes 12-15 (= GUI Y 96-127, footer strip)
// Leaves the rest of 0x24/0x26 intact so the dashboard stays visible
void PIC_display_bar_partial(const unsigned char *picData);

#endif
