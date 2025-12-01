#ifndef GPIO_HANDLER_H
#define GPIO_HANDLER_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

//=============================================================================
// TCA9534 GPIO EXPANDER HANDLER
//=============================================================================
// Handles writing output patterns to TCA9534 I2C GPIO expander
// Supports output inversion for hardware compatibility

//-----------------------------------------------------------------------------
// TCA9534 REGISTER ADDRESSES
//-----------------------------------------------------------------------------

#define TCA9534_REG_INPUT       0x00    // Input port register (read-only)
#define TCA9534_REG_OUTPUT      0x01    // Output port register (read/write)
#define TCA9534_REG_POLARITY    0x02    // Polarity inversion register
#define TCA9534_REG_CONFIG      0x03    // Configuration register (0=output, 1=input)

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Initialize I2C and configure GPIO expander
void initGPIO();

// Write gear pattern to GPIO expander (handles inversion automatically)
void writeGPIOPattern(uint8_t gear);

// Write raw 8-bit value to GPIO expander (handles inversion automatically)
void writeGPIORaw(uint8_t value);

// Get current GPIO output value (for debugging)
uint8_t getCurrentGPIOOutput();

#endif // GPIO_HANDLER_H
