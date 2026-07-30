/***************************************************
  Boardoza_MAX31865 Library for RTD Sensors
  
 ****************************************************/

#ifndef BOARDOZA_MAX31865_H
#define BOARDOZA_MAX31865_H

/* Register Definitions */
#define MAX31865_CONFIG_REG     0x00
#define MAX31865_CONFIG_BIAS     0x80
#define MAX31865_CONFIG_MODEAUTO 0x40
#define MAX31865_CONFIG_MODEOFF  0x00
#define MAX31865_CONFIG_1SHOT    0x20
#define MAX31865_CONFIG_3WIRE    0x10
#define MAX31865_CONFIG_24WIRE   0x00
#define MAX31865_CONFIG_FAULTSTAT 0x02
#define MAX31865_CONFIG_FILT50HZ 0x01
#define MAX31865_CONFIG_FILT60HZ 0x00

#define MAX31865_RTDMSB_REG      0x01
#define MAX31865_RTDLSB_REG      0x02
#define MAX31865_HFAULTMSB_REG   0x03
#define MAX31865_HFAULTLSB_REG   0x04
#define MAX31865_LFAULTMSB_REG   0x05
#define MAX31865_LFAULTLSB_REG   0x06
#define MAX31865_FAULTSTAT_REG   0x07

/* Fault Status Definitions */
#define MAX31865_FAULT_HIGHTHRESH 0x80
#define MAX31865_FAULT_LOWTHRESH  0x40
#define MAX31865_FAULT_REFINLOW   0x20
#define MAX31865_FAULT_REFINHIGH  0x10
#define MAX31865_FAULT_RTDINLOW   0x08
#define MAX31865_FAULT_OVUV       0x04

/* Callendar-Van Dusen Coefficients */
#define RTD_A 3.9083e-3
#define RTD_B -5.775e-7

#if (ARDUINO >= 100)
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include <Adafruit_SPIDevice.h>

/** * @brief RTD Wire Count Selection 
 */
typedef enum max31865_numwires {
  MAX31865_2WIRE = 0,
  MAX31865_3WIRE = 1,
  MAX31865_4WIRE = 0
} max31865_numwires_t;

/** * @brief Fault Detection Cycle Selection 
 */
typedef enum {
  MAX31865_FAULT_NONE = 0,
  MAX31865_FAULT_AUTO,
  MAX31865_FAULT_MANUAL_RUN,
  MAX31865_FAULT_MANUAL_FINISH
} max31865_fault_cycle_t;

/** * @class Boardoza_MAX31865
 * @brief Main class for interfacing with the MAX31865 RTD-to-Digital Converter.
 */
class Boardoza_MAX31865 {
public:
  /* Software SPI Constructor */
  /**
   * @brief Create an instance using software (bit-bang) SPI.
   * @param spi_cs Chip Select pin.
   * @param spi_mosi MOSI pin.
   * @param spi_miso MISO pin.
   * @param spi_clk Clock pin.
   */
  Boardoza_MAX31865(int8_t spi_cs, int8_t spi_mosi, int8_t spi_miso,
                    int8_t spi_clk);

  /* Hardware SPI Constructor */
  /**
   * @brief Create an instance using hardware SPI.
   * @param spi_cs Chip Select pin.
   * @param theSPI Pointer to SPIClass instance (defaults to &SPI).
   */
  Boardoza_MAX31865(SPIClass *theSPI = &SPI);
  /**
   * @brief Initialize the device and apply default settings.
   * @param wires RTD wire configuration (defaults to 2-wire).
   * @return True if initialization was successful.
   */
  bool begin(max31865_numwires_t wires = MAX31865_2WIRE);

  /**
   * @brief Read the fault status register.
   * @param fault_cycle Desired fault detection cycle mode.
   * @return Fault register byte value.
   */
  uint8_t readFault(max31865_fault_cycle_t fault_cycle = MAX31865_FAULT_AUTO);

  /** @brief Clear all fault status bits. */
  void clearFault(void);

  /** @brief Read the raw 15-bit RTD resistance value. */
  uint16_t readRTD();

  /**
   * @brief Set the high and low fault thresholds.
   * @param lower Raw 15-bit lower threshold.
   * @param upper Raw 15-bit upper threshold.
   */
  void setThresholds(uint16_t lower, uint16_t upper);

  /** @brief Get the current lower fault threshold. */
  uint16_t getLowerThreshold(void);

  /** @brief Get the current upper fault threshold. */
  uint16_t getUpperThreshold(void);

  /**
   * @brief Configure the number of wires used by the RTD.
   * @param wires RTD wire configuration.
   */
  void setWires(max31865_numwires_t wires);

  /**
   * @brief Enable or disable automatic conversion mode.
   * @param b True for automatic, false for manual.
   */
  void autoConvert(bool b);

  /**
   * @brief Select the noise rejection filter frequency.
   * @param b True for 50 Hz, false for 60 Hz.
   */
  void enable50Hz(bool b);

  /**
   * @brief Enable or disable the V_bias voltage.
   * @param b True to enable, false to disable.
   */
  void enableBias(bool b);

  /**
   * @brief Read the RTD and return the temperature in Celsius.
   * @param RTDnominal Nominal resistance of the RTD at 0°C (e.g., 100.0 for PT100).
   * @param refResistor Value of the reference resistor on the PCB (e.g., 430.0).
   * @return Temperature in Celsius.
   */
  float temperature(float RTDnominal, float refResistor);

  /**
   * @brief Calculate temperature from a raw RTD resistance reading.
   * @param RTDraw Raw 15-bit ADC value.
   * @param RTDnominal Nominal resistance of the RTD (ohms).
   * @param refResistor Reference resistor value (ohms).
   * @return Calculated temperature in Celsius.
   */
  float calculateTemperature(uint16_t RTDraw, float RTDnominal,
                               float refResistor);

private:
  Adafruit_SPIDevice spi_dev;

  /**
   * @brief Read multiple bytes from the device starting at a register address.
   * @param addr Register address.
   * @param buffer Pointer to the output buffer.
   * @param n Number of bytes to read.
   */
  void readRegisterN(uint8_t addr, uint8_t buffer[], uint8_t n);

  /**
   * @brief Read a single byte from a register.
   * @param addr Register address.
   * @return Byte read from the register.
   */
  uint8_t readRegister8(uint8_t addr);

  /**
   * @brief Read a 16-bit word from a register.
   * @param addr Starting register address.
   * @return 16-bit value read from the device.
   */
  uint16_t readRegister16(uint8_t addr);

  /**
   * @brief Write a single byte to a register.
   * @param addr Register address.
   * @param data Byte value to write.
   */
  void writeRegister8(uint8_t addr, uint8_t data);
};

#endif