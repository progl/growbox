#include <stdint.h>
#include <Arduino.h>
#include "LittleFS.h"
#ifdef ARDUINO_ARCH_ESP32
#undef noInterrupts  // () {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)
#undef interrupts    // () portEXIT_CRITICAL(&mux);}
#endif
#include <MCP342x.h>
#include "GyverFilters.h"
#include <Adafruit_AHTX0.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <soc/rtc.h>
#include <WiFiClient.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <Adafruit_MCP23X17.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <GyverOLED.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_BME280.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <algorithm>
#include <DNSServer.h>  // Для работы с DNS сервером
#include <Ticker.h>
#include <set>
#include "ram_saver.h"
Ticker restartTicker;
String chipStr;
// Global HTTP client
HTTPClient http;
AsyncWebServer server(80);
AsyncEventSource events("/events");  // SSE endpoint по адресу /events
String indexHtml;
bool waitingSecondOta = false;

std::set<String> notFoundCache;
std::set<String> FoundCache;
// WiFi client configuration
const int WIFI_CONNECT_TIMEOUT = 5000;    // 5 seconds
const int WIFI_RESPONSE_TIMEOUT = 10000;  // 10 seconds
static unsigned long lastDataTime = 0;
static bool otaTimedOut = false;
static bool updateBegun = false;
// Initialize HTTP client with timeouts
void initWiFiClient()
{
    http.setTimeout(WIFI_RESPONSE_TIMEOUT);
    http.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
}

// Function to clean up WiFi client
void cleanupWiFiClient()
{
    if (http.connected())
    {
        http.end();
    }
    http.end();
}
#include <lwip/dns.h>
#include <sstream>
#include <cmath>
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <RunningMedian.h>
#include <Preferences.h>
#include <AM232X.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ccs811.h"  // CCS811 library
#include <VL53L0X.h>
#undef NOP
#include "HX710B.h"

#include <nvs_flash.h>
#include <variables.h>
#include <functions.h>

#include "esp_adc_cal.h"
#define ONE_WIRE_BUS 23  // Порт 1-Wire
#include <Wire.h>        // Шина I2C
#define I2C_SDA 21       // SDA
#define I2C_SCL 22       // SCL
bool isAPMode = false;
int stack_size = 4096;
String httpAuthUser, httpAuthPass;
uint8_t buff[128] = {0};
WiFiUDP udpClient;

#include <Syslog.h>
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);
// Обработчик загрузки файла
bool shouldReboot = false;

void syslog_ng(String x)
{
    if (x == nullptr)
    {
        Serial.println("Ошибка: syslog_ng  x null");
        return;
    }

    events.send(x.c_str(), "log", millis());

    x = fFTS(float(millis()) / 1000, 3) + "s " + x;
    if (!isAPMode && WiFi.status() == WL_CONNECTED)
    {
        syslog.log(LOG_INFO, x.c_str());
    }

    Serial.println(x);
}

void syslog_err(String x)
{
    if (x == nullptr)
    {
        Serial.println("Ошибка: syslog_err  x null");
        return;
    }
    events.send(x.c_str(), "log", millis());

    x = fFTS(float(millis()) / 1000, 3) + "s " + x;
    if (!isAPMode && WiFi.status() == WL_CONNECTED)
    {
        syslog.log(LOG_ERR, x.c_str());
    }

    Serial.println(x);
}

// Настройки ADC
esp_adc_cal_characteristics_t *adc_chars;
const adc_bits_width_t width = ADC_WIDTH_BIT_12;  // Разрядность (12 бит)
const adc_atten_t atten = ADC_ATTEN_DB_12;        // Коэффициент ослабления (0–3.3 В)

// Указываем каналы для ADC1
const adc1_channel_t channels[] = {
    ADC1_CHANNEL_5,  // GPIO33
    ADC1_CHANNEL_6,  // GPIO34
    ADC1_CHANNEL_7   // GPIO35
};
const size_t channel_count = sizeof(channels) / sizeof(channels[0]);

struct TaskParams
{
    const char *taskName;
    void (*taskFunction)();
    unsigned long repeatInterval;
    SemaphoreHandle_t xSemaphore;
    unsigned long busyCounter;
};

struct TaskInfo
{
    const char *taskName;
    unsigned long lastExecutionTime;
    unsigned long totalExecutionTime;
    unsigned int executionCount;
    unsigned int minFreeStackSize;
    SemaphoreHandle_t xSemaphore;
};
#define MAX_TASKS 40
#define US_SDA 13  // SDA
#define US_SCL 14  // SCL

TaskInfo tasks[MAX_TASKS];
int taskCount = 0;

uint8_t LCDaddr = 0x3c;
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
SemaphoreHandle_t xSemaphore_C = NULL;
SemaphoreHandle_t xSemaphoreX_WIRE = NULL;
Preferences preferences;
Preferences config_preferences;
extern AsyncWebServer server;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t mqttReconnectTimerHa;
TimerHandle_t wifiReconnectTimer;
TaskHandle_t xHandleUpdate = NULL;

AsyncMqttClient mqttClient;
AsyncMqttClient mqttClientHA;
WiFiEventId_t wifiConnectHandler;
String clientId, clientIdHa;
String sessionToken = "";
#define MAX_MQTT_QUEUE_SIZE 50

#include <map>
#include <params.h>
#include <driver/adc.h>

#include <queue>

#define SCK_PIN 13
#define SDI_PIN 14
#define ADS1115_MiddleCount 10000  // 6236ms за 10 тыс усреднений
#include <debug_info_new.h>
#include <http/funcs.h>
// Структура для управления SSE клиентами
struct SSEClient
{
    WiFiClient client;
    bool connected;
};

// Массив SSE клиентов
#define MAX_SSE_CLIENTS 4
SSEClient sseClients[MAX_SSE_CLIENTS];
int sseClientCount = 0;

#include <dev/ec/old_adc/wega-adc.h>

void removeSSEClient(const WiFiClient &client)
{
    for (int i = 0; i < sseClientCount; i++)
    {
        if (sseClients[i].client == client)
        {
            syslog_ng("Removing SSE client");

            // Остановить и очистить клиент
            sseClients[i].client.stop();
            sseClients[i].client = WiFiClient();
            sseClients[i].connected = false;

            // Сдвигаем оставшиеся клиенты
            for (int j = i; j < sseClientCount - 1; j++)
            {
                sseClients[j] = sseClients[j + 1];
            }
            sseClientCount--;
            break;
        }
    }
}

// Функция для добавления нового SSE клиента
void addSSEClient(const WiFiClient &client)
{
    // Проверяем, не добавляем ли мы существующего клиента
    for (int i = 0; i < sseClientCount; i++)
    {
        if (sseClients[i].client == client)
        {
            syslog_ng("Client already exists");
            return;
        }
    }

    if (sseClientCount < MAX_SSE_CLIENTS)
    {
        sseClients[sseClientCount].client = client;
        sseClients[sseClientCount].connected = true;
        sseClientCount++;
        syslog_ng(String("SSE client added, total: ") + sseClientCount);
    }
    else
    {
        syslog_err("SSE client limit reached");
    }
}

// Function to send message to all connected SSE clients
void sendSSEMessage(const String &message)
{
    if (message.length() == 0)
    {
        syslog_err("Empty message in sendSSEMessage");
        return;
    }

    // Format the message with event type
    String sseMessage = "event: message\n";
    sseMessage += "data: " + message + "\n\n";

    // Send to all connected clients
    for (int i = 0; i < sseClientCount; i++)
    {
        if (sseClients[i].connected && sseClients[i].client.connected())
        {
            syslog_ng(String("Sending to client ") + i);

            // Try to send message with retries
            int retries = 3;
            bool messageSent = false;
            while (retries > 0)
            {
                sseClients[i].client.print(sseMessage);
                sseClients[i].client.flush();

                // Проверяем успешность отправки через available()
                if (sseClients[i].client.available() == 0)
                {
                    messageSent = true;
                    syslog_ng(String("Message sent to client ") + i);
                    break;
                }
                retries--;
                syslog_err(String("Failed to send message to client ") + i + String(", retries left: ") + retries);
            }

            if (!messageSent)
            {
                syslog_err(String("Failed to send message to client ") + i + String(" after retries"));
                sseClients[i].connected = false;
            }

            if (retries == 0)
            {
                syslog_err(String("Failed to send message to client ") + i + String(" after retries"));
                sseClients[i].connected = false;
            }
        }
        else
        {
            syslog_err(String("Client ") + i + String(" disconnected"));
            // Client disconnected, remove from list
            removeSSEClient(sseClients[i].client);
            i--;  // Adjust index since we removed an element
        }
    }
}

// Function to clean up disconnected SSE clients
void cleanupSSEClients()
{
    syslog_ng("Starting SSE client cleanup");
    syslog_ng(String("Total clients: ") + sseClientCount);

    // Create a temporary array of active clients
    SSEClient activeClients[MAX_SSE_CLIENTS];
    int activeCount = 0;

    // First pass: collect active clients
    for (int i = 0; i < sseClientCount; i++)
    {
        if (sseClients[i].connected && sseClients[i].client.connected())
        {
            // Проверяем, что клиент активен
            if (sseClients[i].client.available() > 0)
            {
                sseClients[i].client.read();  // Очищаем входной буфер
            }
            activeClients[activeCount++] = sseClients[i];
            syslog_ng(String("Client ") + i + String(" is active"));
        }
        else
        {
            syslog_ng(String("Found disconnected client ") + i);
            sseClients[i].client.stop();  // Clean up the client
            sseClients[i].connected = false;
        }
    }

    // Second pass: update the main array
    if (activeCount < sseClientCount)
    {
        for (int i = 0; i < activeCount; i++)
        {
            sseClients[i] = activeClients[i];
        }
        sseClientCount = activeCount;
        syslog_ng(String("Active clients after cleanup: ") + sseClientCount);
    }
    else
    {
        syslog_ng("No disconnected clients found");
    }
}

// Функция для удаления SSE клиента
void TaskTemplate(void *params)
{
    TaskParams *taskParams = (TaskParams *)params;
    taskParams->busyCounter = 0;
    syslog_ng(String("TaskTemplate " + String(taskParams->taskName) + " received task ") +
              String(taskParams->taskName));
    TaskInfo *currentTask = nullptr;
    int counter = 0;
    unsigned long lastExecutionTime = millis();
    for (int i = 0; i < taskCount; i++)
    {
        if (tasks[i].taskName != NULL && taskParams->taskName != NULL &&
            strcmp(tasks[i].taskName, taskParams->taskName) == 0)
        {
            {
                counter = i;
                syslog_ng("TaskTemplate already found task! " + String(taskParams->taskName) + " TaskTemplateCounter " +
                          String(counter));
                currentTask = &tasks[i];
                break;
            }
        }
    }

    if (currentTask == nullptr && taskCount < MAX_TASKS)
    {
        syslog_ng("Adding task " + String(taskParams->taskName) + " " + String(taskParams->taskName) +
                  " to tasks array.");
        tasks[taskCount].taskName = strdup(taskParams->taskName);
        tasks[taskCount].lastExecutionTime = 0;
        tasks[taskCount].totalExecutionTime = 0;
        tasks[taskCount].executionCount = 0;

        tasks[taskCount].minFreeStackSize = uxTaskGetStackHighWaterMark(NULL);
        tasks[taskCount].xSemaphore = taskParams->xSemaphore;
        currentTask = &tasks[taskCount];
        counter = taskCount;
        taskCount++;
    }

    if (taskParams == nullptr)
    {
        syslog_ng("TaskTemplate  " + String(taskParams->taskName) +
                  " taskParams == nullptr   received invalid parameters, deleting task counter " + String(counter) +
                  " taskParams->taskName " + String(taskParams->taskName));
        vTaskDelete(NULL);
    }

    if (taskParams->xSemaphore == nullptr)
    {
        syslog_ng("TaskTemplate  xSemaphore == nullptr  received invalid parameters, deleting task counter " +
                  String(counter) + " taskParams->taskName " + String(taskParams->taskName));
        vTaskDelete(NULL);
    }

    if (taskParams->taskName == nullptr)
    {
        syslog_ng("TaskTemplate  taskName == nullptr  received invalid parameters, deleting task counter " +
                  String(counter) + " taskParams->taskName " + String(taskParams->taskName));
        vTaskDelete(NULL);
    }

    for (;;)
    {
        if (OtaStart == true)
        {
            syslog_ng(String(taskParams->taskName) + " OTA start detected, deleting task");
            // Free any allocated resources here if needed

            vTaskDelete(NULL);  // Delete the current task
            return;             // This line is technically unreachable but good practice
        }

        unsigned long currentTime = millis();
        unsigned long timeSinceLastExecution = currentTime - lastExecutionTime;

        if (timeSinceLastExecution >= taskParams->repeatInterval)
        {
            if (log_debug)
            {
                syslog_ng("TaskTemplate  " + String(taskParams->taskName) + " Starting vTaskDelay counter " +
                          String(counter) + "taskName" + String(taskParams->taskName));
                syslog_ng("TaskTemplate  " + String(taskParams->taskName) + " Starting taskName" +
                          String(taskParams->taskName));
                syslog_ng("TaskTemplate  " + String(taskParams->taskName) +
                          " timeSinceLastExecution >= taskParams->repeatInterval Starting taskName: " +
                          String(taskParams->taskName));
            }
            if (xSemaphoreTake(taskParams->xSemaphore, (TickType_t)100) == pdTRUE)
            {
                taskParams->busyCounter = 0;
                lastExecutionTime = currentTime;
                unsigned long startTime = millis();

                syslog_ng(" Start  " + String(taskParams->taskName) + " TaskTemplateCounter " + String(counter) +
                          String(taskParams->taskName));

                taskParams->taskFunction();

                unsigned long endTime = millis();
                unsigned long executionTime = endTime - startTime;

                currentTask->lastExecutionTime = executionTime;
                currentTask->totalExecutionTime += executionTime;
                currentTask->executionCount++;
                unsigned int currentStackSize = uxTaskGetStackHighWaterMark(NULL);
                if (currentStackSize < currentTask->minFreeStackSize)
                {
                    currentTask->minFreeStackSize = currentStackSize;
                }

                syslog_ng(String(taskParams->taskName) + " End, currentStackSize " + String(currentStackSize) +
                          " Execution time: " + String(executionTime) + "ms");

                xSemaphoreGive(taskParams->xSemaphore);
                vTaskDelay(taskParams->repeatInterval / portTICK_PERIOD_MS);
            }
            else
            {
                taskParams->busyCounter++;
                if (taskParams->busyCounter > 15)
                {
                    syslog_ng(String(taskParams->taskName) + " symofore busy ");
                }

                vTaskDelay(random(500, 1501) / portTICK_PERIOD_MS);
            }
        }
        else
        {
            vTaskDelay(random(500, 1501) / portTICK_PERIOD_MS);
        }
    }

    syslog_ng(String(taskParams->taskName) + " EXIT Task");
}
static int reconnectDelay = 1000;  // Start with 1 second
static int reconnectAttempts = 0;
long phvalue;
// Очередь для хранения сообщений
std::queue<std::pair<const char *, const char *>> messageQueue;
float saved_dist = 0;
int saved_counter = 0;
float ECDoserLimit;
float ECDoserValueA;
float ECDoserValueB;
int ECDoserInterval, ECDoserEnable, e_ha, port_ha;
#define MAX_DS18B20_SENSORS 10

int E_dallas_kalman = 0;
float dallas_mea_e = 1;
float dallas_est_e = 1;
float dallas_q = 0.3;

struct SensorData
{
    DeviceAddress address;
    float temperature;
    String key;

    // Преобразование адреса в строку
    String addressToString() const
    {
        String str = "";
        for (int i = 0; i < 8; i++)
        {
            if (address[i] < 16) str += "0";  // Добавляем ведущий ноль для однозначных значений
            str += String(address[i], HEX);
            if (i < 7) str += ":";
        }
        return str;
    }

    // Преобразование строки в адрес
    bool stringToAddress(const String &str)
    {
        int pos = 0;
        for (int i = 0; i < 8; i++)
        {
            int colonIndex = str.indexOf(':', pos);
            String byteStr = str.substring(pos, colonIndex);
            address[i] = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
            pos = colonIndex + 1;
        }
        return true;
    }

    GKalman KalmanDallasTmp;
    SensorData() : KalmanDallasTmp(dallas_mea_e, dallas_est_e, dallas_q) {}
};

struct DallasAdresses
{
    DeviceAddress address;
    String addressToString() const
    {
        String str = "";
        for (int i = 0; i < 8; i++)
        {
            if (address[i] < 16) str += "0";  // Добавляем ведущий ноль для однозначных значений
            str += String(address[i], HEX);
            if (i < 7) str += ":";
        }
        return str;
    }
};

SensorData sensorArray[MAX_DS18B20_SENSORS];
DallasAdresses dallasAdresses[MAX_DS18B20_SENSORS];

String rootTempAddressString;
String WNTCAddressString;

// Преобразование строки в DeviceAddress
bool stringToDeviceAddress(const String &str, DeviceAddress &deviceAddress)
{
    int pos = 0;
    for (int i = 0; i < 8; i++)
    {
        int colonIndex = str.indexOf(':', pos);
        String byteStr = str.substring(pos, colonIndex);
        deviceAddress[i] = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
        pos = colonIndex + 1;
    }
    return true;
}

bool validateAndConvertIP(const char *str, IPAddress &ip)
{
    int parts[4];
    if (sscanf(str, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]) != 4)
    {
        return false;  // Invalid format
    }
    for (int i = 0; i < 4; i++)
    {
        if (parts[i] < 0 || parts[i] > 255)
        {
            return false;  // Out of range
        }
    }
    ip = IPAddress(parts[0], parts[1], parts[2], parts[3]);
    return true;
}

// Преобразование DeviceAddress в строку
String deviceAddressToString(const DeviceAddress &deviceAddress)
{
    String str = "";
    for (int i = 0; i < 8; i++)
    {
        if (deviceAddress[i] < 16) str += "0";  // Добавляем ведущий ноль
        str += String(deviceAddress[i], HEX);
        if (i < 7) str += ":";
    }
    return str;
}

void enqueueMessage(const char *topic, const char *payload, String key = "", String mqtt_type = "all")
{
    // Проверка на NULL для topic и payload
    if (topic == nullptr || payload == nullptr)
    {
        Serial.println("Ошибка: topic или payload равен NULL");
        return;
    }
#if defined(ENABLE_PONICS_ONLINE)
    if (enable_ponics_online)
    {
        if (mqttClient.connected() and (mqtt_type == "usual" or mqtt_type == "all") and WiFi.status() == WL_CONNECTED)
        {
            int packet_id = mqttClient.publish(topic, 0, false, payload);

            String e = String(topic) + ":::" + String(payload);
            events.send(e.c_str(), "mqtt-ponics", millis());

            if (packet_id == 0)
            {
                int payloadSize = strlen(payload);
                syslog_ng("Payload size: " + String(payloadSize));
                syslog_ng(" error enqueueMessage: publish packet_id " + String(packet_id) + " topic " + String(topic) +
                          " payload " + String(payload));
            }
        }
    }
#endif
    if (mqttClientHA.connected() and (mqtt_type == "ha" or mqtt_type == "all") and WiFi.status() == WL_CONNECTED)
    {
        int packet_id = mqttClientHA.publish(topic, 0, false, payload);

        String e = String(topic) + ":::" + String(payload);
        events.send(e.c_str(), "mqtt-ha", millis());
        if (packet_id == 0)
        {
            int payloadSize = strlen(payload);

            syslog_ng("Payload size: " + String(payloadSize));
            syslog_ng(" error enqueueMessage: publish packet_id " + String(packet_id) + " topic " + String(topic) +
                      " payload " + String(payload));
        }
    }
}

// Функция для вычисления насыщенного давления пара
float calculateSVP(float temperature) { return 0.61078 * exp((17.27 * temperature) / (temperature + 237.3)); }

// Функция для вычисления VPD
float calculateVPD(float temperature, float humidity)
{
    float svp = calculateSVP(temperature);
    return svp - (svp * (humidity / 100.0));
}

String getStyle(float vpd, float optimal, float goodLower, float goodUpper)
{
    if (vpd < goodLower || vpd > goodUpper)
    {
        return "color: red;";  // Если VPD вне допустимого диапазона
    }
    if (fabs(vpd - optimal) < 0.1)
    {
        return "color: green;";  // Если VPD близок к оптимальному значению
    }
    return "color: orange;";  // Если VPD в допустимом диапазоне, но не оптимальный
}

struct ParamSettings
{
    int compare;
    float action;
    String url;
    ParamSettings(int comp = 0, float act = 0, String u = "") : compare(comp), action(act), url(u) {}
};

std::map<String, ParamSettings> paramSettingsMap;

void setParamSettings(const String &key, float compare, int action, String url)
{
    ParamSettings settings(compare, action, url);
    paramSettingsMap[key] = settings;  // Добавляем или обновляем настройки в map
}

void publish_parameter(const String &key, float value, int precision, int timescale)
{
    // Отправляем RSSI значение если это первый вызов

    // Проверка на пустой ключ
    if (key.length() == 0)
    {
        syslog_ng("ERROR: Parameter key is empty.");
        return;  // Выходим, если ключ пустой
    }

    if (timescale == 1)
    {
        String topic = mqttPrefix + timescale_prefix + key;  // Используем соответствующий префикс темы
        char valueStr[32];
        if (log_debug) syslog_ng("publish_parameter topic " + topic + " value: " + String(value));

        dtostrf(value, 1, precision, valueStr);  // Преобразуем float в строку с заданной точностью

        if (log_debug) syslog_ng("published topic " + topic + " value: " + String(value));
        enqueueMessage(topic.c_str(), valueStr, key);
    }
    else
    {
        String topic = mqttPrefix + data_prefix + key;  // Используем соответствующий префикс темы
        char valueStr[32];

        dtostrf(value, 1, precision, valueStr);  // Преобразуем float в строку с заданной точностью
        enqueueMessage(topic.c_str(), valueStr, key);
    }
}

void publish_parameter(const String &key, const String &value, int timescale)
{
    // Проверка на пустой ключ
    if (key.length() == 0)
    {
        syslog_ng("ERROR: Parameter key is empty.");
        return;  // Выходим, если ключ пустой
    }

    // Проверка на пустое значение
    if (value.length() == 0)
    {
        syslog_ng("ERROR: Parameter value is empty for key: " + key);
        return;  // Выходим, если значение пустое
    }

    if (timescale == 1)
    {
        String topic = mqttPrefix + timescale_prefix + key;  // Используем соответствующий префикс темы
        if (log_debug) syslog_ng("publish_parameter topic " + topic + " value: " + String(value));

        enqueueMessage(topic.c_str(), String(value).c_str(), key);
        if (log_debug) syslog_ng("published topic " + topic + " value: " + String(value));
    }
    else
    {
        String topic = mqttPrefix + data_prefix + key;  // Используем соответствующий префикс темы

        enqueueMessage(topic.c_str(), String(value).c_str(), key);
    }
}
void get_ph()
{
    if (pHmV)
    {
        float pa = -(-px1 * py3 + px1 * py2 - px3 * py2 + py3 * px2 + py1 * px3 - py1 * px2) /
                   (-pow(px1, 2) * px3 + pow(px1, 2) * px2 - px1 * pow(px2, 2) + px1 * pow(px3, 2) - pow(px3, 2) * px2 +
                    px3 * pow(px2, 2));
        float pb = (py3 * pow(px2, 2) - pow(px2, 2) * py1 + pow(px3, 2) * py1 + py2 * pow(px1, 2) - py3 * pow(px1, 2) -
                    py2 * pow(px3, 2)) /
                   ((-px3 + px2) * (px2 * px3 - px2 * px1 + pow(px1, 2) - px3 * px1));
        float pc = (py3 * pow(px1, 2) * px2 - py2 * pow(px1, 2) * px3 - pow(px2, 2) * px1 * py3 +
                    pow(px3, 2) * px1 * py2 + pow(px2, 2) * py1 * px3 - pow(px3, 2) * py1 * px2) /
                   ((-px3 + px2) * (px2 * px3 - px2 * px1 + pow(px1, 2) - px3 * px1));
        wpH = pa * pow(pHmV, 2) + pb * pHmV + pc + pH_lkorr;
        preferences.putFloat("wpH", wpH);
    }
}

void get_ec()
{
    if (Ap < 4095 and Ap > 0 and An > 0 and An < 4095)
    {
        R2p = (-An * R1 - An * Rx1 + R1 * Dr + Rx1 * Dr) / An;
        R2n = -(-Ap * R1 - Ap * Rx2 + Rx2 * Dr) / (-Ap + Dr);

        wR2 = (R2p + R2n) / 2;

        syslog_ng("make_raschet before calculation: An: " + fFTS(An, 3) + ", R1: " + fFTS(R1, 3) +
                  ", Rx1: " + fFTS(Rx1, 3) + ", Dr: " + fFTS(Dr, 3) + ", Ap: " + fFTS(Ap, 3) +
                  ", Rx2: " + fFTS(Rx2, 3) + ", R2p: " + fFTS(R2p, 3) + ", R2n: " + fFTS(R2n, 3) +
                  ", wR2: " + fFTS(wR2, 3) + ", An: " + fFTS(An, 3));

        if (wR2 > 0)
        {
            float eb = (-log10(ec1 / ec2)) / (log10(ex2 / ex1));
            float ea = pow(ex1, -eb) * ec1;
            ec_notermo = ea * pow(wR2, eb);
            syslog_ng("make_raschet eb: " + fFTS(eb, 3) + " ec: " + fFTS(ec_notermo, 3) + "ea: " + fFTS(ea, 3) +
                      " wNTC: " + fFTS(wNTC, 3));
            wEC_ususal = ec_notermo / (1 + kt * (wNTC - 25)) + eckorr;
            syslog_ng("EC_KAL_E: " + fFTS(EC_KAL_E, 3));

            if (EC_KAL_E == 1)
            {
                EC_Kalman = KalmanEC.filtered(wEC_ususal);
                ec_notermo_kalman = KalmanEcUsual.filtered(ec_notermo);
                if (first_EC)
                {
                    first_EC = false;
                    int iiii = 0;
                    while (iiii < 45)
                    {
                        iiii += 1;
                        EC_Kalman = KalmanEC.filtered(wEC_ususal);
                        ec_notermo_kalman = KalmanEcUsual.filtered(ec_notermo);
                    }
                }
                wEC = wEC_ususal;
                ec_notermo = ec_notermo_kalman;
            }
            else
            {
                wEC = wEC_ususal;
            }
        }
        else
        {
            if (wR2 == 0)
            {
                wEC = 0;
                ec_notermo = 0;
                ec_notermo_kalman = 0;
            }
        }
    }
    else
    {
        wEC = 0;
        ec_notermo = 0;
        ec_notermo_kalman = 0;
    }
}

Group groups[] = {
    {"Долив воды",
     7,
     {
         {"ECStabEnable", "EC вкл коррекцию водой(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"ECStabPomp", "Помпа Пин", Param::INT, .defaultInt = 0},
         {"ECStabValue", "EC нужный", Param::FLOAT, .defaultFloat = 2.5},
         {"ECStabTime", "Время работы секунды", Param::INT, .defaultInt = 20},
         {"ECStabInterval", "Время отдыха секунды", Param::INT, .defaultInt = 180},
         {"ECStabMinDist", "Уровень минимальный литры", Param::FLOAT, .defaultFloat = 5},
         {"ECStabMaxDist", "Уровень максимальный литры", Param::FLOAT, .defaultFloat = 50},
     }},
    {"Давление корней",
     12,
     {
         {"RootPomp", "Вкл(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"SelectedRP", "Помпа Пин", Param::INT, .defaultInt = 0},
         {"RootPwdOn", "Регулировать ШИМом(0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"RootPwdMax", "ШИМ максимум", Param::INT, .defaultInt = 254},
         {"RootPwdMin", "ШИМ минимум", Param::INT, .defaultInt = 0},
         {"RootDistMin", "Минимальное расстояние", Param::INT, .defaultInt = 0},
         {"PwdStepUp", "ШИМ шаг увеличения", Param::INT, .defaultInt = 1},
         {"PwdStepDown", "ШИМ шаг уменьшения", Param::INT, .defaultInt = 10},
         {"RootPwdTemp", "ШИМ t контроль воздух-коррни", Param::INT, .defaultInt = 0},
         {"PwdNtcRoot", "ШИМ t контроль корни-бак", Param::FLOAT, .defaultFloat = -1},
         {"PwdDifPR", "ШИМ мин свет люкс", Param::INT, .defaultInt = 0},
         {"MinKickPWD", "Минимальный ШИМ помпы без пинка ", Param::INT, .defaultInt = 150},
     }},

    {"Ночной режим",
     3,
     {
         {"PompNightEnable", "Ночной режим(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"PompNightPomp", "Помпа Пин", Param::INT, .defaultInt = 0},
         {"MinLightLevel", "Минимальный уровень датчика света для отключения помпы", Param::INT, .defaultInt = 10},
     }},
    {"WIFI",
     2,
     {
         {"ssid", "Имя сети", Param::STRING, .defaultString = ssid.c_str()},
         {"password", "Пароль сети", Param::STRING, .defaultString = password.c_str()},
     }},

    {"Домашний помощник",
     5,
     {
         {"e_ha", "ВКЛ интеграцию(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"port_ha", "Порт мктт", Param::INT, .defaultInt = 1883},
         {"a_ha", "ip мктт", Param::STRING, .defaultString = a_ha.c_str()},
         {"u_ha", "имя пользоветеля мктт", Param::STRING, .defaultString = ""},
         {"p_ha", "пароль мктт", Param::STRING, .defaultString = ""},

     }},
    {"Сглаживание",
     16,
     {

         {"NTC_KAL_E", "Включить NTC сглаживание(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"ntc_mea_e", "NTC measure", Param::FLOAT, .defaultFloat = 40},
         {"ntc_est_e", "NTC estimate", Param::FLOAT, .defaultFloat = 5},
         {"ntc_q", "NTC сила сглаживания ", Param::FLOAT, .defaultFloat = 0.5},

         {"EC_KAL_E", "Включить EC сглаживание(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"ec_mea_e", "EC measure", Param::FLOAT, .defaultFloat = 1},
         {"ec_est_e", "EC estimate", Param::FLOAT, .defaultFloat = 1},
         {"ec_q", "EC сила сглаживания", Param::FLOAT, .defaultFloat = 0.3},

         {"DIST_KAL_E", "Включить Уровень сглаживание(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"dist_mea_e", "DIST measure", Param::FLOAT, .defaultFloat = 10},
         {"dist_est_e", "DIST estimate", Param::FLOAT, .defaultFloat = 10},
         {"dist_q", "DIST сила сглаживания", Param::FLOAT, .defaultFloat = 0.1},

         {"E_d_k", "Включить Dallas сглаживание корни-бак(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"d_mea_e", "Dallas measure", Param::FLOAT, .defaultFloat = 1},
         {"d_est_e", "Dallas estimate", Param::FLOAT, .defaultFloat = 1},
         {"d_q", "Dallas сила сглаживания", Param::FLOAT, .defaultFloat = 0.3},

     }},

    {"Настройки",
     13,
     {
         {"UPDATE_URL", "Ссылка на прошивку", Param::STRING, .defaultString = UPDATE_URL.c_str()},
         {"httpAU", "Логин интерфейса", Param::STRING, .defaultString = httpAuthUser.c_str()},
         {"httpAP", "Пароль интерфейса", Param::STRING, .defaultString = httpAuthPass.c_str()},
         {"epo", "Вкл мктт поникс(0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"epo_l", "Вкл лог поникс(0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"update_token", "Ключ обновления", Param::STRING, .defaultString = update_token.c_str()},
         {"HOSTNAME", "Имя хоста", Param::STRING, .defaultString = HOSTNAME.c_str()},
         {"vpdstage", "VPD стадия", Param::STRING, .defaultString = vpdstage.c_str()},
         {"disable_ntc", "Отключить NTC(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"calE", "Включить калибровку(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"change_pins", "поменять пины US025(HCR04)(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"clear_pref", "Сброс ВСЕХ настроек кроме wifi(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"vZapas", "Запас в трубе раствора", Param::FLOAT, .defaultFloat = 9.5},

     }},

    {"Дозер A",
     11,
     {
         {"StPumpA_On", "Вкл(0-off, 1-on)", Param::INT, .defaultFloat = 0},
         {"SetPumpA_Ml", "Налить мл разово", Param::FLOAT, .defaultFloat = 0},
         {"StPumpA_Del", "Отдых мс", Param::INT, .defaultInt = 700},
         {"StPumpA_Ret", "Шаг мс", Param::INT, .defaultInt = 700},

         {"StPumpA_cStepMl", "Число шагов на объем для калибровки", Param::FLOAT, .defaultFloat = 1000},
         {"StPumpA_cMl", "Объем в мл для калибровки", Param::FLOAT, .defaultFloat = 1},
         {"StPumpA_cStep", "Число шагов за 1 цикл", Param::FLOAT, .defaultFloat = 1000},

         {"StPumpA_A", "Пин 1", Param::INT, .defaultInt = 4},
         {"StPumpA_B", "Пин 2", Param::INT, .defaultInt = 5},
         {"StPumpA_C", "Пин 3", Param::INT, .defaultInt = 6},
         {"StPumpA_D", "Пин 4", Param::INT, .defaultInt = 7},

     }},

    {"Дозер B",
     11,
     {
         {"StPumpB_On", "Вкл(0-off, 1-on)", Param::INT, .defaultFloat = 0},
         {"SetPumpB_Ml", "Налить мл разово(0-off, 1-on)", Param::FLOAT, .defaultFloat = 0},
         {"StPumpB_Del", "Отдых мс", Param::INT, .defaultInt = 700},
         {"StPumpB_Ret", "Шаг мс", Param::INT, .defaultInt = 700},

         {"StPumpB_cStepMl", "Число шагов на объем для калибровки", Param::FLOAT, .defaultFloat = 1000},
         {"StPumpB_cMl", "Объем в мл для калибровки", Param::FLOAT, .defaultFloat = 1},
         {"StPumpB_cStep", "Число шагов за 1 цикл", Param::FLOAT, .defaultFloat = 1000},
         {"StPumpB_A", "Пин 1", Param::INT, .defaultInt = 8},
         {"StPumpB_B", "Пин 2", Param::INT, .defaultInt = 9},
         {"StPumpB_C", "Пин 3", Param::INT, .defaultInt = 10},
         {"StPumpB_D", "Пин 4", Param::INT, .defaultInt = 11},
     }},
    {"Долив концентрата",
     5,
     {
         {"ECDoserEnable", "Дозер концентратов(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"ECDoserLimit", "ЕС максимальный", Param::FLOAT, .defaultFloat = 1.5},
         {"ECDoserValueA", "Помпа А МЛ за одну подачу", Param::FLOAT, .defaultFloat = 1},
         {"ECDoserValueB", "Помпа Б МЛ за одну подачу", Param::FLOAT, .defaultFloat = 1},
         {"ECDoserInterval", "Интервал коррекции", Param::INT, .defaultInt = 600},
     }},
    {"ШИМ группа 1",
     11,
     {
         {"PWDport1", "ШИМ порт ESP", Param::INT, .defaultInt = 16},
         {"PWD1", "Значение PWD ШИМ > 255-откл", Param::INT, .defaultInt = 256},
         {"FREQ1", "Частота", Param::INT, .defaultInt = 5000},
         {"DRV1_A_State", "DRV1_A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV1_B_State", "DRV1_B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_A_State", "DRV2_A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_B_State", "DRV2_B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_A_State", "DRV3_A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_B_State", "DRV3_B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_A_State", "DRV4_A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_B_State", "DRV4_B (0-off, 1-on)", Param::INT, .defaultInt = 0},
     }},

    {"ШИМ группа 2",
     11,
     {
         {"PWDport2", "ШИМ порт ESP", Param::INT, .defaultInt = 17},
         {"PWD2", "Значение PWD ШИМ > 255-откл", Param::INT, .defaultInt = 255},
         {"FREQ2", "Частота", Param::INT, .defaultInt = 5000},
         {"DRV1_C_State", "DRV1_C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV1_D_State", "DRV1_D (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_C_State", "DRV2_C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_D_State", "DRV2_D (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_C_State", "DRV3_C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_D_State", "DRV3_D (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_C_State", "DRV4_C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_D_State", "DRV4_D (0-off, 1-on)", Param::INT, .defaultInt = 0},

     }},

    {"Даллас",
     2,
     {
         {"RootTempAddress", "Адрес Темп корней", Param::STRING, .defaultString = ""},
         {"WNTCAddress", "Адрес Темп бака", Param::STRING, .defaultString = ""},
     }},

    {"Периодика",
     6,
     {
         {"RDEnable", "Включить периодику (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"RDDelayOn", "Сколько работать", Param::INT, .defaultInt = 60},
         {"RDDelayOff", "Сколько ждать", Param::INT, .defaultInt = 300},
         {"RDSelectedRP", "Пин помпы", Param::INT, .defaultInt = 0},
         {"RDPWD", "Значение Pulse Width Modulation (PWD). ШИМ периодики", Param::INT, .defaultInt = 256},
         {"RDWorkNight", "Работать 0-всегда, 1-ночью", Param::INT, .defaultInt = 1},

     }},

    {"Пинок для насоса",
     20,
     {
         {"KickUpMax", "Максимум мощности пинка", Param::INT, .defaultInt = 255},
         {"KickUpStrart", "Начальная можность пинка", Param::INT, .defaultInt = 10},
         {"KickUpTime", "Время пинка в миллисекундах", Param::INT, .defaultInt = 300},
         {"KickOnce", "Разово пнуть (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV1_A_PK_On", "Вкючить пинок для DRV1 A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV1_B_PK_On", "Вкючить пинок для DRV1 B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV1_C_PK_On", "Вкючить пинок для DRV1 C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV1_D_PK_On", "Вкючить пинок для DRV1 D (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_A_PK_On", "Вкючить пинок для DRV2 A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_B_PK_On", "Вкючить пинок для DRV2 B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_C_PK_On", "Вкючить пинок для DRV2 C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV2_D_PK_On", "Вкючить пинок для DRV2 D (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_A_PK_On", "Вкючить пинок для DRV3 A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_B_PK_On", "Вкючить пинок для DRV3 B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_C_PK_On", "Вкючить пинок для DRV3 C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV3_D_PK_On", "Вкючить пинок для DRV3 D (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_A_PK_On", "Вкючить пинок для DRV4 A (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_B_PK_On", "Вкючить пинок для DRV4 B (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_C_PK_On", "Вкючить пинок для DRV4 C (0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"DRV4_D_PK_On", "Вкючить пинок для DRV4 D (0-off, 1-on)", Param::INT, .defaultInt = 0},
     }},

    {"Калибровка",
     22,
     {

         {"VccRU", "Параметр RAW VCC для расчета своего VCC", Param::FLOAT, .defaultFloat = VccRawUser},
         {"tR_DAC", "Параметр tR_DAC для NTC", Param::FLOAT, .defaultFloat = tR_DAC},
         {"tR_B", "Параметр tR_B для NTC", Param::FLOAT, .defaultFloat = tR_B},
         {"tR_val_korr", "Линейная коррекция NTC", Param::FLOAT, .defaultFloat = tR_val_korr},
         {"tR_type", "Тип датчика температуры NTC", Param::STRING, .defaultString = tR_type.c_str()},

         {"R1", "Сопротивление R1 для EC", Param::FLOAT, .defaultFloat = R1},
         {"Rx1", "Сопротивление Rx1 для EC", Param::FLOAT, .defaultFloat = Rx1},
         {"Rx2", "Сопротивление Rx2 для EC", Param::FLOAT, .defaultFloat = Rx2},
         {"Dr", "Параметр Dr EC", Param::FLOAT, .defaultFloat = Dr},
         {"eckorr", "Коррекция EC", Param::FLOAT, .defaultFloat = eckorr},
         {"ec1", "Коэффициент ec1", Param::FLOAT, .defaultFloat = ec1},
         {"ec2", "Коэффициент ec2", Param::FLOAT, .defaultFloat = ec2},
         {"ex1", "Коэффициент ex1 для EC", Param::FLOAT, .defaultFloat = EC_R2_p1},
         {"ex2", "Коэффициент ex2 для EC", Param::FLOAT, .defaultFloat = EC_R2_p2},
         {"kt", "Температурный коэффициент EC", Param::FLOAT, .defaultFloat = kt},

         {"px1", "Коэффициент px1 для pH", Param::FLOAT, .defaultFloat = px1},
         {"px2", "Коэффициент px2 для pH", Param::FLOAT, .defaultFloat = px2},
         {"px3", "Коэффициент px3 для pH", Param::FLOAT, .defaultFloat = px3},
         {"py1", "Коэффициент py1 для pH", Param::FLOAT, .defaultFloat = py1},
         {"py2", "Коэффициент py2 для pH", Param::FLOAT, .defaultFloat = py2},
         {"py3", "Коэффициент py3 для pH", Param::FLOAT, .defaultFloat = py3},
         {"pH_lkorr", "Коррекция линейная pH", Param::FLOAT, .defaultFloat = pH_lkorr}

         //    {"max_l_level", "Макс. уровень жидкости", Param::FLOAT, .defaultFloat = lc.first() ? lc.first().level :
         //    0},
         //    {"max_l_raw", "Сырой макс. уровень жидкости", Param::FLOAT, .defaultFloat = lc.first() ? lc.first().raw :
         //    0},
         //    {"min_l_level", "Мин. уровень жидкости", Param::FLOAT, .defaultFloat = lc.last() ? lc.last().level : 0},
         //    {"min_l_raw", "Сырой мин. уровень жидкости", Param::FLOAT, .defaultFloat = lc.last() ? lc.last().raw :
         //    0},

         //    {"pR_DAC", "Настройка DAC для освещенности", Param::FLOAT, .defaultFloat = pR_DAC},
         //    {"pR1", "Сопротивление R1 для освещенности", Param::FLOAT, .defaultFloat = pR1},
         //    {"pR_raw_p1", "Коэффициент p1 освещенности", Param::FLOAT, .defaultFloat = pR_raw_p1},
         //    {"pR_raw_p2", "Коэффициент p2 освещенности", Param::FLOAT, .defaultFloat = pR_raw_p2},
         //    {"pR_raw_p3", "Коэффициент p3 освещенности", Param::FLOAT, .defaultFloat = pR_raw_p3},
         //    {"pR_val_p1", "Значение p1 освещенности", Param::FLOAT, .defaultFloat = pR_val_p1},
         //    {"pR_val_p2", "Значение p2 освещенности", Param::FLOAT, .defaultFloat = pR_val_p2},
         //    {"pR_val_p3", "Значение p3 освещенности", Param::FLOAT, .defaultFloat = pR_val_p3},
         //    {"pR_Rx", "Сопротивление Rx освещенности", Param::FLOAT, .defaultFloat = pR_Rx},
         //    {"pR_T", "pR_T освещенности", Param::FLOAT, .defaultFloat = pR_T},
         //    {"pR_x", "pR_xосвещенности", Param::FLOAT, .defaultFloat = pR_x},
         //    {"pR_type", "Тип сенсора освещенности", Param::STRING, .defaultString = pR_type},
     }}

};

String getGroupNameByParameter(const String &paramName)
{
    for (const auto &group : groups)
    {
        for (const auto &param : group.params)
        {
            if (String(param.name) == paramName)
            {
                return group.caption;  // Return the group name (caption)
            }
        }
    }
    return "common_group";  // If the parameter is not found
}

const int PwdChannel1 = 1;
const int PwdResolution1 = 8;
const int PwdChannel2 = 2;
const int PwdResolution2 = 8;

#include <rom/rtc.h>
#include <etc/ResetReason.h>
#include <etc/http_cert.h>

#include <dev/mcp3421/vars.h>
#include <dev/bmp280/vars.h>
#include <dev/ds18b20/main.h>
#include <dev/aht10/main.h>
#include <ADS1115_WE.h>
#include <dev/cput/main.h>
#include <dev/vcc/main.h>
#include <dev/ads1115/main.h>
#include <dev/pr/main.h>
#include <dev/us025/main.h>

#include <dev/ccs811/main.h>
#include <dev/am2320/main.h>
#include <dev/mcp23017/main.h>
#include <dev/hx710b/main.h>
#include <VL6180X.h>
#include <dev/sdc30/main.h>
#include <dev/lcd/func.h>
#include <dev/vl6180x/main.h>
#include <dev/vl53l0x_us/main.h>
#include <dev/doser/main.h>
#include <dev/ec/main.h>
#include <dev/ntc/main.h>
#include <tasks.h>
#include <web/root.h>
#include <etc/update.h>
#include <preferences_local.h>
#include <mqtt/mqtt.h>

#include <etc/wifi_ap.h>
#include <web/new_settings.h>

#include <ram_saver_api.h>
#include <setup.h>
#include <loop.h>
