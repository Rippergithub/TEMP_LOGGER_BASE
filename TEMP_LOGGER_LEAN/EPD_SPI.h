#ifndef _EPD_SPI_H_
#define _EPD_SPI_H_

#include <Arduino.h>
#include <SPI.h>

// Pin Tanımların (Senin .ino dosyasından alındı)
#define EPD_W21_CS 20
#define EPD_W21_DC 5
#define EPD_W21_RST 19
#define EPD_W21_BUSY 14

// Makrolar
#define EPD_W21_RST_0 digitalWrite(EPD_W21_RST, LOW)
#define EPD_W21_RST_1 digitalWrite(EPD_W21_RST, HIGH)
#define EPD_W21_DC_0 digitalWrite(EPD_W21_DC, LOW)
#define EPD_W21_DC_1 digitalWrite(EPD_W21_DC, HIGH)
#define EPD_W21_CS_0 digitalWrite(EPD_W21_CS, LOW)
#define EPD_W21_CS_1 digitalWrite(EPD_W21_CS, HIGH)
#define isEPD_W21_BUSY digitalRead(EPD_W21_BUSY)

// SPI Ayarları
#define EPD_SPI_SETTINGS SPISettings(2000000, MSBFIRST, SPI_MODE0)

inline void EPD_W21_WriteCMD(unsigned char command) {
  SPI.beginTransaction(EPD_SPI_SETTINGS);
  EPD_W21_CS_0;
  EPD_W21_DC_0;
  SPI.transfer(command);
  EPD_W21_CS_1;
  SPI.endTransaction();
}

inline void EPD_W21_WriteDATA(unsigned char data) {
  SPI.beginTransaction(EPD_SPI_SETTINGS);
  EPD_W21_CS_0;
  EPD_W21_DC_1;
  SPI.transfer(data);
  EPD_W21_CS_1;
  SPI.endTransaction();
}

inline void EPD_W21_WriteBatchDATA(const unsigned char *data, uint32_t len) {
  SPI.beginTransaction(EPD_SPI_SETTINGS);
  EPD_W21_CS_0;
  EPD_W21_DC_1;
  SPI.writeBytes(data, len);
  EPD_W21_CS_1;
  SPI.endTransaction();
}
#endif