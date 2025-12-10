#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

//=============================================================================
// WEB SERVER FOR REAL-TIME DEBUG DISPLAY
//=============================================================================
// Provides WiFi AP and web interface for debugging paddle shifter
// - Creates WiFi AP "Leaf-Shifter" with password "LeafControl"
// - Serves HTML page at http://192.168.4.1
// - Provides JSON API at /data for real-time updates
// - Displays ADC values, gear state, lockout status, thresholds, etc.
//=============================================================================

// Initialize web server and WiFi AP
// Call once from setup()
void initWebServer();

// Handle web server requests
// Call periodically from loop()
void handleWebServer();

// Get current system state as JSON string
// Used by /data endpoint for AJAX polling
String getStateJSON();

#endif // WEB_SERVER_H
