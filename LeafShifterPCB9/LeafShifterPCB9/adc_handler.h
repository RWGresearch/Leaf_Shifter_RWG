#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

//=============================================================================
// MCP3202 ADC HANDLER
//=============================================================================
// Handles reading analog values from MCP3202 12-bit dual-channel ADC
// Connected via SPI interface

//-----------------------------------------------------------------------------
// DUAL-INPUT MODE STRUCTURES
//-----------------------------------------------------------------------------

// Structure to hold dual paddle input readings
struct DualPaddleInput {
    uint16_t left_adc;      // Left paddle ADC value (0-4095)
    uint16_t right_adc;     // Right paddle ADC value (0-4095)
    bool left_pulled;       // True if left paddle is pulled (active)
    bool right_pulled;      // True if right paddle is pulled (active)
};

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Initialize SPI and ADC hardware
void initADC();

// Read raw 12-bit ADC value from specified channel (0 or 1)
uint16_t readADCRaw(uint8_t channel);

// Read voltage from specified channel (returns 0.0 to ADC_VREF)
float readADCVoltage(uint8_t channel);

// Read both paddle inputs for dual-input mode
DualPaddleInput readDualPaddleInputs();

#endif // ADC_HANDLER_H
