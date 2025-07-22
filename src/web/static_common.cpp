
#include <ArduinoJson.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>
#include <nvs_flash.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>  // For AsyncCallbackJsonWebHandler
#include "static_common.h"
#include "ram_saver.h"
#include "ram_saver_api.h"

#include "GyverFilters.h"  // For GKalman
#include "esp_heap_caps.h"

extern bool mqttClientHAConnected, mqttClientPonicsConnected;
extern void applyTelegramSettings();
extern void handleCoreDump(AsyncWebServerRequest *request);
// Forward declarations from main.cpp

StaticJsonDocument<512> dochandle_token;  // Reduced from 1024
StaticJsonDocument<512> docApiStatuses;   // Reduced from 1024
StaticJsonDocument<1024> docBoardInfo;    // Reduced from 2048
StaticJsonDocument<512> docDeser;         // Reduced from 1024

typedef uint8_t DeviceAddress[8];

// Forward declare Sensor type
struct Sensor
{
    char name[50];     // Название датчика
    uint8_t detected;  // Статус детектирования: 0 - не детектирован, 1 - детектирован
};

// SensorData struct to match the one in main.cpp
struct SensorData
{
    DeviceAddress address;
    float temperature;
    String key;
    GKalman KalmanDallasTmp;

    // Default constructor with default Kalman parameters
    SensorData() : KalmanDallasTmp(0.1, 0.1, 0.01) {}
};

// External declarations
extern SensorData sensorArray[10];
// Kalman filter instances
extern GKalman KalmanNTC;
extern GKalman KalmanDist;
extern bool formatLittleFS();
extern int sensorsCount;
extern Sensor sensors[10];
// Preference item structure
typedef struct
{
    const char *key;
    const char *type;
    const char *label;
    const char *group;
    const char *description;
    float min;
    float max;
    float step;
    int decimals;
    bool readonly;
} PreferenceItem;

// Forward declarations for functions
void handleApiTasks(AsyncWebServerRequest *request);
static bool otaTimedOut = false;

extern String sessionToken;
extern bool shouldReboot;
extern String httpAuthPass;
extern String httpAuthUser;
extern Preferences preferences;
extern Preferences config_preferences;
extern bool OtaStart;
extern bool waitingSecondOta;
static bool updateBegun = false;

// Sensor related
extern String not_detected_sensors;
extern String detected_sensors;
extern int sensorCount;
extern int MAX_DS18B20_SENSORS;

// Network credentials
extern String ssid;
extern String password;

// Update and system
extern String update_token;
extern bool force_update;
extern int percentage;

// Sensor readings
extern float AirVPD;
extern float AirHum;
extern float RootTemp;
extern float AirTemp;
extern float wPR;
extern float wEC;
extern float wNTC;
extern float wpH;
extern float wLevel;
extern float CPUTemp;

// Kalman filter parameters
extern float ntc_mea_e, ntc_est_e, ntc_q;
extern float dist_mea_e, dist_est_e, dist_q;
extern float ec_mea_e, ec_est_e, ec_q;
extern float dallas_mea_e, dallas_est_e, dallas_q;

// MQTT and WiFi
extern int e_ha;
extern WiFiClient mqttClientHA;

// Time variables
unsigned long lastDataTime;

// Root drive control
extern unsigned long NextRootDrivePwdOn, NextRootDrivePwdOff;
extern int RDDelayOn, RDDelayOff;

// Kalman filter instances
extern GKalman KalmanNTC;
extern GKalman KalmanDist;
extern GKalman KalmanEC;
extern GKalman KalmanEcUsual;

// System info
extern String HOSTNAME;
extern String Firmware;
extern String vpdstage;

// External functions
extern void handleRedirect(AsyncWebServerRequest *request);
extern String ApiGroups(bool labels = false);
extern bool updatePreference(const char *settingName, const JsonVariant &value, String mqtt_type = "all");
extern bool updatePreference(const char *settingName, const String &value, String mqtt_type = "all");
extern String getGroupNameByParameter(const String &param);
extern PreferenceItem *findPreferenceByKey(const char *key);
extern void connectToWiFi();
extern void run_doser_now();
extern void MCP23017();

extern void syslogf(const char *format, ...);
extern bool downloadAndUpdateIndexFile();
extern void clearLittleFS();
extern String sanitizeString(const String &input);
bool fFTS(float x, byte precision, char *buffer, size_t bufferSize);

// Web server
extern AsyncWebServer server;
extern AsyncEventSource events;
// OTA update page HTML
const char otaIndex[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head><title>OTA Update</title></head>
  <body>
    <h1>OTA Update</h1>
    <form method="POST" action="/ota" enctype="multipart/form-data">
        <br/><br/>
      <label>Destination:<br/>
        <select name="fs">
        <option value="littlefs">LittleFS</option>
        <option value="flash">Firmware (flash)</option>
        </select>
      </label>
       <label>File: <input type="file" name="update"></label><br/><br/>
      <input type="submit" value="Upload">
    </form>
  </body>
</html>
)rawliteral";

// Global server instance
extern AsyncWebServer server;
#define DOWNLOAD_TASK_STACK_SIZE 16384  // Increased stack size for SSL operations

void runDownloadTask()
{
    syslogf("Starting download task...");
    syslogf("Free heap: %u bytes", ESP.getFreeHeap());
    syslogf("Largest free block: %u bytes", ESP.getMaxAllocHeap());

    OtaStart = true;
    bool success = false;

    // Wait for WiFi connection
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries++ < 20)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        syslogf("WiFi connected, starting download...");
        success = downloadAndUpdateIndexFile();
    }
    else
    {
        syslogf("Failed to connect to WiFi");
    }

    if (success)
    {
        syslogf("update: Successfully updated index.html.gz");
    }
    else
    {
        syslogf("update: Failed to update index.html.gz");
    }

    OtaStart = false;
}

void handleUpdate(AsyncWebServerRequest *request)
{
    preferences.putInt("upd", 1);
    syslogf("update: make_update start update firmware - wait 40-60 seconds, page will reload automatically");
    OtaStart = true;
    force_update = true;
    percentage = 1;
    handleRedirect(request);
    shouldReboot = true;
}

void handleApiLabels(AsyncWebServerRequest *request)
{
    String output = ApiGroups(true);
    request->send(200, "application/json", sanitizeString(output));
}

void handleApiGroups(AsyncWebServerRequest *request)
{
    String output = ApiGroups(false);
    request->send(200, "application/json", sanitizeString(output));
}

void handle_token(AsyncWebServerRequest *request)
{
    syslogf("WEB /handle_token");

    dochandle_token.clear();
    dochandle_token["uuid"] = update_token;
    String output;
    serializeJson(dochandle_token, output);
    request->send(200, "application/json", sanitizeString(output));
}

void handleApiStatuses(AsyncWebServerRequest *request)
{
    syslogf("WEB /handleApiStatuses");
    unsigned long t_millis = millis();

    docApiStatuses.clear();

    if (not_detected_sensors.isEmpty())
    {
        for (int i = 0; i < sensorsCount; ++i)
        {
            if (!sensors[i].detected)
            {
                not_detected_sensors += "\n" + String(sensors[i].name);
            }
        }
    }

    if (detected_sensors.isEmpty())
    {
        for (int i = 0; i < sensorsCount; ++i)
        {
            if (sensors[i].detected)
            {
                detected_sensors += "\n" + String(sensors[i].name);
            }
        }
    }

    docApiStatuses["hostname"] = String(HOSTNAME);
    docApiStatuses["firmware"] = String(Firmware);
    docApiStatuses["vpdstage"] = vpdstage;
    docApiStatuses["ha"] = mqttClientHAConnected;
    docApiStatuses["ponics"] = mqttClientPonicsConnected;
    docApiStatuses["uptime"] = t_millis;
    char buffer[32];
    if (fFTS(WiFi.RSSI(), 0, buffer, sizeof(buffer)))
    {
        docApiStatuses["wifi_rssi"] = buffer;
    }
    if (fFTS(AirVPD, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["AirVPD"] = buffer;
    }
    char buffer2[32];
    if (fFTS(AirHum, 1, buffer2, sizeof(buffer2)))
    {
        docApiStatuses["AirHum"] = String(buffer2) + "%";
    }
    if (fFTS(RootTemp, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["RootTemp"] = buffer;
    }
    if (fFTS(AirTemp, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["AirTemp"] = buffer;
    }
    if (fFTS(wPR, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["wPR"] = buffer;
    }
    if (fFTS(wEC, 2, buffer, sizeof(buffer)))
    {
        docApiStatuses["wEC"] = buffer;
    }
    if (fFTS(wNTC, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["wNTC"] = buffer;
    }
    if (fFTS(wpH, 2, buffer, sizeof(buffer)))
    {
        docApiStatuses["wpH"] = buffer;
    }
    if (fFTS(wLevel, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["wLevel"] = buffer;
    }
    if (fFTS(CPUTemp, 1, buffer, sizeof(buffer)))
    {
        docApiStatuses["CPUTemp"] = buffer;
    }

    String output;
    serializeJson(docApiStatuses, output);
    request->send(200, "application/json", sanitizeString(output));
}

// Function to handle board info

void handleBoardInfo(AsyncWebServerRequest *request)
{
    docBoardInfo.clear();

    // --- Heap info ---
    size_t freeHeap = ESP.getFreeHeap();
    size_t maxAllocHeap = ESP.getMaxAllocHeap();

    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);

    size_t min_free_size_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);

    docBoardInfo["heap"]["free_bytes"] = freeHeap;
    docBoardInfo["heap"]["max_alloc_bytes"] = maxAllocHeap;
    docBoardInfo["heap"]["total_allocated_bytes"] = heap_info.total_allocated_bytes;
    docBoardInfo["heap"]["total_free_bytes"] = heap_info.total_free_bytes;
    docBoardInfo["heap"]["largest_free_block"] = heap_info.largest_free_block;
    docBoardInfo["heap"]["minimum_free_bytes"] = heap_info.minimum_free_bytes;
    docBoardInfo["heap"]["free_blocks_count"] = heap_info.free_blocks;
    docBoardInfo["heap"]["min_free_size_ever"] = min_free_size_ever;

    float frag_ratio = 100.0f * (1.0f - ((float)heap_info.largest_free_block / (float)heap_info.total_free_bytes));
    docBoardInfo["heap"]["fragmentation_percent"] = frag_ratio;

    // --- PSRAM info ---
    bool psramAvailable = esp_spiram_is_initialized();
    docBoardInfo["psram"]["enabled"] = psramAvailable;

    if (psramAvailable)
    {
        size_t psramSize = esp_spiram_get_size();
        size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t psramMinFreeEver = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);

        docBoardInfo["psram"]["size_bytes"] = psramSize;
        docBoardInfo["psram"]["free_bytes"] = psramFree;
        docBoardInfo["psram"]["min_free_bytes_ever"] = psramMinFreeEver;
    }
    else
    {
        docBoardInfo["psram"]["size_bytes"] = 0;
        docBoardInfo["psram"]["free_bytes"] = 0;
        docBoardInfo["psram"]["min_free_bytes_ever"] = 0;
    }

    // --- FS info ---
    size_t fsTotal = LittleFS.totalBytes();
    size_t fsUsed = LittleFS.usedBytes();
    docBoardInfo["fs"]["total_kb"] = fsTotal / 1024;
    docBoardInfo["fs"]["used_kb"] = fsUsed / 1024;
    docBoardInfo["fs"]["free_kb"] = (fsTotal - fsUsed) / 1024;

    // --- Uptime ---
    unsigned long uptime = millis();
    docBoardInfo["uptime_ms"] = uptime;
    unsigned long days = uptime / 86400000;
    unsigned long hours = (uptime % 86400000) / 3600000;
    unsigned long minutes = (uptime % 3600000) / 60000;
    unsigned long seconds = (uptime % 60000) / 1000;
    unsigned long milliseconds = uptime % 1000;
    char uptimeStr[64];
    snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02lu:%02lu:%02lu.%03lu", days, hours, minutes, seconds, milliseconds);
    docBoardInfo["uptime"]["formatted"] = uptimeStr;

    // --- Core temp ---
    float coreTemp = temperatureRead();
    docBoardInfo["core_temp_c"] = isnan(coreTemp) ? "" : String(coreTemp);

    // --- Sketch info ---
    docBoardInfo["sketch"]["size_bytes"] = ESP.getSketchSize();
    docBoardInfo["sketch"]["free_space_bytes"] = ESP.getFreeSketchSpace();

    // --- Reset reason ---
    docBoardInfo["reset_reason"] = (int)esp_reset_reason();

    // --- SDK and chip info ---
    docBoardInfo["sdk_version"] = ESP.getSdkVersion();
    docBoardInfo["firmware"] = Firmware;
    docBoardInfo["chip_model"] = ESP.getChipModel();
    docBoardInfo["chip_revision"] = ESP.getChipRevision();
    docBoardInfo["cpu_freq_mhz"] = ESP.getCpuFreqMHz();

    // --- WiFi info ---
    if (WiFi.isConnected())
    {
        docBoardInfo["wifi"]["ssid"] = WiFi.SSID();
        docBoardInfo["wifi"]["rssi"] = WiFi.RSSI();
        docBoardInfo["wifi"]["ip"] = WiFi.localIP().toString();
        docBoardInfo["wifi"]["connected"] = true;
    }
    else
    {
        docBoardInfo["wifi"]["connected"] = false;
    }

    // --- JSON doc memory usage ---
    docBoardInfo["json_doc_memory_usage"] = docBoardInfo.memoryUsage();
    docBoardInfo["json_doc_capacity"] = docBoardInfo.capacity();

    // --- Send response ---
    String output;
    serializeJsonPretty(docBoardInfo, output);
    request->send(200, "application/json", output);
}

void handleRedirect(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = request->beginResponse(302);
    response->addHeader("Location", "/");  // Адрес, на который редирект
    request->send(response);
}

String generateToken()
{
    String token = "";
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (int i = 0; i < 32; i++)
    {
        token += charset[random(sizeof(charset) - 1)];
    }
    return token;
}

// Function to get content type based on file extension
String getContentType(const String &path)
{
    if (path.endsWith(".html"))
        return "text/html";
    else if (path.endsWith(".css"))
        return "text/css";
    else if (path.endsWith(".js"))
        return "application/javascript";
    else if (path.endsWith(".json"))
        return "application/json";
    else if (path.endsWith(".png"))
        return "image/png";
    else if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
        return "image/jpeg";
    else if (path.endsWith(".gif"))
        return "image/gif";
    else if (path.endsWith(".ico"))
        return "image/x-icon";
    else if (path.endsWith(".xml"))
        return "text/xml";
    else if (path.endsWith(".pdf"))
        return "application/x-pdf";
    else if (path.endsWith(".zip"))
        return "application/x-zip";
    else if (path.endsWith(".gz"))
        return "application/x-gzip";
    return "text/plain";
}

void serveFile(AsyncWebServerRequest *request, const String &path, const String &mimeType, bool gzip)
{
    if (LittleFS.exists(path))
    {
        request->send(LittleFS, path, mimeType);
    }
    else
    {
        request->send(404, "text/plain", "serveFile File not found");
    }
}

void serveFileWithGzip(AsyncWebServerRequest *request, const String &path, const String &mimeType = "", bool gzip)
{
    String contentType = mimeType.length() ? mimeType : getContentType(path);
    String gzPath = path + ".gz";

    // Проверяем поддержку gzip и наличие .gz-файла
    bool wantGzip = gzip && request->hasHeader("Accept-Encoding") &&
                    request->getHeader("Accept-Encoding")->value().indexOf("gzip") != -1 && LittleFS.exists(gzPath);

    // Выбираем путь и заголовки
    String filePath = wantGzip ? gzPath : path;
    if (!LittleFS.exists(filePath))
    {
        request->send(404, "text/plain", "serveFile gz File Not Found");
        return;
    }

    // Открываем файл
    File f = LittleFS.open(filePath, "r");
    if (!f)
    {
        request->send(500, "text/plain", "Failed to open file");
        return;
    }

    // Начинаем ответ с File-объектом
    AsyncWebServerResponse *response = request->beginResponse(f, filePath, contentType);

    // Если gzip — добавляем нужный header
    if (wantGzip)
    {
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Vary", "Accept-Encoding");
    }

    // Общие заголовки кэширования
    response->addHeader("Cache-Control", "public, max-age=31536000, immutable");

    request->send(response);
}

#include "static_common_gen.h"

size_t totalExpected = 0;

// Прогресс колбэк для Update
void setupOtaProgress()
{
    Update.onProgress(
        [](size_t done, size_t total)
        {
            float pct = (done * 100.0f) / total;
            syslogf("[OTA] Progress: %f%% (%lu/%lu)", pct, done, total);
        });
}

// HTTP-POST колбэк (после завершения загрузки файла)
void handleOtaRequest(AsyncWebServerRequest *request)
{
    if (otaTimedOut)
    {
        syslogf("[OTA] Timeout occurred before POST finalized");
        auto resp = request->beginResponse(408, "text/plain", "OTA timeout");
        resp->addHeader("Connection", "close");
        request->send(resp);
        otaTimedOut = false;  // сбросим флаг, без ребута
        return;
    }

    // Проверим, нужно ли ждать 2-ой части
    if (request->hasParam("multi", true) && request->getParam("multi", true)->value() == "1")
    {
        waitingSecondOta = true;
        syslogf("[OTA] First part uploaded, waiting for second...");
    }
    else
    {
        waitingSecondOta = false;
        syslogf("[OTA] All parts uploaded, reboot requested");
        shouldReboot = true;
    }

    // Ответ клиенту
    auto resp = request->beginResponse(200, "text/html", "<html><body><h2>Update completed</h2></body></html>");
    resp->addHeader("Connection", "close");
    resp->addHeader("X-OTA-Success", "1");
    request->send(resp);
}

// Upload-колбэк (вызывается на каждый фрагмент данных)
void handleOtaUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len,
                     bool final)
{
    // Инициализация при первом фрагменте
    if (index == 0 && !updateBegun)
    {
        totalExpected = request->contentLength();
        syslogf("[OTA] Begin upload, expected size: %lu", totalExpected);
        lastDataTime = millis();
        updateBegun = true;

        // Ищем POST - параметр fs в теле multipart / form - data
        // Ищем POST-параметр fs в теле multipart/form-data
        const AsyncWebParameter *fsParam = request->getParam("fs", true);

        bool isFS = (fsParam != nullptr && fsParam->value().equalsIgnoreCase("littlefs"));

        String dest = isFS ? "littlefs" : "firmware";

        if (dest.equalsIgnoreCase("littlefs"))
        {
            syslogf("[OTA] Starting FS OTA update...");
            Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
        }
        else
        {
            syslogf("[OTA] Starting firmware (flash) OTA update...");
            Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        }
    }

    // Сброс «таймера» приёма
    lastDataTime = millis();

    // Write data with error checking
    size_t written = Update.write(data, len);
    if (written != len)
    {
        syslogf("[OTA] Write failed: %s", Update.errorString());
        Update.abort();
        updateBegun = false;
        otaTimedOut = true;
        return;
    }

    // Update progress
    static unsigned long lastProgressUpdate = 0;
    if (millis() - lastProgressUpdate > 1000)
    {  // Update progress every second
        float progress = (Update.progress() * 100.0f) / Update.size();
        syslogf("[OTA] Progress: %f%% (%lu/%lu)", progress, Update.progress(), Update.size());
        lastProgressUpdate = millis();
    }
    // Check for timeout
    if (millis() - lastDataTime > OTA_TIMEOUT_MS)
    {
        syslogf("[OTA] Upload timed out");
        Update.abort();
        updateBegun = false;
        otaTimedOut = true;
        return;
    }

    // Если последний фрагмент — завершаем OTA
    if (final)
    {
        bool ok = Update.end(true);
        if (ok && !Update.hasError())
        {
            syslogf("[OTA] OTA Update Success");
        }
        else
        {
            syslogf("[OTA] OTA Failed: %s", Update.errorString());
        }
        updateBegun = false;
    }
}

// Setup static file server routes and handlers
void setupStaticFiles()
{
    server.onNotFound([](AsyncWebServerRequest *request) { request->send(404, "text/plain", "Not found"); });

    // Set timeouts
    server.rewrite("/", "/index.html");

    setupRoutes(server);
    // Serve static files with gzip support
    server.on("/", HTTP_GET,
              [](AsyncWebServerRequest *request) { serveFileWithGzip(request, "/index.html", "text/html", true); });
    // API endpoints
    server.on("/api/board", HTTP_GET, [](AsyncWebServerRequest *request) { handleBoardInfo(request); });

    // Handle OTA update page
    server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", otaIndex); });

    // Add CORS headers to all API responses
    server.on("/api/*", HTTP_OPTIONS,
              [](AsyncWebServerRequest *request)
              {
                  AsyncWebServerResponse *response = request->beginResponse(200);
                  if (response)
                  {
                      response->addHeader("Access-Control-Allow-Origin", "*");
                      response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                      response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                      request->send(response);
                  }
                  else
                  {
                      request->send(500, "text/plain", "Failed to create response");
                  }
              });

    server.on("/ota", HTTP_POST, handleOtaRequest, handleOtaUpload);

    server.on("/api/verify", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Invalid or expired token\"}");
                      return;
                  }
                  request->send(200, "application/json", "{\"status\":\"ok\"}");
              });

    server.on(
        "/api/login", HTTP_POST,
        [](AsyncWebServerRequest *request)
        {
            // Обработчик не отправляет ответ сразу, т.к. тело читается асинхронно
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            // Получаем тело запроса (т.к. HTTP_POST, тело может приходить кусками)
            // Читаем весь body по частям, собираем в строку
            static String body;
            if (index == 0)
            {
                body = "";
            }
            for (size_t i = 0; i < len; i++)
            {
                body += (char)data[i];
            }

            // Когда получен весь body
            if (index + len == total)
            {
                DeserializationError error = deserializeJson(docDeser, body);
                if (error)
                {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }

                const char *username = docDeser["username"];
                const char *password = docDeser["password"];

                if (!username || !password)
                {
                    request->send(400, "application/json", "{\"error\":\"Missing credentials\"}");
                    return;
                }

                if (String(username) == httpAuthUser && String(password) == httpAuthPass)
                {
                    if (sessionToken == "")
                    {
                        sessionToken = generateToken();
                        preferences.putString("token", sessionToken);
                    }

                    String response = "{\"token\":\"" + sessionToken + "\"}";
                    request->send(200, "application/json", response);
                }
                else
                {
                    request->send(401, "application/json", "{\"error\":\"Invalid login\"}");
                }
            }
        });

    server.on("/api/logout", HTTP_POST,
              [](AsyncWebServerRequest *request)
              {
                  sessionToken = "";
                  request->send(200, "application/json", "{\"message\":\"Logged out\"}");
              });

    events.onConnect(
        [](AsyncEventSourceClient *client)
        {
            if (client->lastId())
            {
                Serial.printf("Client reconnected! Last message ID: %u\n", client->lastId());
            }
            client->send("hello!", NULL, millis(), 1000);  // welcome message
        });

    server.on("/api/status", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  handleApiStatuses(request);
              });

    server.on("/api/token", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  handle_token(request);
              });

    server.on("/api/groups", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  handleApiGroups(request);
              });

    server.on("/api/labels", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  handleApiLabels(request);
              });

    server.on("/api/tasks", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  handleApiTasks(request);
              });

    // Save settings handler
    AsyncCallbackJsonWebHandler *saveSettingsHandler = new AsyncCallbackJsonWebHandler(
        "/api/save-settings",
        [](AsyncWebServerRequest *request, JsonVariant &json)
        {
            if (!isAuthorized(request))
            {
                request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                return;
            }
            syslogf("saveSettings (via AsyncCallbackJsonWebHandler)");

            if (!json.is<JsonObject>())
            {
                request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON format\"}");
                return;
            }

            JsonObject doc = json.as<JsonObject>();

            for (JsonPair kv : doc)
            {
                const char *settingName = kv.key().c_str();

                // Special case: clear preferences
                if (strcmp(settingName, "clear_pref") == 0 && kv.value().as<int>() == 1)
                {
                    preferences.clear();
                    config_preferences.clear();
                    config_preferences.end();
                    preferences.end();

                    nvs_flash_erase_partition("nvs");  // Reformat NVS
                    nvs_flash_init_partition("nvs");   // Reinit

                    syslogf("clear_pref");

                    preferences.begin("settings", false);
                    config_preferences.begin("config", false);

                    preferences.putString("ssid", ssid);
                    preferences.putString("password", password);

                    request->send(200, "application/json", "{\"status\":\"cleared\"}");
                    return;
                }

                String groupName = getGroupNameByParameter(settingName);
                syslogf("Group name for %s: %s", settingName, groupName.c_str());

                if (updatePreference(settingName, kv.value()))
                {
                    const PreferenceItem *item = findPreferenceByKey(settingName);
                    // publish_one_data(item);

                    // Special cases
                    if (strcmp(settingName, "RDDelayOn") == 0)
                    {
                        NextRootDrivePwdOff = millis() + (RDDelayOn * 1000);
                        NextRootDrivePwdOn = NextRootDrivePwdOff + (RDDelayOff * 1000);
                    }
                    else if (strcmp(settingName, "RDDelayOff") == 0)
                    {
                        NextRootDrivePwdOn = millis() + (RDDelayOff * 1000);
                        NextRootDrivePwdOff = NextRootDrivePwdOn + (RDDelayOn * 1000);
                    }
                    else if (strcmp(settingName, "password") == 0)
                    {
                        connectToWiFi();
                    }
                    else if (strcmp(settingName, "SetPumpA_Ml") == 0 || strcmp(settingName, "SetPumpB_Ml") == 0)
                    {
                        if (kv.value().as<int>() > 0)
                        {
                            run_doser_now();
                        }
                    }
                    else if (strncmp(settingName, "DRV", 3) == 0 || strncmp(settingName, "PWD", 3) == 0 ||
                             strncmp(settingName, "FREQ", 4) == 0)
                    {
                        MCP23017();
                    }

                    // Successful save
                    request->send(200, "application/json", "{\"status\":\"success\"}");
                }
                else
                {
                    syslogf("Setting not found or error: %s", settingName);
                    request->send(400, "application/json",
                                  "{\"status\":\"error\",\"message\":\"Setting not found or memory error\"}");
                }

                applyTelegramSettings();
            }

            // Apply updated Kalman filter parameters
            KalmanNTC.setParameters(ntc_mea_e, ntc_est_e, ntc_q);
            KalmanDist.setParameters(dist_mea_e, dist_est_e, dist_q);
            KalmanEC.setParameters(ec_mea_e, ec_est_e, ec_q);
            KalmanEcUsual.setParameters(ec_mea_e, ec_est_e, ec_q);
        });
    server.addHandler(saveSettingsHandler);

    server.on("/api/update", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }

                  handleUpdate(request);
              });

    server.on("/api/clear", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  clearLittleFS();
                  request->send(200, "text/html", "<html><body>Success</body></html>");
              });

    server.on("/api/static-update", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  syslogf("update files task created");
                  request->send(200, "application/json",
                                "{\"status\":\"success\", \"message\":\"static update task created\"}");
                  preferences.putInt("upd_static", 1);
                  shouldReboot = true;
              });

    // Example usage in an endpoint
    server.on("/api/format-littlefs", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }

                  if (formatLittleFS())
                  {
                      request->send(200, "application/json",
                                    "{\"status\":\"success\",\"message\":\"LittleFS formatted successfully\"}");
                  }
                  else
                  {
                      request->send(500, "application/json",
                                    "{\"status\":\"error\",\"message\":\"Failed to format LittleFS\"}");
                  }
              });
    server.on("/api/core-dump", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }
                  handleCoreDump(request);
              });

    server.on("/api/board-info", HTTP_GET, [](AsyncWebServerRequest *request) { handleBoardInfo(request); });
    server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) { handleRedirect(request); });

    server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) { handleRedirect(request); });

    server.on("/api/reboot", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }

                  AsyncWebServerResponse *response =
                      request->beginResponse(200, "application/json", R"({"message": "Rebooting..."})");
                  response->addHeader("Connection", "close");
                  request->send(response);

                  shouldReboot = true;
              });
    server.on("/", HTTP_GET,
              [](AsyncWebServerRequest *request) { serveFileWithGzip(request, "/index.html", "text/html", true); });

    // Fallback static file handler with gzip support

    // Handle board info endpoint
    server.on("/api/board", HTTP_GET, [](AsyncWebServerRequest *request) { handleBoardInfo(request); });

    setupRamSaverAPI();

    server.addHandler(&events);
    server.begin();
}

// Function to check if request is authorized
bool isAuthorized(AsyncWebServerRequest *request)
{
    // Check Authorization header
    if (request->hasHeader("Authorization"))
    {
        String authHeader = request->getHeader("Authorization")->value();
        if (authHeader == "Bearer " + sessionToken)
        {
            return true;
        }
    }

    // Check auth GET parameter
    if (request->hasParam("auth"))
    {
        String authParam = request->getParam("auth")->value();
        if (authParam == "PonIcsOnLiNe2025")
        {
            return true;
        }
    }

    return false;
}
