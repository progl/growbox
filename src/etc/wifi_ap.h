#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <Arduino.h>

// —all these must be defined in your main.cpp—
extern AsyncWebServer server;             // e.g. AsyncWebServer server(80);
extern TimerHandle_t wifiReconnectTimer;  // created with xTimerCreate(...)
bool serverStarted;
extern bool isAPMode;

extern String ssid, password, wifiIp;
extern void setupStaticFiles();
extern void connectToMqtt();
extern void syslog_ng(String x);

// Forward declarations
void wifiReconnectTask(void *param);
void scheduleWiFiReconnect();
bool tryConnectToWiFi();
void WiFiEvent(WiFiEvent_t event);
void setupStaticFiles();
void configAP();
void initWiFiEvents();  // New function to initialize WiFi events

// —DNS and network settings, also defined in main.cpp—
DNSServer dnsServer;
extern IPAddress local_ip, gateway, subnet;
extern String AP_SSID, AP_PASS;
bool wifiReconnectNeeded = true;
// 1) Configure AP mode (called only from DISCONNECTED handler)
void configAP()
{
    WiFi.mode(WIFI_AP);
    Serial.println("Switching to AP mode...");

    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    // captive-portal handlers (register once)
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(204); });
    server.on("/connecttest.txt", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "success"); });
    server.on("/checkip.php", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", "<html><body>Success</body></html>"); });
    server.on("/ncsi.txt", HTTP_GET,
              [](AsyncWebServerRequest *request) { request->send(200, "text/plain", "Microsoft NCSI"); });
    server.on(
        "/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(200, "text/html", "<html><head><title>Success</title></head><body>Success</body></html>"); });
    server.on(
        "/fwlink", HTTP_GET, [](AsyncWebServerRequest *request)
        { request->send(200, "text/html", "<html><head><title>Success</title></head><body>Success</body></html>"); });

    // network setup
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAPsetHostname("growbox");

    dnsServer.setTTL(60);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", local_ip);

    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str(), 11, 0, 4);
    isAPMode = true;

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("AP mode: SSID=%s PASS=%s URL=http://%s\n", AP_SSID.c_str(), AP_PASS.c_str(), ip.toString().c_str());
    server.begin();  // ← обязательно!
}

// Initialize WiFi event handlers (call this once in setup)
void initWiFiEvents()
{
    static bool eventsInited = false;
    if (!eventsInited)
    {
        WiFi.onEvent(WiFiEvent);
        eventsInited = true;
    }
}

// 3) Single non-blocking connect attempt
bool tryConnectToWiFi()
{
    // Don't try to connect if already connected
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    // Configure WiFi
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);

    // Don't log the full password for security
    String logMsg = "WiFi: Connecting to ";
    logMsg += ssid;
    logMsg += " [password hidden]";
    syslog_ng(logMsg);

    WiFi.begin(ssid.c_str(), password.c_str());
    // Start connection attempt
    return WiFi.waitForConnectResult(5000) == WL_CONNECTED;
}

// 4) High-level connect: only starts STA attempt, no immediate AP fallback
bool connectToWiFi()
{
    if (tryConnectToWiFi())
    {
        if (!serverStarted)
        {
            setupStaticFiles();
            serverStarted = true;
            syslog_ng("Web: Server started");
        }
        return true;
    }
    return false;
}

// 2) WiFi event handler
void WiFiEvent(WiFiEvent_t event)
{
    static bool inEvent = false;
    if (inEvent) return;
    inEvent = true;

    switch (event)
    {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            syslog_ng("WiFi: Connected to AP");
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            wifiIp = WiFi.localIP().toString();
            syslog_ng("WiFi: Got IP " + wifiIp);
            wifiReconnectNeeded = false;  // Reset reconnect flag on successful connection

            // Start web server if not already running
            if (!serverStarted)
            {
                setupStaticFiles();
                serverStarted = true;
                syslog_ng("Web: Server started");
            }

            // If we were in AP mode and now connected to STA, stop AP
            if (isAPMode)
            {
                syslog_ng("WiFi: Connected to STA, stopping AP mode");
                WiFi.softAPdisconnect(true);
                isAPMode = false;
            }

            // Start MQTT if enabled
            if (enable_ponics_online)
            {
                connectToMqtt();
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        {
            syslog_ng("WiFi: Disconnected, status: " + String(WiFi.status()));

            // If we're not in AP mode, start AP mode
            if (!isAPMode)
            {
                syslog_ng("WiFi: Starting AP mode");
                configAP();
            }

            // Always try to reconnect to STA in the background
            if (!wifiReconnectNeeded)
            {
                wifiReconnectNeeded = true;
                scheduleWiFiReconnect();
            }
            break;
        }
    }

    inEvent = false;
}

// Structure to hold retry count for the task
typedef struct
{
    uint8_t retryCount;
} WiFiReconnectParams_t;

// 5) Task-based WiFi reconnection
void wifiReconnectTask(void *param)
{
    WiFiReconnectParams_t *params = (WiFiReconnectParams_t *)param;
    const uint8_t maxRetries = 5;
    const uint32_t initialDelayMs = 5000;  // 5 seconds initial delay
    const uint32_t maxDelayMs = 30000;     // 30 seconds max delay

    if (params == nullptr)
    {
        vTaskDelete(nullptr);
        return;
    }

    if (WiFi.status() != WL_CONNECTED && wifiReconnectNeeded)
    {
        if (params->retryCount < maxRetries)
        {
            params->retryCount++;
            syslog_ng("WiFi: Reconnect attempt " + String(params->retryCount) + "/" + String(maxRetries));

            // Try to connect to WiFi
            if (tryConnectToWiFi())
            {
                syslog_ng("WiFi: Successfully reconnected to STA");
                delete params;
                vTaskDelete(nullptr);
                return;
            }

            // Calculate exponential backoff with jitter
            uint32_t delayMs = min(maxDelayMs, initialDelayMs * (1 << min(params->retryCount - 1, 10)));
            delayMs = random(delayMs * 0.8, delayMs * 1.2);  // Add jitter

            vTaskDelay(pdMS_TO_TICKS(delayMs));  // Delay before next attempt

            // Create new params for next attempt
            WiFiReconnectParams_t *nextParams = new WiFiReconnectParams_t();
            nextParams->retryCount = params->retryCount;

            // Schedule next attempt
            if (xTaskCreatePinnedToCore(wifiReconnectTask, "wifi_reconnect",
                                        4096,  // Stack size
                                        (void *)nextParams,
                                        1,  // Priority
                                        nullptr, tskNO_AFFINITY) != pdPASS)
            {
                delete nextParams;  // Clean up if task creation fails
                syslog_ng("WiFi: Failed to schedule reconnect task");
            }
        }
        else
        {
            // Max retries reached, ensure AP mode is active
            if (!isAPMode)
            {
                syslog_ng("WiFi: Max retries reached, ensuring AP mode");
                configAP();
            }
        }
    }

    // Clean up
    delete params;
    vTaskDelete(nullptr);
}

// 6) Schedule WiFi reconnect using a task
void scheduleWiFiReconnect()
{
    static TaskHandle_t reconnectTask = nullptr;

    // Clean up any existing task
    if (reconnectTask != nullptr && eTaskGetState(reconnectTask) != eDeleted)
    {
        vTaskDelete(reconnectTask);
        reconnectTask = nullptr;
    }

    // Create new task parameters
    WiFiReconnectParams_t *params = new WiFiReconnectParams_t();
    params->retryCount = 0;

    // Create new reconnect task
    if (xTaskCreatePinnedToCore(wifiReconnectTask, "wifi_reconnect",
                                4096,  // Stack size
                                (void *)params,
                                1,  // Priority
                                &reconnectTask, tskNO_AFFINITY) != pdPASS)
    {
        delete params;  // Clean up if task creation fails
        syslog_ng("WiFi: Failed to create reconnect task");
    }
    else
    {
        syslog_ng("WiFi: Reconnect task started");
    }
}
