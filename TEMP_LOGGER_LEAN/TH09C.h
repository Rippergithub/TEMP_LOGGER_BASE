/*!
 *  @file TH09C.h
 *
 *  @mainpage TH09C is a low cost temperature/humidity sensor.
 *
 *  @section intro_sec Introduction
 *
 *  This is a library for Boardoza TH09C low cost temperature/humidity sensors.
 *
 *  @section author Author
 *
 *  Written by Boardoza.
 *
 *  @section license License
 *
 *  MIT license, all text above must be included in any redistribution
 */

#ifndef TH09C_h
#define TH09C_h

#include <Arduino.h>
#include <Wire.h>

#define TH09_Address 0x43
#define PART_ID 0x00
#define DIE_REV 0x02
#define UID 0x04
#define SYS_CTRL 0x10
#define SYS_STAT 0x11
#define SENS_RUN 0x21
#define SENS_START 0x22
#define SENS_STOP 0x23
#define SENS_STAT 0x24
#define T_VAL 0x30
#define H_VAL 0x33

class TH09C {
public:
  // v5.7.1: Soft reset olmadan sadece SENS_RUN=0x00 yazar.
  // Deep sleep sonrası register'lar korunmuşsa begin() yerine bu çağrılır.
  void setManualMode();
  TH09C();
  void begin();
  void triggerMeasurement(); // v5.15.9: Asynchronous trigger
  void fetchData(float &temperature, float &humidity); // v5.15.9: Non-blocking fetch
  void readTempHum(float &temperature, float &humidity); // Legacy blocking
  void readTemp(float &temperature);
  void readHum(float &humidity);
  float kelvinConverter(float &temperature, char unit);
  float celciusConverter(float &temperature, char unit);
  float fahrenheitConverter(float &temperature, char unit);
  void calibration();

private:
  uint8_t readByteOneReg(uint8_t devAddr, uint8_t regAddr);
  uint8_t readMultiBytesOneReg(uint8_t devAddr, uint8_t regAddr, uint8_t *data,
                               uint8_t count);
  uint8_t writeByteOneReg(uint8_t devAddr, uint8_t regAddr, uint8_t data);
};

#endif