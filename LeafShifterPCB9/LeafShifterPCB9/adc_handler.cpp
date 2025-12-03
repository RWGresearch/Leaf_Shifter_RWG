#include "adc_handler.h"

//=============================================================================
// MCP3202 ADC HANDLER IMPLEMENTATION
//=============================================================================

/**
 * Initialize SPI interface for MCP3202 ADC
 * Sets up SPI pins and configures SPI communication
 */
void initADC() {
    // Configure ADC chip select pin
    pinMode(PIN_CS_ADC, OUTPUT);
    digitalWrite(PIN_CS_ADC, HIGH);  // Deselect ADC initially

    // Initialize SPI with 8MHz clock for fast readings
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_CS_ADC);
    SPI.beginTransaction(SPISettings(SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE0));

    Serial.println("ADC: MCP3202 initialized (8MHz SPI, 12-bit)");
}

/**
 * Read raw 12-bit value from MCP3202 ADC
 *
 * @param channel ADC channel to read (0 or 1)
 * @return Raw 12-bit ADC value (0-4095)
 */
uint16_t readADCRaw(uint8_t channel) {
    if (channel > 1) {
        Serial.printf("ADC ERROR: Invalid channel %d (must be 0 or 1)\n", channel);
        return 0;
    }

    // MCP3202 requires 3-byte SPI sequence for 12-bit conversion
    digitalWrite(PIN_CS_ADC, LOW);  // Select ADC

    // Byte 1: Start bit (0x01)
    SPI.transfer(0x01);

    // Byte 2: Channel selection and receive MSB
    // Channel 0: 0x80 (single-ended CH0)
    // Channel 1: 0xC0 (single-ended CH1)
    uint8_t msb = SPI.transfer(channel == 0 ? 0x80 : 0xC0);

    // Byte 3: Receive LSB
    uint8_t lsb = SPI.transfer(0x00);

    digitalWrite(PIN_CS_ADC, HIGH);  // Deselect ADC

    // Combine 12-bit result: 4 MSB bits from byte 2 + 8 LSB bits from byte 3
    uint16_t result = ((msb & 0x0F) << 8) | lsb;

    return result;
}

/**
 * Read voltage from MCP3202 ADC channel
 *
 * @param channel ADC channel to read (0 or 1)
 * @return Voltage reading (0.0 to ADC_VREF volts)
 */
float readADCVoltage(uint8_t channel) {
    uint16_t raw = readADCRaw(channel);

    // Convert 12-bit ADC value to voltage
    // Formula: voltage = (adc_value / 4095) * reference_voltage
    return (raw / (float)ADC_MAX_VALUE) * ADC_VREF;
}

/**
 * Read both paddle inputs for dual-input mode
 *
 * In dual-input mode:
 * - Each paddle is connected to its own ADC channel
 * - ~5V (ADC ~4095) = Home/neutral/not active
 * - ~0V (ADC ~0)    = Pulled/active/triggered
 *
 * @return DualPaddleInput structure with both paddle states
 */
DualPaddleInput readDualPaddleInputs() {
    DualPaddleInput inputs;

    // Read both ADC channels
    inputs.left_adc = readADCRaw(ADC_CHANNEL_LEFT);
    inputs.right_adc = readADCRaw(ADC_CHANNEL_RIGHT);

    // Determine if paddles are pulled (active)
    // Pulled = ADC value BELOW threshold (closer to 0V)
    // Home = ADC value ABOVE threshold (closer to 5V)
    inputs.left_pulled = (inputs.left_adc < DUAL_INPUT_THRESHOLD);
    inputs.right_pulled = (inputs.right_adc < DUAL_INPUT_THRESHOLD);

    return inputs;
}
