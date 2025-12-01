#include "gpio_handler.h"

//=============================================================================
// TCA9534 GPIO EXPANDER HANDLER IMPLEMENTATION
//=============================================================================

// Track current GPIO output state
static uint8_t current_gpio_output = 0x00;

/**
 * Initialize I2C interface and configure TCA9534 GPIO expander
 * Sets all pins as outputs and initializes to HOME position
 */
void initGPIO() {
    // Initialize I2C bus
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);  // 400kHz I2C fast mode

    Serial.printf("GPIO: Initializing TCA9534 at address 0x%02X\n", I2C_GPIO_ADDR);

    // Configure all pins as outputs (0x00 = all outputs)
    Wire.beginTransmission(I2C_GPIO_ADDR);
    Wire.write(TCA9534_REG_CONFIG);
    Wire.write(0x00);  // All pins as outputs
    uint8_t result = Wire.endTransmission();

    if (result != 0) {
        Serial.printf("GPIO ERROR: Failed to configure TCA9534 (error %d)\n", result);
        Serial.println("GPIO ERROR: Check I2C connections and address!");
        return;
    }

    // Set initial output to HOME position
    writeGPIOPattern(GEAR_HOME);

    Serial.println("GPIO: TCA9534 initialized successfully (all pins = outputs)");
    Serial.printf("GPIO: Initial position = HOME (0x%02X", GEAR_PATTERNS[GEAR_HOME].gpio_pattern);
    if (INVERT_GPIO_OUTPUT) {
        Serial.printf(" → 0x%02X inverted", ~GEAR_PATTERNS[GEAR_HOME].gpio_pattern);
    }
    Serial.println(")");
}

/**
 * Write gear pattern to GPIO expander
 * Automatically handles inversion if enabled
 *
 * @param gear Gear position (GEAR_HOME, GEAR_PARK, etc.)
 */
void writeGPIOPattern(uint8_t gear) {
    if (gear >= 5) {
        Serial.printf("GPIO ERROR: Invalid gear %d\n", gear);
        return;
    }

    uint8_t pattern = GEAR_PATTERNS[gear].gpio_pattern;
    writeGPIORaw(pattern);
}

/**
 * Write raw 8-bit value to GPIO expander
 * Automatically handles inversion if enabled
 *
 * @param value 8-bit pattern to write (before inversion)
 */
void writeGPIORaw(uint8_t value) {
    // Apply inversion if enabled
    uint8_t output_value = INVERT_GPIO_OUTPUT ? ~value : value;

    // Write to TCA9534 output register
    Wire.beginTransmission(I2C_GPIO_ADDR);
    Wire.write(TCA9534_REG_OUTPUT);
    Wire.write(output_value);
    uint8_t result = Wire.endTransmission();

    if (result != 0) {
        Serial.printf("GPIO ERROR: Failed to write to TCA9534 (error %d)\n", result);
        return;
    }

    // Store current output state
    current_gpio_output = output_value;
}

/**
 * Get current GPIO output value
 *
 * @return Current 8-bit output value (after inversion if enabled)
 */
uint8_t getCurrentGPIOOutput() {
    return current_gpio_output;
}
