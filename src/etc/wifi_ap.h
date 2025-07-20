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
extern void connectToMqtt();
extern void syslog_ng(String x);

bool tryConnectToWiFi();
void WiFiEvent(WiFiEvent_t event);
void setupStaticFiles();
void configAP();
void initWiFiEvents();  // New function to initialize WiFi events

// —DNS and network settings, also defined in main.cpp—
DNSServer dnsServer;
extern IPAddress local_ip, gateway, subnet;
extern String AP_SSID, AP_PASS;

// 1) Configure AP mode (called only from DISCONNECTED handler)
void configAP()
{
    WiFi.mode(WIFI_AP);
    Serial.println("Switching to AP mode...");

    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    // Обработчик главной страницы - отображаем обычный интерфейс с кнопкой настройки WiFi
    setupStaticFiles();
    // Обработчики для Captive Portal
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    server.on("/checkip.php", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });
    server.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/"); });

    // Настройка сети
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAPsetHostname("growbox");

    // Настройка DNS сервера для перенаправления всех запросов
    dnsServer.setTTL(60);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", local_ip);

    // Запуск точки доступа
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str(), 11, 0, 4);
    isAPMode = true;

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("AP mode: SSID=%s PASS=%s URL=http://%s\n", AP_SSID.c_str(), AP_PASS.c_str(), ip.toString().c_str());

    // Запуск веб-сервера
    server.begin();

    // Логируем URL для доступа к настройкам
    Serial.println("Captive Portal started. Connect to WiFi 'GrowBox' and open http://192.168.4.1 in your browser.");
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
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }
    String logMsg = "WiFi: Connecting to ";
    logMsg += ssid;
    logMsg += " [password hidden]";
    syslog_ng(logMsg);

    WiFi.begin(ssid.c_str(), password.c_str());
    bool connected = WiFi.waitForConnectResult(5000) == WL_CONNECTED;
    if (connected)
    {
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);
    }
    // Start connection attempt
    return connected;
}

// 4) High-level connect: only starts STA attempt, no immediate AP fallback
bool connectToWiFi()
{
    if (tryConnectToWiFi())
    {
        return true;
    }
    configAP();
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

            // Start web server if not already running
            if (!serverStarted)
            {
                setupStaticFiles();
                serverStarted = true;
                syslog_ng("Web: Server started");
            }
            // Start MQTT if enabled
            if (enable_ponics_online)
            {
                connectToMqtt();
            }
            break;
    }

    inEvent = false;
}

// Structure to hold retry count for the task
typedef struct
{
    uint8_t retryCount;
} WiFiReconnectParams_t;
