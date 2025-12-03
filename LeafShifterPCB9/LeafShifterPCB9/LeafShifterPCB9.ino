/*
 * LeafShifterPCB9* - Simplified Paddle Shifter Controller for Nissan Leaf
 *
 * REVISION: 2.4.0 - DUAL-INPUT MODE SUPPORT
 *
 * Simple logic:
 * 1. Read ADC paddle position (matrix mode OR dual-input mode)
 * 2. Match to gear (PARK, REVERSE, DRIVE, NEUTRAL, HOME)
 * 3. PARK SPECIAL: Both paddles pressed → immediate processing (bypasses debounce & lockout)
 * 4. DEBOUNCE: Wait 50ms for stable reading (prevents false triggers during transition)
 * 5. Pulse GPIO for that gear, then return to HOME
 * 6. NEUTRAL: Hold REVERSE position > 500ms → upgrades to NEUTRAL
 * 7. DRIVE/BRAKE: Toggles between DRIVE and BRAKE each trigger
 * 8. LOCKOUT: After gear change, paddle must return to HOME + 100ms delay
 * 9. WEB SERVER: WiFi AP "Leaf-Shifter" provides real-time debug at http://192.168.4.1 (only use when on USB power)
 * 10. SAFETY: GPIO initialized immediately after Serial (~30ms) for hardware protection
 *
 * Input Modes (selectable via USE_DUAL_INPUT_MODE flag in config.h):
 * - MATRIX MODE (default): Single resistor matrix input on ADC channel 0
 * - DUAL-INPUT MODE: Separate left/right paddle inputs on ADC channels 0 & 1
 *   - ~5V (ADC ~4095) = Home/neutral/not active
 *   - ~0V (ADC ~0)    = Pulled/active/triggered
 *
 * Hardware:
 * - ESP32-C3 Super Mini
 * - MCP3202T ADC (SPI)
 * - TCA9534 GPIO Expander at 0x39 (I2C, outputs only)
 *
 * Author: ~Russ Gries ~ RWGresearch.com
 * Built with love for god's glory.
 * Simplified from Leaf_control V7.12 for gear shifter only.
 * Date: 12/3/2025
 */

#include <Arduino.h>
#include "config.h"
#include "adc_handler.h"
#include "gpio_handler.h"
#include "web_server.h"

//=============================================================================
// STATE TRACKING
//=============================================================================

struct ShifterState {
    uint8_t current_gear;           // Current gear (PARK, REVERSE, DRIVE, NEUTRAL, HOME)
    uint8_t drive_brake_mode;       // MODE_DRIVE or MODE_BRAKE

    // GPIO pulse timing
    bool gpio_pulsing;              // Is GPIO currently pulsing?
    unsigned long gpio_start;       // When did pulse start?
    uint8_t gpio_gear;              // What gear is pulsing?

    // NEUTRAL hold timer (push REVERSE > 500ms → NEUTRAL)
    bool neutral_timing;            // Timing for NEUTRAL upgrade?
    unsigned long neutral_start;    // When did REVERSE push start?
    bool neutral_triggered;         // Already triggered NEUTRAL?

    // Gear change debounce (prevents false triggers during paddle transition)
    bool gear_pending;              // Is a gear change pending debounce?
    uint8_t pending_gear;           // What gear is pending?
    unsigned long pending_start;    // When did pending gear first appear?

    // Gear change lockout (debounce protection)
    bool gear_locked;               // Is gear changing currently locked?
    bool waiting_for_home;          // Waiting for paddle to return to HOME?
    unsigned long home_detected_time; // When was HOME position detected?
    unsigned long last_gear_change_time; // When did last gear change occur? (for PARK override)
};

ShifterState state;

// Debug output timing
unsigned long last_debug = 0;
uint8_t last_gpio = 0x00;

//=============================================================================
// SETUP
//=============================================================================

void setup() {
    // Initialize serial for debug output
    Serial.begin(SERIAL_BAUD);

    // CRITICAL: Initialize GPIO IMMEDIATELY to set hardware to safe HOME position
    initGPIO();

    // Short delay for serial monitor to connect (reduced from 500ms)
    delay(200);

#if USE_DUAL_INPUT_MODE
    Serial.println("\n\nLeafShifterPCB9 v2.4.0 - Dual-Input Mode\n");
    Serial.println("Input Mode: DUAL-INPUT (separate left/right paddles)");
    Serial.printf("Threshold: %d (below = pulled, above = home)\n\n", DUAL_INPUT_THRESHOLD);
#else
    Serial.println("\n\nLeafShifterPCB9 v2.4.0 - Matrix Mode\n");
    Serial.println("Input Mode: MATRIX (single resistor matrix input)\n");
#endif

    // Initialize remaining hardware (non-critical for safety)
    initADC();

    // Initialize web server (if enabled)
    if (ENABLE_WEB_SERVER) {
        initWebServer();
    }

    // Initialize state variables
    state.current_gear = GEAR_HOME;
    state.drive_brake_mode = MODE_DRIVE;
    state.gpio_pulsing = false;
    state.neutral_timing = false;
    state.neutral_triggered = false;
    state.gear_pending = false;
    state.pending_gear = GEAR_HOME;
    state.pending_start = 0;
    state.gear_locked = false;
    state.waiting_for_home = false;
    state.home_detected_time = 0;
    state.last_gear_change_time = 0;

    Serial.println("Ready!\n");
    delay(1000);
}

//=============================================================================
// MAIN LOOP
//=============================================================================

void loop() {
    // 1. Check if GPIO pulse is done (return to HOME)
    checkGPIOPulse();

#if USE_DUAL_INPUT_MODE
    // DUAL-INPUT MODE: Read both paddle inputs separately
    // 2. Read both paddle ADC channels
    DualPaddleInput paddle_inputs = readDualPaddleInputs();

    // 3. Match dual inputs to gear
    uint8_t requested_gear = matchDualInput(paddle_inputs);

    // 4. Check for NEUTRAL hold timer (REVERSE held > NEUTRAL_HOLD_TIME_DUAL)
    checkNeutralHold(requested_gear);

    // 5. Check gear debounce (waits for stable reading before processing)
    //    PARK bypasses debounce and lockout entirely (handled in checkGearDebounce)
    checkGearDebounce(requested_gear);

    // 6. Check and update gear lockout state
    checkGearLockout(requested_gear);

    // NOTE: processGear() is called from checkGearDebounce() after debounce confirms stable reading
    // NOTE: PARK is handled specially in checkGearDebounce() - bypasses both debounce and lockout

    // 7. Handle web server requests (if enabled)
    if (ENABLE_WEB_SERVER) {
        handleWebServer();
    }

    // 8. Debug output (every 500ms or on GPIO change)
    printDebugDual(paddle_inputs);

#else
    // MATRIX MODE: Read single resistor matrix input
    // 2. Read paddle ADC
    uint16_t adc = readADCRaw(ADC_CHANNEL_PADDLE);

    // 3. Match ADC to gear
    uint8_t requested_gear = matchADC(adc);

    // 4. Check for NEUTRAL hold timer (REVERSE held > 500ms)
    checkNeutralHold(requested_gear);

    // 5. Check gear debounce (waits for stable reading before processing)
    //    PARK bypasses debounce and lockout entirely (handled in checkGearDebounce)
    checkGearDebounce(requested_gear);

    // 6. Check and update gear lockout state
    checkGearLockout(requested_gear);

    // NOTE: processGear() is called from checkGearDebounce() after debounce confirms stable reading
    // NOTE: PARK is handled specially in checkGearDebounce() - bypasses both debounce and lockout

    // 7. Handle web server requests (if enabled)
    if (ENABLE_WEB_SERVER) {
        handleWebServer();
    }

    // 8. Debug output (every 500ms or on GPIO change)
    printDebug(adc);
#endif
}

//=============================================================================
// ADC MATCHING
//=============================================================================

uint8_t matchADC(uint16_t adc) {
    for (int i = 0; i < NUM_THRESHOLDS; i++) {
        if (adc >= PADDLE_THRESHOLDS[i].adc_min &&
            adc <= PADDLE_THRESHOLDS[i].adc_max) {
            return PADDLE_THRESHOLDS[i].gear_output;
        }
    }
    return GEAR_HOME;  // Default to HOME if no match
}

//=============================================================================
// DUAL-INPUT MATCHING
//=============================================================================

uint8_t matchDualInput(DualPaddleInput inputs) {
    // Both paddles pulled → PARK
    if (inputs.left_pulled && inputs.right_pulled) {
        return GEAR_PARK;
    }

    // Left paddle pulled only → REVERSE (can upgrade to NEUTRAL with hold timer)
    if (inputs.left_pulled && !inputs.right_pulled) {
        return GEAR_REVERSE;
    }

    // Right paddle pulled only → DRIVE/BRAKE
    if (!inputs.left_pulled && inputs.right_pulled) {
        return GEAR_DRIVE;
    }

    // Neither paddle pulled → HOME
    return GEAR_HOME;
}

//=============================================================================
// GPIO PULSE TIMING
//=============================================================================

void checkGPIOPulse() {
    if (!state.gpio_pulsing) return;

    unsigned long elapsed = millis() - state.gpio_start;
    unsigned long hold_time = getGPIOHoldTime(state.gpio_gear);

    if (elapsed >= hold_time) {
        // Pulse done → return to HOME
        writeGPIOPattern(GEAR_HOME);
        state.gpio_pulsing = false;
        Serial.println(">>> GPIO → HOME");
    }
}

void startGPIOPulse(uint8_t gear) {
    if (gear == GEAR_HOME) {
        // HOME doesn't pulse, just stays
        writeGPIOPattern(GEAR_HOME);
        state.gpio_pulsing = false;
        return;
    }

    // Start pulse
    state.gpio_pulsing = true;
    state.gpio_start = millis();
    state.gpio_gear = gear;
    writeGPIOPattern(gear);

    const char* name = getGearName(gear, state.drive_brake_mode);
    unsigned long hold = getGPIOHoldTime(gear);
    Serial.printf(">>> GPIO PULSE: %s (0x%02X, %lums)\n",
                  name, GEAR_PATTERNS[gear].gpio_pattern, hold);
}

//=============================================================================
// NEUTRAL HOLD TIMER
//=============================================================================

void checkNeutralHold(uint8_t requested_gear) {
    if (!ENABLE_NEUTRAL_HOLD) return;

    // If requesting REVERSE, start/continue timer
    if (requested_gear == GEAR_REVERSE) {
        if (!state.neutral_timing) {
            // Start timing
            state.neutral_timing = true;
            state.neutral_start = millis();
            state.neutral_triggered = false;
        } else if (!state.neutral_triggered) {
            // Check if 500ms elapsed
            unsigned long elapsed = millis() - state.neutral_start;
            if (elapsed >= NEUTRAL_HOLD_TIME) {
                // Upgrade to NEUTRAL!
                Serial.println(">>> NEUTRAL HOLD TRIGGERED (>500ms)");
                state.neutral_triggered = true;
                processGear(GEAR_NEUTRAL);
            }
        }
    } else {
        // Not requesting REVERSE → reset timer
        state.neutral_timing = false;
        state.neutral_triggered = false;
    }
}

//=============================================================================
// GEAR CHANGE DEBOUNCE
//=============================================================================

void checkGearDebounce(uint8_t requested_gear) {
    // PARK bypasses debounce (both paddles = clear intent, no transition values)
    if (requested_gear == GEAR_PARK) {
        // Cancel any pending gear (user changed their mind to PARK)
        if (state.gear_pending) {
            Serial.printf(">>> PARK: Cancelling pending %s\n",
                         GEAR_PATTERNS[state.pending_gear].name);
            state.gear_pending = false;
        }

        // Process PARK immediately (no debounce, bypasses lockout)
        if (!state.gpio_pulsing || state.gpio_gear != GEAR_PARK) {
            processGear(GEAR_PARK);
        }
        return;
    }

    if (!ENABLE_GEAR_DEBOUNCE) {
        // Debounce disabled, process immediately if not pulsing or locked
        if (!state.gpio_pulsing && !state.gear_locked) {
            processGear(requested_gear);
        }
        return;
    }

    // Ignore HOME requests
    if (requested_gear == GEAR_HOME) {
        // Reset debounce if paddle returned to HOME
        if (state.gear_pending) {
            state.gear_pending = false;
            Serial.println(">>> Debounce: Cancelled (returned to HOME)");
        }
        return;
    }

    // Check if this is a new gear request
    if (!state.gear_pending || requested_gear != state.pending_gear) {
        // Start new debounce period
        bool was_pending = state.gear_pending;
        state.gear_pending = true;
        state.pending_gear = requested_gear;
        state.pending_start = millis();

        if (was_pending) {
            Serial.printf(">>> Debounce: Changed to %s (restarting timer)\n",
                         GEAR_PATTERNS[requested_gear].name);
        } else {
            Serial.printf(">>> Debounce: Started for %s (%dms)\n",
                         GEAR_PATTERNS[requested_gear].name, GEAR_DEBOUNCE_MS);
        }
        return;
    }

    // Check if debounce period has elapsed
    unsigned long elapsed = millis() - state.pending_start;
    if (elapsed >= GEAR_DEBOUNCE_MS) {
        // Stable reading for required time, process gear change
        Serial.printf(">>> Debounce: Confirmed %s after %lums\n",
                     GEAR_PATTERNS[requested_gear].name, elapsed);
        state.gear_pending = false;

        // Process the gear change (only if not pulsing or locked)
        if (!state.gpio_pulsing && !state.gear_locked) {
            processGear(requested_gear);
        }
    }
}

//=============================================================================
// GEAR LOCKOUT (Debounce Protection)
//=============================================================================

void checkGearLockout(uint8_t requested_gear) {
    if (!ENABLE_GEAR_LOCKOUT) return;

    // If we're waiting for HOME and paddle has returned to HOME
    if (state.waiting_for_home && requested_gear == GEAR_HOME) {
        // Record the time we detected HOME (only once)
        if (state.home_detected_time == 0) {
            state.home_detected_time = millis();
            Serial.println(">>> Lockout: HOME detected, starting delay timer");
        }

        // Check if delay period has elapsed
        unsigned long elapsed = millis() - state.home_detected_time;
        if (elapsed >= GEAR_LOCKOUT_DELAY_MS) {
            // Unlock gear changes!
            state.gear_locked = false;
            state.waiting_for_home = false;
            state.home_detected_time = 0;
            Serial.printf(">>> Lockout: Released after %lums delay\n", elapsed);
        }
    }

    // If paddle moves away from HOME while waiting, reset the timer
    if (state.waiting_for_home && requested_gear != GEAR_HOME) {
        if (state.home_detected_time != 0) {
            state.home_detected_time = 0;
            Serial.println(">>> Lockout: Paddle moved away from HOME, resetting timer");
        }
    }
}

//=============================================================================
// GEAR PROCESSING
//=============================================================================

void processGear(uint8_t gear) {
    // Ignore HOME requests (we're already at HOME when not pulsing)
    if (gear == GEAR_HOME) return;

    // Ignore if already triggered NEUTRAL during this hold
    if (state.neutral_triggered && gear == GEAR_REVERSE) return;

    // PARK overrides lockout (both paddles = clear intent)
    bool bypass_lockout = (gear == GEAR_PARK);

    // Check lockout (unless PARK)
    if (!bypass_lockout && state.gear_locked && ENABLE_GEAR_LOCKOUT) {
        return;
    }

    // Handle DRIVE/BRAKE toggle
    if (gear == GEAR_DRIVE) {
        handleDriveBrake();
        return;
    }

    // Handle other gears
    if (gear != state.current_gear) {
        Serial.printf(">>> GEAR: %s → %s\n",
                     GEAR_PATTERNS[state.current_gear].name,
                     GEAR_PATTERNS[gear].name);
        state.current_gear = gear;
        startGPIOPulse(gear);

        // Record gear change timestamp (for PARK override timing)
        state.last_gear_change_time = millis();

        // Engage gear lockout
        if (ENABLE_GEAR_LOCKOUT) {
            state.gear_locked = true;
            state.waiting_for_home = true;
            state.home_detected_time = 0;
            Serial.println(">>> Lockout: ENGAGED (gear changed)");
        }
    }
}

void handleDriveBrake() {
    if (state.current_gear == GEAR_DRIVE) {
        // Already in DRIVE → toggle mode
        if (state.drive_brake_mode == MODE_DRIVE) {
            state.drive_brake_mode = MODE_BRAKE;
            Serial.println(">>> TOGGLE: DRIVE → BRAKE");
        } else {
            state.drive_brake_mode = MODE_DRIVE;
            Serial.println(">>> TOGGLE: BRAKE → DRIVE");
        }
    } else {
        // Coming from different gear → always start in DRIVE
        Serial.printf(">>> GEAR: %s → DRIVE\n", GEAR_PATTERNS[state.current_gear].name);
        state.current_gear = GEAR_DRIVE;
        state.drive_brake_mode = MODE_DRIVE;
    }

    startGPIOPulse(GEAR_DRIVE);

    // Record gear change timestamp (for PARK override timing)
    state.last_gear_change_time = millis();

    // Engage gear lockout
    if (ENABLE_GEAR_LOCKOUT) {
        state.gear_locked = true;
        state.waiting_for_home = true;
        state.home_detected_time = 0;
        Serial.println(">>> Lockout: ENGAGED (DRIVE/BRAKE changed)");
    }
}

//=============================================================================
// DEBUG OUTPUT
//=============================================================================

void printDebug(uint16_t adc) {
    if (!ENABLE_DEBUG_OUTPUT) return;

    uint8_t current_gpio = getCurrentGPIOOutput();
    bool gpio_changed = (current_gpio != last_gpio);
    bool time_elapsed = (millis() - last_debug >= DEBUG_INTERVAL_MS);

    if (!gpio_changed && !time_elapsed) return;

    last_debug = millis();
    last_gpio = current_gpio;

    // Header
    Serial.println("=== Paddle Shifter v2.3.3 ===");

    // ADC info
    float voltage = (adc / 4095.0) * 5.0;
    uint8_t matched = matchADC(adc);
    const char* desc = "No match";
    for (int i = 0; i < NUM_THRESHOLDS; i++) {
        if (adc >= PADDLE_THRESHOLDS[i].adc_min &&
            adc <= PADDLE_THRESHOLDS[i].adc_max) {
            desc = PADDLE_THRESHOLDS[i].description;
            break;
        }
    }
    Serial.printf("ADC: %4d (%.2fV) | %s\n", adc, voltage, desc);

    // Enhanced ADC threshold visualization (helps diagnose triggering issues)
    Serial.print("Thresholds: ");
    for (int i = 0; i < NUM_THRESHOLDS; i++) {
        bool is_match = (adc >= PADDLE_THRESHOLDS[i].adc_min &&
                        adc <= PADDLE_THRESHOLDS[i].adc_max);
        const char* gear_name = GEAR_PATTERNS[PADDLE_THRESHOLDS[i].gear_output].name;

        Serial.printf("%s[%d-%d]",
                     gear_name,
                     PADDLE_THRESHOLDS[i].adc_min,
                     PADDLE_THRESHOLDS[i].adc_max);

        if (is_match) Serial.print("←MATCH");
        if (i < NUM_THRESHOLDS - 1) Serial.print(" | ");
    }
    Serial.println();

    // Current state
    const char* gear_name = getGearName(state.current_gear, state.drive_brake_mode);
    Serial.printf("Gear: %s | GPIO: 0x%02X", gear_name, current_gpio);

    if (state.gpio_pulsing) {
        unsigned long elapsed = millis() - state.gpio_start;
        unsigned long total = getGPIOHoldTime(state.gpio_gear);
        Serial.printf(" (pulse %lu/%lums)", elapsed, total);
    } else {
        Serial.print(" (idle)");
    }
    Serial.println();

    // NEUTRAL hold timer
    if (state.neutral_timing && !state.neutral_triggered) {
        unsigned long elapsed = millis() - state.neutral_start;
        Serial.printf("NEUTRAL timer: %lu/%d ms\n", elapsed, NEUTRAL_HOLD_TIME);
    }

    // Gear lockout status
    if (ENABLE_GEAR_LOCKOUT && state.gear_locked) {
        if (state.waiting_for_home) {
            if (state.home_detected_time > 0) {
                unsigned long elapsed = millis() - state.home_detected_time;
                Serial.printf("Lockout: HOME delay %lu/%d ms\n", elapsed, GEAR_LOCKOUT_DELAY_MS);
            } else {
                Serial.println("Lockout: Waiting for HOME position");
            }
        }
    }

    // Uptime
    Serial.printf("Uptime: %02lu:%02lu:%02lu\n",
                 (millis() / 3600000) % 24,
                 (millis() / 60000) % 60,
                 (millis() / 1000) % 60);

    Serial.println("===========================\n");
}

void printDebugDual(DualPaddleInput inputs) {
    if (!ENABLE_DEBUG_OUTPUT) return;

    uint8_t current_gpio = getCurrentGPIOOutput();
    bool gpio_changed = (current_gpio != last_gpio);
    bool time_elapsed = (millis() - last_debug >= DEBUG_INTERVAL_MS);

    if (!gpio_changed && !time_elapsed) return;

    last_debug = millis();
    last_gpio = current_gpio;

    // Header
    Serial.println("=== Paddle Shifter v2.4.0 (DUAL-INPUT MODE) ===");

    // Dual paddle ADC info
    float left_voltage = (inputs.left_adc / 4095.0) * 5.0;
    float right_voltage = (inputs.right_adc / 4095.0) * 5.0;

    Serial.printf("Left Paddle:  ADC=%4d (%.2fV) %s\n",
                  inputs.left_adc, left_voltage,
                  inputs.left_pulled ? "[PULLED]" : "[HOME]");
    Serial.printf("Right Paddle: ADC=%4d (%.2fV) %s\n",
                  inputs.right_adc, right_voltage,
                  inputs.right_pulled ? "[PULLED]" : "[HOME]");

    // Paddle combination description
    if (inputs.left_pulled && inputs.right_pulled) {
        Serial.println("Input: Both Paddles → PARK");
    } else if (inputs.left_pulled && !inputs.right_pulled) {
        Serial.println("Input: Left Paddle → REVERSE (hold for NEUTRAL)");
    } else if (!inputs.left_pulled && inputs.right_pulled) {
        Serial.println("Input: Right Paddle → DRIVE/BRAKE");
    } else {
        Serial.println("Input: None → HOME");
    }

    // Current state
    const char* gear_name = getGearName(state.current_gear, state.drive_brake_mode);
    Serial.printf("Gear: %s | GPIO: 0x%02X", gear_name, current_gpio);

    if (state.gpio_pulsing) {
        unsigned long elapsed = millis() - state.gpio_start;
        unsigned long total = getGPIOHoldTime(state.gpio_gear);
        Serial.printf(" (pulse %lu/%lums)", elapsed, total);
    } else {
        Serial.print(" (idle)");
    }
    Serial.println();

    // NEUTRAL hold timer
    if (state.neutral_timing && !state.neutral_triggered) {
        unsigned long elapsed = millis() - state.neutral_start;
        Serial.printf("NEUTRAL timer: %lu/%d ms\n", elapsed, NEUTRAL_HOLD_TIME_DUAL);
    }

    // Gear lockout status
    if (ENABLE_GEAR_LOCKOUT && state.gear_locked) {
        if (state.waiting_for_home) {
            if (state.home_detected_time > 0) {
                unsigned long elapsed = millis() - state.home_detected_time;
                Serial.printf("Lockout: HOME delay %lu/%d ms\n", elapsed, GEAR_LOCKOUT_DELAY_MS);
            } else {
                Serial.println("Lockout: Waiting for HOME position");
            }
        }
    }

    // Uptime
    Serial.printf("Uptime: %02lu:%02lu:%02lu\n",
                 (millis() / 3600000) % 24,
                 (millis() / 60000) % 60,
                 (millis() / 1000) % 60);

    Serial.println("============================================\n");
}
