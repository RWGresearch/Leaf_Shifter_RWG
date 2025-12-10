#include "web_server.h"
#include "config.h"
#include "adc_handler.h"
#include "gpio_handler.h"
#include <WiFi.h>
#include <WebServer.h>

// External reference to main program state (defined in .ino file)
// The ShifterState struct is defined in the main .ino file
struct ShifterState {
    uint8_t current_gear;
    uint8_t drive_brake_mode;
    bool gpio_pulsing;
    unsigned long gpio_start;
    uint8_t gpio_gear;
    bool neutral_timing;
    unsigned long neutral_start;
    bool neutral_triggered;
    bool gear_locked;
    bool waiting_for_home;
    unsigned long home_detected_time;
    unsigned long last_gear_change_time;
};

extern ShifterState state;
extern const int NUM_THRESHOLDS;

// Global web server instance
WebServer server(WEB_SERVER_PORT);

// Last ADC reading (updated from main loop)
uint16_t lastADC = 0;

//=============================================================================
// HTML PAGE (stored in PROGMEM to save RAM)
//=============================================================================

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Leaf Shifter Debug Console</title>
    <style>
        /* CSS Variables for Theme Management */
        :root {
            --bg-color: #0a0a0a;
            --card-bg: rgba(0, 255, 255, 0.03);
            --card-border: #00d4d4;
            --accent: #00ffff;
            --text: #ffffff;
            --text-muted: #b0b0b0;
            --status-active: #00ffff;
            --status-inactive: #404040;
            --status-warning: #ff9800;
            --shadow: rgba(0, 255, 255, 0.1);
            --gear-bg: rgba(0, 255, 255, 0.08);
        }

        [data-theme="day"] {
            --bg-color: #f5f5f5;
            --card-bg: #ffffff;
            --card-border: #00d4d4;
            --accent: #00b8b8;
            --text: #1a1a1a;
            --text-muted: #666666;
            --status-active: #00b8b8;
            --status-inactive: #d0d0d0;
            --status-warning: #ff9800;
            --shadow: rgba(0, 0, 0, 0.1);
            --gear-bg: rgba(0, 184, 184, 0.08);
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-color);
            color: var(--text);
            padding: 12px;
            min-height: 100vh;
            font-size: 14px;
            transition: background-color 0.3s ease, color 0.3s ease;
        }

        .container {
            max-width: 480px;
            margin: 0 auto;
        }

        .header {
            margin-bottom: 15px;
        }

        .header-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 8px;
        }

        .header h1 {
            font-size: 1.5em;
            margin: 0;
            color: var(--text);
        }

        .theme-btn {
            background: var(--card-bg);
            border: 1px solid var(--card-border);
            color: var(--text);
            font-size: 1.3em;
            width: 40px;
            height: 40px;
            border-radius: 8px;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: all 0.2s ease;
            padding: 0;
        }

        .theme-btn:active {
            transform: scale(0.95);
            background: var(--accent);
        }

        .header .subtitle {
            font-size: 0.75em;
            color: var(--text-muted);
            text-align: center;
        }

        .card {
            background: var(--card-bg);
            border: 1px solid var(--card-border);
            border-radius: 10px;
            padding: 12px;
            margin-bottom: 10px;
            box-shadow: 0 2px 8px var(--shadow);
            transition: background-color 0.3s ease, border-color 0.3s ease;
        }

        .card h2 {
            font-size: 1.1em;
            margin-bottom: 10px;
            color: var(--accent);
            border-bottom: 1px solid var(--card-border);
            padding-bottom: 8px;
        }

        .data-row {
            display: flex;
            justify-content: space-between;
            padding: 6px 0;
            border-bottom: 1px solid var(--card-border);
            align-items: center;
        }

        .data-row:last-child {
            border-bottom: none;
        }

        .data-label {
            font-weight: 500;
            color: var(--text-muted);
            font-size: 0.85em;
            display: flex;
            align-items: center;
        }

        .data-value {
            font-family: 'Courier New', monospace;
            font-weight: 600;
            font-size: 0.95em;
            color: var(--text);
        }

        .gear-display {
            text-align: center;
            font-size: 2em;
            font-weight: bold;
            padding: 15px;
            background: var(--gear-bg);
            border: 2px solid var(--accent);
            border-radius: 10px;
            margin: 0;
            color: var(--accent);
        }

        .threshold-item {
            padding: 6px 10px;
            margin: 4px 0;
            background: var(--card-bg);
            border: 1px solid var(--card-border);
            border-radius: 6px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            font-size: 0.85em;
        }

        .threshold-match {
            background: rgba(0, 255, 255, 0.1);
            border: 2px solid var(--accent);
            font-weight: 600;
        }

        .status-indicator {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 6px;
        }

        .status-active {
            background: var(--status-active);
            box-shadow: 0 0 8px var(--status-active);
        }

        .status-inactive {
            background: var(--status-inactive);
            box-shadow: 0 0 4px var(--status-inactive);
        }

        .status-warning {
            background: var(--status-warning);
            box-shadow: 0 0 8px var(--status-warning);
        }

        .update-indicator {
            text-align: center;
            padding: 8px;
            font-size: 0.75em;
            color: var(--text-muted);
        }

        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        .updating {
            animation: pulse 1s ease-in-out infinite;
        }

        /* Desktop optimization */
        @media (min-width: 769px) {
            body {
                padding: 20px;
                font-size: 16px;
            }
            .container {
                max-width: 600px;
            }
            .header h1 {
                font-size: 2em;
            }
            .card {
                padding: 16px;
                margin-bottom: 15px;
            }
            .gear-display {
                font-size: 2.5em;
                padding: 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div class="header-row">
                <h1>🚗 Leaf Shifter</h1>
                <button id="themeToggle" class="theme-btn">☀️</button>
            </div>
            <div class="subtitle" id="modeSubtitle">Debug Console v2.5.0</div>
        </div>

        <!-- Current Gear Display -->
        <div class="card">
            <div class="gear-display" id="gearDisplay">---</div>
        </div>

        <!-- ADC & GPIO Data -->
        <div class="card">
            <h2>Sensor Data</h2>
            <!-- Matrix Mode Display -->
            <div id="matrixModeData">
                <div class="data-row">
                    <span class="data-label">ADC Reading:</span>
                    <span class="data-value" id="adcValue">0</span>
                </div>
                <div class="data-row">
                    <span class="data-label">Voltage:</span>
                    <span class="data-value" id="voltageValue">0.00V</span>
                </div>
                <div class="data-row">
                    <span class="data-label">GPIO Output:</span>
                    <span class="data-value" id="gpioValue">0x00</span>
                </div>
            </div>
            <!-- Dual-Input Mode Display -->
            <div id="dualInputModeData" style="display:none;">
                <div class="data-row">
                    <span class="data-label">Left Paddle ADC:</span>
                    <span class="data-value" id="leftADC">0</span>
                </div>
                <div class="data-row">
                    <span class="data-label">Left Voltage:</span>
                    <span class="data-value" id="leftVoltage">0.00V</span>
                </div>
                <div class="data-row">
                    <span class="data-label">
                        <span class="status-indicator" id="leftStateIndicator"></span>
                        Left State:
                    </span>
                    <span class="data-value" id="leftState">HOME</span>
                </div>
                <div class="data-row">
                    <span class="data-label">Right Paddle ADC:</span>
                    <span class="data-value" id="rightADC">0</span>
                </div>
                <div class="data-row">
                    <span class="data-label">Right Voltage:</span>
                    <span class="data-value" id="rightVoltage">0.00V</span>
                </div>
                <div class="data-row">
                    <span class="data-label">
                        <span class="status-indicator" id="rightStateIndicator"></span>
                        Right State:
                    </span>
                    <span class="data-value" id="rightState">HOME</span>
                </div>
                <div class="data-row">
                    <span class="data-label">Pull Threshold:</span>
                    <span class="data-value" id="dualThreshold">2048</span>
                </div>
                <div class="data-row">
                    <span class="data-label">GPIO Output:</span>
                    <span class="data-value" id="gpioValueDual">0x00</span>
                </div>
            </div>
        </div>

        <!-- System Status -->
        <div class="card">
            <h2>System Status</h2>
            <div class="data-row">
                <span class="data-label">
                    <span class="status-indicator" id="lockIndicator"></span>
                    Gear Lockout:
                </span>
                <span class="data-value" id="lockStatus">Unlocked</span>
            </div>
            <div class="data-row">
                <span class="data-label">
                    <span class="status-indicator" id="pulseIndicator"></span>
                    GPIO Pulsing:
                </span>
                <span class="data-value" id="pulseStatus">Idle</span>
            </div>
            <div class="data-row">
                <span class="data-label">
                    <span class="status-indicator" id="neutralIndicator"></span>
                    NEUTRAL Timer:
                </span>
                <span class="data-value" id="neutralStatus">Inactive</span>
            </div>
        </div>

        <!-- Threshold Ranges (Matrix Mode Only) -->
        <div class="card" id="thresholdCard">
            <h2>Threshold Ranges</h2>
            <div id="thresholdList"></div>
        </div>

        <!-- System Info -->
        <div class="card">
            <h2>System Info</h2>
            <div class="data-row">
                <span class="data-label">Uptime:</span>
                <span class="data-value" id="uptimeValue">00:00:00</span>
            </div>
            <div class="data-row">
                <span class="data-label">IP Address:</span>
                <span class="data-value">192.168.4.1</span>
            </div>
            <div class="data-row">
                <span class="data-label">SSID:</span>
                <span class="data-value">Leaf-Shifter</span>
            </div>
        </div>

        <div class="update-indicator" id="updateIndicator">
            <span class="updating">● </span>Updating...
        </div>
    </div>

    <script>
        // ==================== THEME MANAGEMENT ====================

        function setTheme(theme) {
            document.body.setAttribute('data-theme', theme);
            localStorage.setItem('leafShifterTheme', theme);
            updateThemeButton(theme);
        }

        function toggleTheme() {
            const currentTheme = document.body.getAttribute('data-theme') || 'night';
            const newTheme = currentTheme === 'night' ? 'day' : 'night';
            setTheme(newTheme);
        }

        function updateThemeButton(theme) {
            const btn = document.getElementById('themeToggle');
            if (theme === 'night') {
                btn.textContent = '☀️';  // Show sun when in night mode (switch to day)
            } else {
                btn.textContent = '🌙';  // Show moon when in day mode (switch to night)
            }
        }

        function loadTheme() {
            const savedTheme = localStorage.getItem('leafShifterTheme') || 'night';
            setTheme(savedTheme);
        }

        // Load theme immediately before rendering
        loadTheme();

        // Add theme toggle event listener
        document.addEventListener('DOMContentLoaded', function() {
            document.getElementById('themeToggle').addEventListener('click', toggleTheme);
        });

        // ==================== DATA UPDATE ====================

        // Fetch data from server and update display
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    // Update mode-specific subtitle
                    const modeName = data.input_mode === 'dual' ? 'Dual-Input Mode' : 'Matrix Mode';
                    document.getElementById('modeSubtitle').textContent =
                        'Debug Console v2.4.0 - ' + modeName;

                    // Show/hide appropriate sensor data section
                    if (data.input_mode === 'dual') {
                        // DUAL-INPUT MODE
                        document.getElementById('matrixModeData').style.display = 'none';
                        document.getElementById('dualInputModeData').style.display = 'block';
                        document.getElementById('thresholdCard').style.display = 'none';

                        // Update dual-input data
                        document.getElementById('leftADC').textContent = data.left_adc;
                        document.getElementById('leftVoltage').textContent = data.left_voltage.toFixed(2) + 'V';
                        document.getElementById('leftState').textContent = data.left_pulled ? 'PULLED' : 'HOME';

                        // Update left paddle indicator
                        const leftInd = document.getElementById('leftStateIndicator');
                        leftInd.className = 'status-indicator ' +
                            (data.left_pulled ? 'status-active' : 'status-inactive');

                        document.getElementById('rightADC').textContent = data.right_adc;
                        document.getElementById('rightVoltage').textContent = data.right_voltage.toFixed(2) + 'V';
                        document.getElementById('rightState').textContent = data.right_pulled ? 'PULLED' : 'HOME';

                        // Update right paddle indicator
                        const rightInd = document.getElementById('rightStateIndicator');
                        rightInd.className = 'status-indicator ' +
                            (data.right_pulled ? 'status-active' : 'status-inactive');

                        document.getElementById('dualThreshold').textContent = data.threshold;
                        document.getElementById('gpioValueDual').textContent = data.gpio;

                    } else {
                        // MATRIX MODE
                        document.getElementById('matrixModeData').style.display = 'block';
                        document.getElementById('dualInputModeData').style.display = 'none';
                        document.getElementById('thresholdCard').style.display = 'block';

                        // Update matrix mode data
                        document.getElementById('adcValue').textContent = data.adc;
                        document.getElementById('voltageValue').textContent = data.voltage.toFixed(2) + 'V';
                        document.getElementById('gpioValue').textContent = data.gpio;

                        // Update thresholds
                        let thresholdHTML = '';
                        data.thresholds.forEach(t => {
                            const matchClass = t.match ? 'threshold-match' : '';
                            const matchIndicator = t.match ? ' ← MATCH' : '';
                            thresholdHTML += `
                                <div class="threshold-item ${matchClass}">
                                    <span>${t.name}</span>
                                    <span>[${t.min}-${t.max}]${matchIndicator}</span>
                                </div>
                            `;
                        });
                        document.getElementById('thresholdList').innerHTML = thresholdHTML;
                    }

                    // Update gear display (common to both modes)
                    document.getElementById('gearDisplay').textContent = data.gear;

                    // Update lockout status (common to both modes)
                    const lockInd = document.getElementById('lockIndicator');
                    if (data.locked) {
                        lockInd.className = 'status-indicator status-inactive';
                        document.getElementById('lockStatus').textContent =
                            data.waiting_home ? 'Locked (Waiting HOME)' : 'Locked';
                    } else {
                        lockInd.className = 'status-indicator status-active';
                        document.getElementById('lockStatus').textContent = 'Unlocked';
                    }

                    // Update pulse status (common to both modes)
                    const pulseInd = document.getElementById('pulseIndicator');
                    if (data.pulsing) {
                        pulseInd.className = 'status-indicator status-active';
                        document.getElementById('pulseStatus').textContent = 'Active';
                    } else {
                        pulseInd.className = 'status-indicator status-inactive';
                        document.getElementById('pulseStatus').textContent = 'Idle';
                    }

                    // Update neutral timer (common to both modes)
                    const neutralInd = document.getElementById('neutralIndicator');
                    if (data.neutral_timing) {
                        neutralInd.className = 'status-indicator status-warning';
                        document.getElementById('neutralStatus').textContent = 'Timing...';
                    } else {
                        neutralInd.className = 'status-indicator status-inactive';
                        document.getElementById('neutralStatus').textContent = 'Inactive';
                    }

                    // Update uptime (common to both modes)
                    const hours = Math.floor(data.uptime_sec / 3600);
                    const minutes = Math.floor((data.uptime_sec % 3600) / 60);
                    const seconds = data.uptime_sec % 60;
                    document.getElementById('uptimeValue').textContent =
                        `${hours.toString().padStart(2,'0')}:${minutes.toString().padStart(2,'0')}:${seconds.toString().padStart(2,'0')}`;
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                });
        }

        // Update every 200ms for smooth real-time feel
        setInterval(updateData, 200);

        // Initial update
        updateData();
    </script>
</body>
</html>
)rawliteral";

//=============================================================================
// JSON DATA GENERATION
//=============================================================================

String getStateJSON() {
    // Get gear name
    const char* gearName = getGearName(state.current_gear, state.drive_brake_mode);

    // Get GPIO output
    uint8_t gpio = getCurrentGPIOOutput();

    // Build JSON string
    String json = "{";

#if USE_DUAL_INPUT_MODE
    // DUAL-INPUT MODE: Read both paddle inputs
    DualPaddleInput inputs = readDualPaddleInputs();
    float left_voltage = (inputs.left_adc / 4095.0) * 5.0;
    float right_voltage = (inputs.right_adc / 4095.0) * 5.0;

    // Input mode identifier
    json += "\"input_mode\":\"dual\",";

    // Left paddle data
    json += "\"left_adc\":" + String(inputs.left_adc) + ",";
    json += "\"left_voltage\":" + String(left_voltage, 2) + ",";
    json += "\"left_pulled\":" + String(inputs.left_pulled ? "true" : "false") + ",";

    // Right paddle data
    json += "\"right_adc\":" + String(inputs.right_adc) + ",";
    json += "\"right_voltage\":" + String(right_voltage, 2) + ",";
    json += "\"right_pulled\":" + String(inputs.right_pulled ? "true" : "false") + ",";

    // Threshold
    json += "\"threshold\":" + String(DUAL_INPUT_THRESHOLD) + ",";

#else
    // MATRIX MODE: Read single ADC channel
    lastADC = readADCRaw(ADC_CHANNEL_PADDLE);
    float voltage = (lastADC / 4095.0) * 5.0;

    // Input mode identifier
    json += "\"input_mode\":\"matrix\",";

    // ADC data
    json += "\"adc\":" + String(lastADC) + ",";
    json += "\"voltage\":" + String(voltage, 2) + ",";
#endif

    // Gear and GPIO (common to both modes)
    json += "\"gear\":\"" + String(gearName) + "\",";
    json += "\"gpio\":\"0x" + String(gpio, HEX) + "\",";

    // Status flags (common to both modes)
    json += "\"locked\":" + String(state.gear_locked ? "true" : "false") + ",";
    json += "\"waiting_home\":" + String(state.waiting_for_home ? "true" : "false") + ",";
    json += "\"pulsing\":" + String(state.gpio_pulsing ? "true" : "false") + ",";
    json += "\"neutral_timing\":" + String(state.neutral_timing ? "true" : "false") + ",";

    // Uptime (common to both modes)
    unsigned long uptime_sec = millis() / 1000;
    json += "\"uptime_sec\":" + String(uptime_sec);

#if !USE_DUAL_INPUT_MODE
    // Threshold data (matrix mode only)
    json += ",\"thresholds\":[";
    for (int i = 0; i < NUM_THRESHOLDS; i++) {
        bool is_match = (lastADC >= PADDLE_THRESHOLDS[i].adc_min &&
                        lastADC <= PADDLE_THRESHOLDS[i].adc_max);

        const char* gear_name = GEAR_PATTERNS[PADDLE_THRESHOLDS[i].gear_output].name;

        json += "{";
        json += "\"name\":\"" + String(gear_name) + "\",";
        json += "\"min\":" + String(PADDLE_THRESHOLDS[i].adc_min) + ",";
        json += "\"max\":" + String(PADDLE_THRESHOLDS[i].adc_max) + ",";
        json += "\"match\":" + String(is_match ? "true" : "false");
        json += "}";

        if (i < NUM_THRESHOLDS - 1) json += ",";
    }
    json += "]";
#endif

    json += "}";

    return json;
}

//=============================================================================
// WEB SERVER HANDLERS
//=============================================================================

// Handler for root page "/"
void handleRoot() {
    server.send_P(200, "text/html", HTML_PAGE);
}

// Handler for JSON data endpoint "/data"
void handleData() {
    String json = getStateJSON();
    server.send(200, "application/json", json);
}

//=============================================================================
// WEB SERVER INITIALIZATION
//=============================================================================

void initWebServer() {
    Serial.println("=== Web Server Initialization ===");

#if USE_DUAL_INPUT_MODE
    Serial.println("Input Mode: DUAL-INPUT (separate left/right paddles)");
#else
    Serial.println("Input Mode: MATRIX (single resistor matrix)");
#endif

    // Configure WiFi AP
    Serial.printf("Creating WiFi AP: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, WIFI_HIDDEN, WIFI_MAX_CONNECTIONS);

    // Get IP address
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Setup routes
    server.on("/", handleRoot);
    server.on("/data", handleData);

    // Start server
    server.begin();
    Serial.println("Web server started!");
    Serial.printf("Access dashboard at: http://%s\n", IP.toString().c_str());
    Serial.println("=================================\n");
}

//=============================================================================
// WEB SERVER UPDATE (call from main loop)
//=============================================================================

void handleWebServer() {
    server.handleClient();
}
