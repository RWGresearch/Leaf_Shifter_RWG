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
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #fff;
            padding: 20px;
            min-height: 100vh;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .header .subtitle {
            font-size: 1.1em;
            opacity: 0.9;
        }
        .card {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 32px 0 rgba(31, 38, 135, 0.37);
            border: 1px solid rgba(255, 255, 255, 0.18);
        }
        .card h2 {
            font-size: 1.5em;
            margin-bottom: 15px;
            color: #fff;
            border-bottom: 2px solid rgba(255,255,255,0.3);
            padding-bottom: 10px;
        }
        .data-row {
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        .data-row:last-child {
            border-bottom: none;
        }
        .data-label {
            font-weight: 600;
            opacity: 0.9;
        }
        .data-value {
            font-family: 'Courier New', monospace;
            font-weight: bold;
            font-size: 1.1em;
        }
        .gear-display {
            text-align: center;
            font-size: 3em;
            font-weight: bold;
            padding: 20px;
            background: rgba(255,255,255,0.2);
            border-radius: 10px;
            margin: 15px 0;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        .threshold-item {
            padding: 8px 12px;
            margin: 5px 0;
            background: rgba(255,255,255,0.05);
            border-radius: 8px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .threshold-match {
            background: rgba(76, 175, 80, 0.3);
            border: 2px solid #4CAF50;
            font-weight: bold;
        }
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        .status-active {
            background: #4CAF50;
            box-shadow: 0 0 10px #4CAF50;
        }
        .status-inactive {
            background: #f44336;
            box-shadow: 0 0 10px #f44336;
        }
        .status-warning {
            background: #ff9800;
            box-shadow: 0 0 10px #ff9800;
        }
        .update-indicator {
            text-align: center;
            padding: 10px;
            font-size: 0.9em;
            opacity: 0.7;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .updating {
            animation: pulse 1s ease-in-out infinite;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚗 Leaf Shifter</h1>
            <div class="subtitle">Debug Console v2.3</div>
        </div>

        <!-- Current Gear Display -->
        <div class="card">
            <div class="gear-display" id="gearDisplay">---</div>
        </div>

        <!-- ADC & GPIO Data -->
        <div class="card">
            <h2>Sensor Data</h2>
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

        <!-- Threshold Ranges -->
        <div class="card">
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
        // Fetch data from server and update display
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    // Update ADC values
                    document.getElementById('adcValue').textContent = data.adc;
                    document.getElementById('voltageValue').textContent = data.voltage.toFixed(2) + 'V';

                    // Update gear display
                    document.getElementById('gearDisplay').textContent = data.gear;

                    // Update GPIO
                    document.getElementById('gpioValue').textContent = data.gpio;

                    // Update lockout status
                    const lockInd = document.getElementById('lockIndicator');
                    if (data.locked) {
                        lockInd.className = 'status-indicator status-inactive';
                        document.getElementById('lockStatus').textContent =
                            data.waiting_home ? 'Locked (Waiting HOME)' : 'Locked';
                    } else {
                        lockInd.className = 'status-indicator status-active';
                        document.getElementById('lockStatus').textContent = 'Unlocked';
                    }

                    // Update pulse status
                    const pulseInd = document.getElementById('pulseIndicator');
                    if (data.pulsing) {
                        pulseInd.className = 'status-indicator status-active';
                        document.getElementById('pulseStatus').textContent = 'Active';
                    } else {
                        pulseInd.className = 'status-indicator status-inactive';
                        document.getElementById('pulseStatus').textContent = 'Idle';
                    }

                    // Update neutral timer
                    const neutralInd = document.getElementById('neutralIndicator');
                    if (data.neutral_timing) {
                        neutralInd.className = 'status-indicator status-warning';
                        document.getElementById('neutralStatus').textContent = 'Timing...';
                    } else {
                        neutralInd.className = 'status-indicator status-inactive';
                        document.getElementById('neutralStatus').textContent = 'Inactive';
                    }

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

                    // Update uptime
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
    // Read current ADC value
    lastADC = readADCRaw(ADC_CHANNEL_PADDLE);
    float voltage = (lastADC / 4095.0) * 5.0;

    // Get gear name
    const char* gearName = getGearName(state.current_gear, state.drive_brake_mode);

    // Get GPIO output
    uint8_t gpio = getCurrentGPIOOutput();

    // Build JSON string
    String json = "{";

    // ADC data
    json += "\"adc\":" + String(lastADC) + ",";
    json += "\"voltage\":" + String(voltage, 2) + ",";

    // Gear and GPIO
    json += "\"gear\":\"" + String(gearName) + "\",";
    json += "\"gpio\":\"0x" + String(gpio, HEX) + "\",";

    // Status flags
    json += "\"locked\":" + String(state.gear_locked ? "true" : "false") + ",";
    json += "\"waiting_home\":" + String(state.waiting_for_home ? "true" : "false") + ",";
    json += "\"pulsing\":" + String(state.gpio_pulsing ? "true" : "false") + ",";
    json += "\"neutral_timing\":" + String(state.neutral_timing ? "true" : "false") + ",";

    // Uptime
    unsigned long uptime_sec = millis() / 1000;
    json += "\"uptime_sec\":" + String(uptime_sec) + ",";

    // Threshold data
    json += "\"thresholds\":[";
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
