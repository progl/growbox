#pragma once

// Global preferences and configuration for the GrowBox firmware

// Firmware version
#define FIRMWARE_VERSION "1.0.0"

// WiFi configuration
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// MQTT configuration
#define MQTT_SERVER ""
#define MQTT_USER ""
#define MQTT_PASSWORD ""

// Web server configuration
#define WEB_SERVER_PORT 80
#define WEB_USERNAME "admin"
#define WEB_PASSWORD "admin"

// OTA Update configuration
#define OTA_PORT 3232
#define OTA_PASSWORD "otapass"

// Debug settings
#define DEBUG_SERIAL_BAUD 115200

// Pin definitions (example - adjust according to your hardware)
#define PIN_RELAY_1 12
#define PIN_RELAY_2 13
#define PIN_SENSOR_1 34
#define PIN_SENSOR_2 35
