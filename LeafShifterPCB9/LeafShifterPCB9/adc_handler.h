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
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Initialize SPI and ADC hardware
void initADC();

// Read raw 12-bit ADC value from specified channel (0 or 1)
uint16_t readADCRaw(uint8_t channel);

// Read voltage from specified channel (returns 0.0 to ADC_VREF)
float readADCVoltage(uint8_t channel);

#endif // ADC_HANDLER_H
