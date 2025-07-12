
#include <Arduino.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>  // For esp_reset_reason()
#include <Update.h>      // For OTA updates

// Forward declarations
class AsyncWebServerRequest;
class AsyncWebServerResponse;

// Forward declarations from other modules
String getContentType(const String &path);

// External server instance
extern AsyncWebServer server;

// Function declarations
void serveFile(AsyncWebServerRequest *request, const String &path, const String &mimeType);
void serveFileWithGzip(AsyncWebServerRequest *request, const String &path, const String &mimeType, bool gzip = true);
// Function declarations
void handleBoardInfo(AsyncWebServerRequest *request);
void setupStaticFiles();
bool isAuthorized(AsyncWebServerRequest *request);
String generateToken();
String getContentType(const String &path);

#define OTA_TIMEOUT_MS 15000