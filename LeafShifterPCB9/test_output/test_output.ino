/*
 * GPIO Output Test - Hardware Verification
 *
 * REVISION: 1.0
 *
 * This simple test cycles through all gear positions to verify:
 * - TCA9534 GPIO expander is working
 * - GPIO patterns are correct
 * - Shifter hardware responds to commands
 *
 * Sequence (repeats forever):
 * HOME → PARK (100ms) → HOME → REVERSE (100ms) → HOME →
 * DRIVE (100ms) → HOME → NEUTRAL (1100ms) → HOME → repeat
 *
 * 3 seconds between each gear change
 */

#include <Wire.h>

//=============================================================================
// CONFIGURATION
//=============================================================================

// I2C Configuration
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9
#define I2C_GPIO_ADDR   0x39    // TCA9534 address

// TCA9534 Registers
#define TCA9534_REG_OUTPUT      0x01
#define TCA9534_REG_CONFIG      0x03

// Serial
#define SERIAL_BAUD     115200

// Timing
#define DELAY_BETWEEN_GEARS     3000    // 3 seconds between gear changes
#define GPIO_PULSE_STANDARD     100     // 100ms for most gears
#define GPIO_PULSE_NEUTRAL      1100    // 1.1 seconds for NEUTRAL

// GPIO Patterns (BEFORE inversion)
#define PATTERN_HOME        0x54    // 01010100
#define PATTERN_PARK        0x45    // 01000101
#define PATTERN_REVERSE     0x98    // 10011000
#define PATTERN_DRIVE       0x32    // 00110010
#define PATTERN_NEUTRAL     0x5A    // 01011010

// Inversion (your hardware requires inverted output)
#define INVERT_OUTPUT       true

//=============================================================================
// SETUP
//=============================================================================

void setup() {
    // Initialize Serial
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("GPIO Output Test - Hardware Verification");
    Serial.println("========================================\n");

    // Initialize I2C
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);  // 400kHz fast mode

    // Configure TCA9534 - all pins as outputs
    Wire.beginTransmission(I2C_GPIO_ADDR);
    Wire.write(TCA9534_REG_CONFIG);
    Wire.write(0x00);  // All outputs
    uint8_t result = Wire.endTransmission();

    if (result != 0) {
        Serial.printf("ERROR: TCA9534 not found at 0x%02X!\n", I2C_GPIO_ADDR);
        Serial.println("Check I2C wiring and address!");
        while(1) { delay(1000); }  // Halt
    }

    Serial.printf("TCA9534 initialized at 0x%02X\n", I2C_GPIO_ADDR);
    Serial.println("All pins configured as outputs\n");

    // Start in HOME position
    writeGPIO(PATTERN_HOME, "HOME");
    Serial.println("Starting test sequence...\n");
    delay(2000);
}

//=============================================================================
// MAIN LOOP
//=============================================================================

void loop() {
    Serial.println("========================================");
    Serial.println("TEST CYCLE START");
    Serial.println("========================================\n");

    // Test sequence: HOME → GEAR → HOME → GEAR → HOME...

    // Test PARK
    testGear("PARK", PATTERN_PARK, GPIO_PULSE_STANDARD);

    // Test REVERSE
    testGear("REVERSE", PATTERN_REVERSE, GPIO_PULSE_STANDARD);

    // Test DRIVE
    testGear("DRIVE", PATTERN_DRIVE, GPIO_PULSE_STANDARD);

    // Test NEUTRAL (longer pulse)
    testGear("NEUTRAL", PATTERN_NEUTRAL, GPIO_PULSE_NEUTRAL);

    Serial.println("========================================");
    Serial.println("TEST CYCLE COMPLETE - Restarting...");
    Serial.println("========================================\n");
    delay(3000);
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

/**
 * Test a gear: Output pattern, hold, return to HOME
 */
void testGear(const char* name, uint8_t pattern, unsigned long hold_time) {
    // Output gear pattern
    Serial.printf("→ Testing %s\n", name);
    writeGPIO(pattern, name);

    // Hold for specified time
    Serial.printf("  Holding for %lu ms...\n", hold_time);
    delay(hold_time);

    // Return to HOME
    Serial.println("  Returning to HOME");
    writeGPIO(PATTERN_HOME, "HOME");

    // Wait between tests
    Serial.printf("  Waiting %d seconds...\n\n", DELAY_BETWEEN_GEARS / 1000);
    delay(DELAY_BETWEEN_GEARS);
}

/**
 * Write pattern to GPIO expander
 */
void writeGPIO(uint8_t pattern, const char* name) {
    // Apply inversion if needed
    uint8_t output = INVERT_OUTPUT ? ~pattern : pattern;

    // Write to TCA9534
    Wire.beginTransmission(I2C_GPIO_ADDR);
    Wire.write(TCA9534_REG_OUTPUT);
    Wire.write(output);
    uint8_t result = Wire.endTransmission();

    if (result != 0) {
        Serial.printf("  ERROR: Failed to write GPIO (error %d)\n", result);
        return;
    }

    // Show what was written
    Serial.printf("  GPIO OUTPUT: %s = 0x%02X", name, pattern);
    if (INVERT_OUTPUT) {
        Serial.printf(" → 0x%02X (inverted)", output);
    }
    Serial.printf(" = ");
    printBinary(output);
    Serial.println();
}

/**
 * Print binary representation
 */
void printBinary(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        Serial.print((value >> i) & 1);
        if (i == 4) Serial.print(" ");  // Space in middle for readability
    }
}
