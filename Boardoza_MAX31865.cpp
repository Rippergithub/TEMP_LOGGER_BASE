/***************************************************
  Boardoza_MAX31865 Library for RTD Sensors
  
 ****************************************************/

#include "Boardoza_MAX31865.h"
#ifdef __AVR
#include <avr/pgmspace.h>
#elif defined(ESP8266)
#include <pgmspace.h>
#endif

#include <stdlib.h>

/* Constructor for Software SPI (Bit-banging) */
/**
 * @brief Create an instance using software (bit-bang) SPI.
 * @param spi_cs CS pin.
 * @param spi_mosi MOSI pin.
 * @param spi_miso MISO pin.
 * @param spi_clk SCK pin.
 */
Boardoza_MAX31865::Boardoza_MAX31865(int8_t spi_cs, int8_t spi_mosi,
                                     int8_t spi_miso, int8_t spi_clk)
    : spi_dev(spi_cs, spi_clk, spi_miso, spi_mosi, 1000000,
              SPI_BITORDER_MSBFIRST, SPI_MODE1) {}

/* Constructor for Hardware SPI */
/**
 * @brief Create an instance using hardware SPI.
 * @param spi_cs CS pin.
 * @param theSPI SPI instance to use.
 */
Boardoza_MAX31865::Boardoza_MAX31865(SPIClass *theSPI)
    : spi_dev(21, 1000000, SPI_BITORDER_MSBFIRST, SPI_MODE1, theSPI) {}

/**
 * @brief Initialize the device and apply default settings.
 * @param wires Wire count selection (2-wire, 3-wire, or 4-wire).
 * @return True if initialization succeeded.
 */
bool Boardoza_MAX31865::begin(max31865_numwires_t wires) {
  spi_dev.begin();

  setWires(wires);
  enableBias(false);
  autoConvert(false);
  setThresholds(0, 0xFFFF);
  clearFault();

  return true;
}

/**
 * @brief Read the fault status register.
 * @param fault_cycle Fault cycle mode selection.
 * @return Fault register value (0 if no fault).
 */
uint8_t Boardoza_MAX31865::readFault(max31865_fault_cycle_t fault_cycle) {
  if (fault_cycle) {
    uint8_t cfg_reg = readRegister8(MAX31865_CONFIG_REG);
    cfg_reg &= 0x11; // Mask out wire and filter bits
    switch (fault_cycle) {
    case MAX31865_FAULT_AUTO:
      writeRegister8(MAX31865_CONFIG_REG, (cfg_reg | 0b10000100));
      delay(1);
      break;
    case MAX31865_FAULT_MANUAL_RUN:
      writeRegister8(MAX31865_CONFIG_REG, (cfg_reg | 0b10001000));
      return 0;
    case MAX31865_FAULT_MANUAL_FINISH:
      writeRegister8(MAX31865_CONFIG_REG, (cfg_reg | 0b10001100));
      return 0;
    case MAX31865_FAULT_NONE:
    default:
      break;
    }
  }
  return readRegister8(MAX31865_FAULTSTAT_REG);
}

/**
 * @brief Clear all fault status bits in the configuration register.
 */
void Boardoza_MAX31865::clearFault(void) {
  uint8_t config = readRegister8(MAX31865_CONFIG_REG);
  config &= ~0x2C; // Clear existing fault bits
  config |= MAX31865_CONFIG_FAULTSTAT;
  writeRegister8(MAX31865_CONFIG_REG, config);
}

/**
 * @brief Enable or disable the V_bias voltage.
 * @param b True to enable, false to disable.
 */
void Boardoza_MAX31865::enableBias(bool b) {
  uint8_t config = readRegister8(MAX31865_CONFIG_REG);
  if (b) {
    config |= MAX31865_CONFIG_BIAS;
  } else {
    config &= ~MAX31865_CONFIG_BIAS;
  }
  writeRegister8(MAX31865_CONFIG_REG, config);
}

/**
 * @brief Enable or disable automatic conversion mode.
 * @param b True to enable (continuous), false to disable (on-demand).
 */
void Boardoza_MAX31865::autoConvert(bool b) {
  uint8_t config = readRegister8(MAX31865_CONFIG_REG);
  if (b) {
    config |= MAX31865_CONFIG_MODEAUTO;
  } else {
    config &= ~MAX31865_CONFIG_MODEAUTO;
  }
  writeRegister8(MAX31865_CONFIG_REG, config);
}

/**
 * @brief Select notch filter frequency for noise rejection.
 * @param b True for 50 Hz, false for 60 Hz.
 */
void Boardoza_MAX31865::enable50Hz(bool b) {
  uint8_t config = readRegister8(MAX31865_CONFIG_REG);
  if (b) {
    config |= MAX31865_CONFIG_FILT50HZ;
  } else {
    config &= ~MAX31865_CONFIG_FILT50HZ;
  }
  writeRegister8(MAX31865_CONFIG_REG, config);
}

/**
 * @brief Set the high and low fault thresholds.
 * @param lower Lower threshold raw value.
 * @param upper Upper threshold raw value.
 */
void Boardoza_MAX31865::setThresholds(uint16_t lower, uint16_t upper) {
  writeRegister8(MAX31865_LFAULTLSB_REG, lower & 0xFF);
  writeRegister8(MAX31865_LFAULTMSB_REG, lower >> 8);
  writeRegister8(MAX31865_HFAULTLSB_REG, upper & 0xFF);
  writeRegister8(MAX31865_HFAULTMSB_REG, upper >> 8);
}

/**
 * @brief Get the current lower fault threshold.
 * @return 16-bit lower threshold value.
 */
uint16_t Boardoza_MAX31865::getLowerThreshold(void) {
  return readRegister16(MAX31865_LFAULTMSB_REG);
}

/**
 * @brief Get the current upper fault threshold.
 * @return 16-bit upper threshold value.
 */
uint16_t Boardoza_MAX31865::getUpperThreshold(void) {
  return readRegister16(MAX31865_HFAULTMSB_REG);
}

/**
 * @brief Configure the number of wires used by the RTD.
 * @param wires MAX31865_3WIRE for 3-wire, or 2/4 wire mode.
 */
void Boardoza_MAX31865::setWires(max31865_numwires_t wires) {
  uint8_t config = readRegister8(MAX31865_CONFIG_REG);
  if (wires == MAX31865_3WIRE) {
    config |= MAX31865_CONFIG_3WIRE;
  } else {
    config &= ~MAX31865_CONFIG_3WIRE;
  }
  writeRegister8(MAX31865_CONFIG_REG, config);
}

/**
 * @brief High-level function to read RTD and calculate temperature.
 * @param RTDnominal Nominal resistance of the RTD (e.g., 100.0 for PT100).
 * @param refResistor Reference resistor value (e.g., 430.0).
 * @return Calculated temperature in Celsius.
 */
float Boardoza_MAX31865::temperature(float RTDnominal, float refResistor) {
  return calculateTemperature(readRTD(), RTDnominal, refResistor);
}

/**
 * @brief Calculate temperature in Celsius using the Callendar-Van Dusen equation.
 * @param RTDraw Raw 15-bit ADC value from the sensor.
 * @param RTDnominal Nominal resistance of the RTD (ohms).
 * @param refResistor Reference resistor value (ohms).
 * @return Calculated temperature in Celsius.
 */
float Boardoza_MAX31865::calculateTemperature(uint16_t RTDraw, float RTDnominal,
                                              float refResistor) {
  float Z1, Z2, Z3, Z4, Rt, temp;

  Rt = RTDraw;
  Rt /= 32768; // Convert raw value to ratio
  Rt *= refResistor;

  // Formula for positive temperatures
  Z1 = -RTD_A;
  Z2 = RTD_A * RTD_A - (4 * RTD_B);
  Z3 = (4 * RTD_B) / RTDnominal;
  Z4 = 2 * RTD_B;

  temp = Z2 + (Z3 * Rt);
  temp = (sqrt(temp) + Z1) / Z4;

  if (temp >= 0)
    return temp;

  // Polynomial correction for negative temperatures
  Rt /= RTDnominal;
  Rt *= 100; // Normalize to 100 ohms for the polynomial

  float rpoly = Rt;
  temp = -242.02;
  temp += 2.2228 * rpoly;
  rpoly *= Rt; 
  temp += 2.5859e-3 * rpoly;
  rpoly *= Rt; 
  temp -= 4.8260e-6 * rpoly;
  rpoly *= Rt; 
  temp -= 2.8183e-8 * rpoly;
  rpoly *= Rt; 
  temp += 1.5243e-10 * rpoly;

  return temp;
}

/**
 * @brief Perform a single-shot reading of the RTD.
 * @return Raw 15-bit RTD value.
 */
uint16_t Boardoza_MAX31865::readRTD(void) {
  clearFault();
  enableBias(true);
  delay(10); // Wait for V_bias to settle
  uint8_t config = readRegister8(MAX31865_CONFIG_REG);
  config |= MAX31865_CONFIG_1SHOT;
  writeRegister8(MAX31865_CONFIG_REG, config);
  delay(65); // Wait for conversion

  uint16_t rtd = readRegister16(MAX31865_RTDMSB_REG);
  enableBias(false); // Disable bias to prevent self-heating
  rtd >>= 1; // Remove fault bit (LSB)
  return rtd;
}

/**
 * @brief Read an 8-bit value from a register.
 * @param addr Register address.
 * @return The 8-bit value read.
 */
uint8_t Boardoza_MAX31865::readRegister8(uint8_t addr) {
  uint8_t ret = 0;
  readRegisterN(addr, &ret, 1);
  return ret;
}

/**
 * @brief Read a 16-bit value from a register (MSB first).
 * @param addr Starting register address.
 * @return The 16-bit value read.
 */
uint16_t Boardoza_MAX31865::readRegister16(uint8_t addr) {
  uint8_t buffer[2] = {0, 0};
  readRegisterN(addr, buffer, 2);
  uint16_t ret = buffer[0];
  ret <<= 8;
  ret |= buffer[1];
  return ret;
}

/**
 * @brief Read N bytes starting from a register address.
 * @param addr Register address.
 * @param buffer Pointer to the output buffer.
 * @param n Number of bytes to read.
 */
void Boardoza_MAX31865::readRegisterN(uint8_t addr, uint8_t buffer[], uint8_t n) {
  addr &= 0x7F; // Ensure read command (bit 7 is 0)
  spi_dev.write_then_read(&addr, 1, buffer, n);
}

/**
 * @brief Write an 8-bit value to a register.
 * @param addr Register address.
 * @param data Byte to write.
 */
void Boardoza_MAX31865::writeRegister8(uint8_t addr, uint8_t data) {
  addr |= 0x80; // Ensure write command (bit 7 is 1)
  uint8_t buffer[2] = {addr, data};
  spi_dev.write(buffer, 2);
}