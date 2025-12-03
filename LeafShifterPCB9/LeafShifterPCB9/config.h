#ifndef CONFIG_H
#define CONFIG_H

//=============================================================================
// LEAF PADDLE SHIFTER CONFIGURATION - VERSION 2.4.0
// RWGresearch.com
// Built with love for god's glory.
// ~Russ Gries
//
// REVISION: 2.4.0
//
// This file contains ALL user-configurable settings.
// Edit values here to customize thresholds, timing, and behavior.
//
//=============================================================================

//-----------------------------------------------------------------------------
// HARDWARE PIN CONFIGURATION
//-----------------------------------------------------------------------------

// SPI pins for MCP3202 ADC
#define PIN_SPI_SCK     4       // SPI Clock
#define PIN_SPI_MISO    5       // SPI Master In Slave Out (data from ADC)
#define PIN_SPI_MOSI    6       // SPI Master Out Slave In
#define PIN_CS_ADC      0       // ADC Chip Select (active LOW)

// I2C pins for TCA9534 GPIO expander
#define PIN_I2C_SDA     8       // I2C Data
#define PIN_I2C_SCL     9       // I2C Clock
#define I2C_GPIO_ADDR   0x39    // TCA9534 I2C address (outputs only)

//-----------------------------------------------------------------------------
// ADC CONFIGURATION
//-----------------------------------------------------------------------------

// INPUT MODE SELECTION
// Set to true for dual-input mode (separate left/right paddle inputs)
// Set to false for matrix mode (single resistor matrix input for push and pull paddle input)
#define USE_DUAL_INPUT_MODE false  // Change to true to use dual-input paddle mode

#define ADC_CHANNEL_PADDLE  0   // Paddle input on MCP3202 Channel 0 (matrix mode)
#define ADC_CHANNEL_LEFT    0   // Left paddle on Channel 0 (dual-input mode)
#define ADC_CHANNEL_RIGHT   1   // Right paddle on Channel 1 (dual-input mode)
#define ADC_VREF           5.0  // ADC reference voltage (5V)
#define ADC_MAX_VALUE      4095 // 12-bit ADC maximum value

//-----------------------------------------------------------------------------
// GEAR ENUMERATION
//-----------------------------------------------------------------------------

enum GearPosition {
    GEAR_HOME = 0,      // Home/resting position
    GEAR_PARK = 1,      // Park gear
    GEAR_REVERSE = 2,   // Reverse gear
    GEAR_DRIVE = 3,     // Drive/Brake gear (shares same GPIO pattern)
    GEAR_NEUTRAL = 4    // Neutral gear
};

// Drive/Brake sub-state (DRIVE and BRAKE share GEAR_DRIVE position)
enum DriveBrakeMode {
    MODE_DRIVE = 0,     // Drive mode
    MODE_BRAKE = 1      // Brake mode
};

//-----------------------------------------------------------------------------
// PADDLE ADC THRESHOLDS (Channel 0 - Single Analog Input)
//-----------------------------------------------------------------------------
// Priority: First match wins! Checked top to bottom.
// To tune: Watch serial output and adjust min/max ranges based on your hardware
//
// ADC formula: adc_value = (voltage / 5.0) * 4095
//
struct PaddleThreshold {
    uint16_t adc_min;           // Minimum ADC value for this position
    uint16_t adc_max;           // Maximum ADC value for this position
    uint8_t  gear_output;       // Gear to output (GEAR_HOME, GEAR_PARK, etc.)
    const char* description;    // Human-readable description
};

// Paddle threshold table - EDIT THESE VALUES to match your hardware
// V1.5 Update: Reordered for correct priority (REVERSE before DRIVE)
// IMPORTANT: First match wins! Order matters!
const PaddleThreshold PADDLE_THRESHOLDS[] = {
    // ADC Min, Max,  Gear,           Description
    {  870,    1020,  GEAR_PARK,      "Both Pushed → PARK"                },
    {  1050,   1200,  GEAR_HOME,      "Right Pull + Left Push"            },
    {  1240,   1390,  GEAR_REVERSE,   "Left Push → REVERSE (hold=NEUTRAL)"},
    {  1490,   1640,  GEAR_HOME,      "Right Push + Left Pull"            },
    {  1780,   1930,  GEAR_REVERSE,   "Right Push → REVERSE (hold=NEUTRAL)"},
    {  2650,   2800,  GEAR_DRIVE,     "Right Pull → DRIVE/BRAKE"          },
    {  2850,   3000,  GEAR_DRIVE,     "Left Pull → DRIVE/BRAKE"           },
    {  3900,   4095,  GEAR_HOME,      "None (resting) → HOME"             }
};

const int NUM_THRESHOLDS = sizeof(PADDLE_THRESHOLDS) / sizeof(PaddleThreshold);

//-----------------------------------------------------------------------------
// DUAL-INPUT MODE THRESHOLDS
//-----------------------------------------------------------------------------
// For dual-input mode: Each paddle has its own ADC channel
// ~5V (ADC ~4095) = Home/neutral/not active
// ~0V (ADC ~0)    = Pulled/active/triggered
//
// Threshold for detecting when a paddle is pulled (active)
// ADC value BELOW this threshold = paddle is pulled
// ADC value ABOVE this threshold = paddle is at home
#define DUAL_INPUT_THRESHOLD    2048    // Midpoint - adjust if needed (50% of 4095)

// Timing for NEUTRAL: Left paddle held alone > NEUTRAL_HOLD_TIME_DUAL
#define NEUTRAL_HOLD_TIME_DUAL  500     // Hold time for NEUTRAL (500ms)

//-----------------------------------------------------------------------------
// GPIO OUTPUT PATTERNS (sent to TCA9534 at address 0x39)
//-----------------------------------------------------------------------------
// NOTE: These patterns are INVERTED before sending to GPIO expander!
//       Hardware requires inverted logic for proper operation.
//       Pattern shown here is BEFORE inversion.
//
struct GearPattern {
    uint8_t gpio_pattern;       // GPIO pattern BEFORE inversion (P7-P0)
    const char* name;           // Gear name for display
};

// GPIO pattern table - EDIT THESE VALUES to match your shifter hardware
const GearPattern GEAR_PATTERNS[] = {
    // Pattern, Name
    { 0x54,     "HOME"     },   // GEAR_HOME = 0    (01010100)
    { 0x45,     "PARK"     },   // GEAR_PARK = 1    (01000101)
    { 0x98,     "REVERSE"  },   // GEAR_REVERSE = 2 (10011000)
    { 0x32,     "DRIVE"    },   // GEAR_DRIVE = 3   (00110010) - BRAKE uses same
    { 0x5A,     "NEUTRAL"  }    // GEAR_NEUTRAL = 4 (01011010)
};

//-----------------------------------------------------------------------------
// GPIO TIMING CONFIGURATION
//-----------------------------------------------------------------------------
// How long to hold each GPIO pattern before returning to HOME

#define GPIO_HOLD_PARK          100     // PARK: 100ms pulse then → HOME
#define GPIO_HOLD_REVERSE       100     // REVERSE: 100ms pulse then → HOME
#define GPIO_HOLD_DRIVE         100     // DRIVE: 100ms pulse then → HOME
#define GPIO_HOLD_BRAKE         100     // BRAKE: 100ms pulse then → HOME (same as DRIVE)
#define GPIO_HOLD_NEUTRAL       1100    // NEUTRAL: 1.1 seconds then → HOME (car requirement)
#define GPIO_HOLD_HOME          0       // HOME: no timing (stays there)

//-----------------------------------------------------------------------------
// NEUTRAL HOLD TIMER
//-----------------------------------------------------------------------------

// Single paddle PUSH hold timer for NEUTRAL
// Quick push (<1500ms) → REVERSE (pulses 100ms, returns to HOME)
// Hold push (>1500ms) → NEUTRAL (pulses 1100ms, returns to HOME)
#define ENABLE_NEUTRAL_HOLD     true    // Enable NEUTRAL hold timer
#define NEUTRAL_HOLD_TIME       1500    // Hold time to trigger NEUTRAL (1500ms)

//-----------------------------------------------------------------------------
// GEAR CHANGE DEBOUNCE
//-----------------------------------------------------------------------------

// Prevents false triggers during paddle transition (while paddle is moving)
// Waits for paddle to be stable (same ADC reading) before processing gear change
// This ensures you're reading the fully-pressed position, not transitional values
#define ENABLE_GEAR_DEBOUNCE    true    // Enable gear change debounce
#define GEAR_DEBOUNCE_MS        50      // Wait time for stable reading (50ms)

//-----------------------------------------------------------------------------
// GEAR CHANGE LOCKOUT (Debounce Protection)
//-----------------------------------------------------------------------------

// Prevents multiple gear changes from a single paddle pull
// After a gear is triggered:
// 1. Further gear changes are locked out
// 2. Paddle must return to HOME position
// 3. A delay (GEAR_LOCKOUT_DELAY_MS) must elapse after HOME is detected
// 4. Then new gear changes are allowed again
#define ENABLE_GEAR_LOCKOUT     true    // Enable gear change lockout
#define GEAR_LOCKOUT_DELAY_MS   100     // Delay after HOME before allowing new gear (100ms)

// PARK Override Window: Allow PARK to bypass lockout within this time
// If both paddles are pushed within this window, PARK will trigger even if locked
// This allows out-of-sync paddle pushes to still trigger PARK
#define PARK_OVERRIDE_WINDOW_MS 300     // Time window for PARK override (300ms)

//-----------------------------------------------------------------------------
// WEB SERVER CONFIGURATION
//-----------------------------------------------------------------------------

#define ENABLE_WEB_SERVER       false               // Enable web debug interface (only use if powered by USB for debug only)
#define WIFI_SSID              "Leaf-Shifter"       // WiFi AP name
#define WIFI_PASSWORD          "LeafControl"        // WiFi AP password (min 8 chars)
#define WEB_SERVER_PORT         80                  // HTTP port
#define WIFI_CHANNEL            1                   // WiFi channel (1-13)
#define WIFI_HIDDEN             false               // Hide SSID broadcast
#define WIFI_MAX_CONNECTIONS    4                   // Max simultaneous connections

//-----------------------------------------------------------------------------
// RUNTIME CONFIGURATION
//-----------------------------------------------------------------------------

#define SERIAL_BAUD             115200  // Serial communication speed
#define LOOP_DELAY_MS           1       // Main loop delay (1ms = 1000Hz update)
#define DEBUG_INTERVAL_MS       500     // Print debug info every 500ms (half second)
#define SPI_CLOCK_SPEED         8000000 // 8MHz SPI clock for fast ADC reads

//-----------------------------------------------------------------------------
// FEATURE ENABLES
//-----------------------------------------------------------------------------

#define ENABLE_DEBUG_OUTPUT     true    // Print detailed debug info to serial
#define INVERT_GPIO_OUTPUT      true    // Invert GPIO outputs (hardware requirement)

//-----------------------------------------------------------------------------
// DRIVE/BRAKE CONFIGURATION
//-----------------------------------------------------------------------------

#define DRIVE_BRAKE_START_MODE  MODE_DRIVE  // Start in DRIVE mode (like original code)

//-----------------------------------------------------------------------------
// HELPER FUNCTION - Get GPIO hold time for a gear
//-----------------------------------------------------------------------------

inline unsigned long getGPIOHoldTime(uint8_t gear) {
    switch(gear) {
        case GEAR_PARK:     return GPIO_HOLD_PARK;
        case GEAR_REVERSE:  return GPIO_HOLD_REVERSE;
        case GEAR_DRIVE:    return GPIO_HOLD_DRIVE;    // BRAKE uses same timing
        case GEAR_NEUTRAL:  return GPIO_HOLD_NEUTRAL;
        case GEAR_HOME:     return GPIO_HOLD_HOME;
        default:            return 100;                 // Default 100ms
    }
}

//-----------------------------------------------------------------------------
// HELPER FUNCTION - Get gear name including Drive/Brake state
//-----------------------------------------------------------------------------

inline const char* getGearName(uint8_t gear, uint8_t drive_brake_mode) {
    if (gear == GEAR_DRIVE) {
        return (drive_brake_mode == MODE_DRIVE) ? "DRIVE" : "BRAKE";
    }
    return GEAR_PATTERNS[gear].name;
}

#endif // CONFIG_H
