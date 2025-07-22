#include <stdint.h>
#include <Arduino.h>
#include <LittleFS.h>
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
#include <vector>
#include <algorithm>
#include <DNSServer.h>  // Для работы с DNS сервером
#include <Ticker.h>
#include <set>
#include <struct.h>
#include <logger.h>
auto safeFloat = [](float v) { return (isnan(v) || isinf(v)) ? 0.0f : v; };
Ticker restartTicker;
String chipStr;
// Global HTTP client
HTTPClient http;
WiFiClientSecure wifiClient;
AsyncWebServer server(80);
AsyncEventSource events("/events");  // SSE endpoint по адресу /events

bool waitingSecondOta = false;

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
// #include <preferences_common.h>
#define I2C_SDA 21  // SDA
#define I2C_SCL 22  // SCL
bool isAPMode = false;
int stack_size = 4096;
String httpAuthUser, httpAuthPass;
uint8_t buff[128] = {0};
WiFiUDP udpClient;
Preferences preferences;
Preferences config_preferences;
#include <Syslog.h>
#include <params.h>

Syslog syslog(udpClient, SYSLOG_PROTO_IETF);
// Обработчик загрузки файла
bool shouldReboot = false;

// Add these at the top of your file
#include <queue>
#include <mutex>

// Add these global variables
struct LogMessage
{
    char message[128];  // Фиксированный буфер
    bool isError;
    unsigned long timestamp;
};

// Максимальный размер очереди логов
const size_t MAX_LOG_QUEUE_SIZE = 20;
std::deque<LogMessage> logQueue;  // Изменяем на deque для эффективного удаления с начала
std::mutex logMutex;
bool logTaskRunning = false;
TaskHandle_t logTaskHandle = NULL;

// Optimized syslog functions
void queueLogMessage(const String &message, bool isError)
{
    LogMessage logMsg;
    logMsg.isError = isError;
    logMsg.timestamp = millis();

    // Копируем строку в фиксированный буфер с проверкой длины
    size_t msgLen = message.length();
    if (msgLen >= sizeof(logMsg.message))
    {
        msgLen = sizeof(logMsg.message) - 1;
    }
    strncpy(logMsg.message, message.c_str(), msgLen);
    logMsg.message[msgLen] = '\0';  // Гарантируем завершающий ноль

    std::lock_guard<std::mutex> lock(logMutex);

    // Проверяем, не переполнена ли очередь
    if (logQueue.size() >= MAX_LOG_QUEUE_SIZE)
    {
        // Удаляем самое старое сообщение
        logQueue.pop_front();
    }

    // Добавляем новое сообщение
    logQueue.push_back(logMsg);
}
void processLogMessages(void *parameter)
{
    while (logTaskRunning)
    {
        if (!logQueue.empty())
        {
            LogMessage logMsg;
            {
                std::lock_guard<std::mutex> lock(logMutex);
                if (logQueue.empty()) continue;
                logMsg = std::move(logQueue.front());
                logQueue.pop_front();
            }

            char formattedMsg[256];  // Use fixed buffer instead of String

            snprintf(formattedMsg, sizeof(formattedMsg), "%.3fs %s", float(logMsg.timestamp) / 1000, logMsg.message);

            // Send to WebSocket

            events.send(formattedMsg, "log", logMsg.timestamp);

            // Send to syslog if connected
            if (!isAPMode && WiFi.status() == WL_CONNECTED)
            {
                syslog.log(logMsg.isError ? LOG_ERR : LOG_INFO, formattedMsg);
            }
            // Send to Serial
            Serial.println(formattedMsg);
        }
        vTaskDelay(100);  // Yield to other tasks
    }
    vTaskDelete(NULL);
}

// Replace your existing syslog_ng and syslog_err with these
void syslog_ng(String x)
{
    if (x.length() == 0)
    {
        return;
    }

    queueLogMessage(x, false);
}

void syslog_err(String x)
{
    if (x.length() == 0)
    {
        // Serial.println("Ошибка: syslog_err пустое сообщение");
        return;
    }

    queueLogMessage(x, true);
}

// Call this in your setup()
void setupLogging()
{
    Serial.println("before syslog config");
    syslog.server(SYSLOG_SERVER.c_str(), SYSLOG_PORT);
    syslog.deviceHostname(HOSTNAME.c_str());
    syslog.appName(appName.c_str());
    Serial.println("syslog config");
    logTaskRunning = true;
    xTaskCreatePinnedToCore(processLogMessages,  // Task function
                            "log_task",          // Name
                            4096,                // Stack size
                            NULL,                // Parameters
                            1,                   // Priority (1 = lower than web server)
                            &logTaskHandle,      // Task handle
                            0                    // Core (1 = same as web server)
    );
}

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
    unsigned long lastRunTime;  // Add lastRunTime field
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
#define MAX_TASKS 20
#define US_SDA 13  // SDA
#define US_SCL 14  // SCL

TaskInfo tasks[MAX_TASKS];
int taskCount = 0;

uint8_t LCDaddr = 0x3c;
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
SemaphoreHandle_t xSemaphore_C = NULL;
SemaphoreHandle_t xSemaphoreX_WIRE = NULL;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t mqttReconnectTimerHa;
TimerHandle_t wifiReconnectTimer;
TaskHandle_t xHandleUpdate = NULL;

AsyncMqttClient mqttClient;
AsyncMqttClient mqttClientHA;
bool mqttClientHAConnected = false;
bool mqttClientPonicsConnected = false;
WiFiEventId_t wifiConnectHandler;
String clientId, clientIdHa;
String sessionToken = "";
#define MAX_MQTT_QUEUE_SIZE 50

#include <driver/adc.h>

#define SCK_PIN 13
#define SDI_PIN 14
#define ADS1115_MiddleCount 10000  // 6236ms за 10 тыс усреднений
#include <debug_info_new.h>
#include <http/funcs.h>
// Структура для управления SSE клиентами

#include <dev/ec/old_adc/wega-adc.h>

// Array to store all sensor tasks
TaskParams *sensorTasks[MAX_TASKS];

// Task handles for our two worker tasks
TaskHandle_t taskHandle1 = NULL;
TaskHandle_t taskHandle2 = NULL;

void addTask(TaskParams *taskParams)
{
    if (taskParams && taskCount < MAX_TASKS)
    {
        // Initialize the lastRunTime to stagger task execution
        taskParams->lastRunTime = millis() + (taskCount * 1000);  // Stagger tasks by 1 second each
        taskParams->busyCounter = 0;
        sensorTasks[taskCount] = taskParams;
        taskCount++;
        syslogf("Added task: %s", taskParams->taskName);
    }
    else if (taskCount >= MAX_TASKS)
    {
        syslogf("Warning: Maximum number of tasks reached!");
    }
}

void workerTask(void *pvParameters)
{
    int taskIndex = (int)pvParameters;
    const char *workerName = (taskIndex == 0) ? "Worker1" : "Worker2";

    syslogf("Starting %s", workerName);

    for (;;)
    {
        // Process every other task starting from taskIndex
        for (int i = taskIndex; i < taskCount; i += 2)
        {
            TaskParams *task = sensorTasks[i];
            if (!task)
            {
                syslogf("%s: Invalid task at index %d", workerName, i);
                continue;
            }

            // OTA interrupt check
            if (OtaStart)
            {
                syslogf("%s: OTA update starting, deleting task", workerName);
                vTaskDelete(NULL);
            }

            unsigned long now = millis();
            unsigned long timeSinceLastRun = now - task->lastRunTime;

            // Check if it's time to run this task
            if (timeSinceLastRun >= task->repeatInterval)
            {
                // Try to take the semaphore
                if (xSemaphoreTake(task->xSemaphore, pdMS_TO_TICKS(1)) == pdTRUE)
                {
                    unsigned long startTime = millis();
                    task->busyCounter = 0;
                    task->lastRunTime = now;

                    // Execute the task function with error handling
                    if (task->taskFunction)
                    {
                        syslogf("%s: Executing %s", workerName, task->taskName);
                        task->taskFunction();
                        unsigned long execTime = millis() - startTime;

                        if (execTime > task->repeatInterval / 2)
                        {
                            syslogf("Warning: Task %s took %ums to execute (interval: %ums)", task->taskName, execTime,
                                    task->repeatInterval);
                        }
                    }
                    else
                    {
                        syslogf("Error: Invalid task function for %s", task->taskName);
                    }

                    xSemaphoreGive(task->xSemaphore);
                }
                else if (++task->busyCounter > 1500)
                {
                    syslogf("Task busy: %s (worker %s, last run: %ums ago)", task->taskName, workerName,
                            timeSinceLastRun);
                    task->busyCounter = 0;
                }
            }
        }

        // Small delay to prevent busy waiting
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void startTaskScheduler()
{
    // Start worker task 1 (handles even indices)
    xTaskCreatePinnedToCore(workerTask, "Worker1",
                            3072,       // Stack size
                            (void *)0,  // Parameter (task index)
                            1,          // Priority
                            &taskHandle1, 1);

    // Start worker task 2 (handles odd indices)
    xTaskCreatePinnedToCore(workerTask, "Worker2",
                            3072,       // Stack size
                            (void *)1,  // Parameter (task index)
                            1,          // Priority
                            &taskHandle2, 1);
}

// Debug function to print task information
void printTaskInfo()
{
    syslogf("\n=== Task Scheduler Debug Info ===");
    syslogf("Total tasks: %d", taskCount);

    for (int i = 0; i < taskCount; i++)
    {
        if (sensorTasks[i])
        {
            syslogf("Task[%d]: %s (Interval: %ums, Last Run: %ums ago)", i, sensorTasks[i]->taskName,
                    sensorTasks[i]->repeatInterval, millis() - sensorTasks[i]->lastRunTime);
        }
        else
        {
            syslogf("Task[%d]: NULL", i);
        }
    }
    syslogf("==============================\n");
}

// Call this after adding all tasks
void initTaskScheduler()
{
    if (taskCount > 0)
    {
        syslogf("Initializing task scheduler with %d tasks", taskCount);
        printTaskInfo();
        startTaskScheduler();
    }
    else
    {
        syslogf("Warning: No tasks registered for scheduler!");
    }
}

// Keep this for backward compatibility, but it won't be used directly
void TaskTemplate(void *params)
{
    // This is now handled by the worker tasks
    vTaskDelete(NULL);
}
// This function should be called after all tasks are added
void setupTaskScheduler()
{
    // Initialize the task scheduler with two worker tasks
    initTaskScheduler();
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
#define MAX_DS18B20_SENSORS 5

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

    if (enable_ponics_online)
    {
        if (mqttClient.connected() and (mqtt_type == "usual" or mqtt_type == "all") and WiFi.status() == WL_CONNECTED)
        {
            int packet_id = mqttClient.publish(topic, 0, false, payload);

            char e[256];  // подберите размер под свои максимальные topic+payload
            snprintf(e, sizeof(e), "%s:::%s", topic, payload);

            if (events.count() > 0)
            {
                events.send(e, "mqtt-ponics", millis());
            }

            if (packet_id == 0)
            {
                syslogf(" error enqueueMessage: publish packet_id %d topic %s payload %s", packet_id, topic, payload);
            }
        }
    }

    if (mqttClientHA.connected() and (mqtt_type == "ha" or mqtt_type == "all") and WiFi.status() == WL_CONNECTED)
    {
        int packet_id = mqttClientHA.publish(topic, 0, false, payload);

        String e = String(topic) + ":::" + String(payload);
        if (events.count() > 0)
        {
            events.send(e.c_str(), "mqtt-ha", millis());
        }
        if (packet_id == 0)
        {
            int payloadSize = strlen(payload);

            syslogf("Payload size: %d", payloadSize);
            syslogf(" error enqueueMessage: publish packet_id %d topic %s payload %s", packet_id, topic, payload);
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
    // Проверка на пустой ключ
    if (key.length() == 0)
    {
        syslogf("mqtt ERROR: Parameter key is empty.");
        return;  // Выходим, если ключ пустой
    }

    if (timescale == 1)
    {
        // Calculate required buffer size: update_token + '/' + timescale_prefix + key + '\0'
        const size_t topicSize = update_token.length() + 1 + timescale_prefix.length() + key.length() + 1;
        char topic[topicSize];
        snprintf(topic, topicSize, "%s/%s%s", update_token.c_str(), timescale_prefix.c_str(), key.c_str());

        char valueStr[32];
        dtostrf(value, 1, precision, valueStr);  // Преобразуем float в строку с заданной точностью

        if (log_debug)
        {
            syslogf("mqtt publish_parameter topic %s value: %s", topic, valueStr);
        }

        if (log_debug) syslogf("mqtt published topic %s value: %s", topic, valueStr);
        enqueueMessage(topic, valueStr, key);
    }
    else
    {
        // Calculate required buffer size: update_token + '/' + data_prefix + key + '\0'
        const size_t topicSize = update_token.length() + 1 + data_prefix.length() + key.length() + 1;
        char topic[topicSize];
        snprintf(topic, topicSize, "%s/%s%s", update_token.c_str(), data_prefix.c_str(), key.c_str());

        char valueStr[32];

        dtostrf(value, 1, precision, valueStr);  // Преобразуем float в строку с заданной точностью
        enqueueMessage(topic, valueStr, key);
    }
}

void publish_parameter(const String &key, const String &value, int timescale)
{
    // Проверка на пустой ключ
    if (key.length() == 0)
    {
        syslogf("ERROR: Parameter key is empty.");
        return;  // Выходим, если ключ пустой
    }

    // Проверка на пустое значение
    if (value.length() == 0)
    {
        syslogf("ERROR: Parameter value is empty for key: %s", key.c_str());
        return;  // Выходим, если значение пустое
    }

    // Calculate required buffer size for topic
    const char *prefix = (timescale == 1) ? timescale_prefix.c_str() : data_prefix.c_str();
    const size_t topicSize = update_token.length() + 1 + strlen(prefix) + key.length() + 1;
    char topic[topicSize];

    // Format the topic string
    snprintf(topic, topicSize, "%s/%s%s", update_token.c_str(), prefix, key.c_str());

    // Log if debug is enabled
    if (log_debug)
    {
        syslogf("publish_parameter topic %s value: %s", topic, value.c_str());
    }

    // Enqueue the message
    enqueueMessage(topic, value.c_str(), key);

    // Log if debug is enabled
    if (log_debug && timescale == 1)
    {
        syslogf("published topic %s value: %s", topic, value.c_str());
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

        char buffer[32];
        char an_str[32], r1_str[32], rx1_str[32], dr_str[32], ap_str[32], rx2_str[32], r2p_str[32], r2n_str[32],
            wr2_str[32];

        fFTS(An, 3, an_str, sizeof(an_str));
        fFTS(R1, 3, r1_str, sizeof(r1_str));
        fFTS(Rx1, 3, rx1_str, sizeof(rx1_str));
        fFTS(Dr, 3, dr_str, sizeof(dr_str));
        fFTS(Ap, 3, ap_str, sizeof(ap_str));
        fFTS(Rx2, 3, rx2_str, sizeof(rx2_str));
        fFTS(R2p, 3, r2p_str, sizeof(r2p_str));
        fFTS(R2n, 3, r2n_str, sizeof(r2n_str));
        fFTS(wR2, 3, wr2_str, sizeof(wr2_str));

        syslogf(
            "make_raschet before calculation: An: %s, R1: %s, Rx1: %s, Dr: %s, Ap: %s, Rx2: %s, R2p: %s, R2n: %s, wR2: "
            "%s, An: %s",
            an_str, r1_str, rx1_str, dr_str, ap_str, rx2_str, r2p_str, r2n_str, wr2_str, an_str);

        if (wR2 > 0)
        {
            float eb = (-log10(ec1 / ec2)) / (log10(ex2 / ex1));
            float ea = pow(ex1, -eb) * ec1;
            ec_notermo = ea * pow(wR2, eb);
            char eb_str[32], ec_str[32], ea_str[32], wntc_str[32];
            fFTS(eb, 3, eb_str, sizeof(eb_str));
            fFTS(ec_notermo, 3, ec_str, sizeof(ec_str));
            fFTS(ea, 3, ea_str, sizeof(ea_str));
            fFTS(wNTC, 3, wntc_str, sizeof(wntc_str));
            syslogf("make_raschet eb: %s ec: %s ea: %s wNTC: %s", eb_str, ec_str, ea_str, wntc_str);
            wEC_ususal = ec_notermo / (1 + kt * (wNTC - 25)) + eckorr;
            char eckal_str[32];
            fFTS(EC_KAL_E, 3, eckal_str, sizeof(eckal_str));
            syslogf("EC_KAL_E: %s", eckal_str);

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

    {"TG",
     20,
     {

         {"tbt", "Бот токен", Param::STRING, .defaultString = ""},
         {"tcid", "ID чата", Param::STRING, .defaultString = ""},

         {"ph_e", "Контроль pH (0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"ph_min", "Минимальный pH", Param::FLOAT, .defaultFloat = 5.5},
         {"ph_max", "Максимальный pH", Param::FLOAT, .defaultFloat = 6.5},

         {"ec_e", "Контроль EC (0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"ec_min", "Минимальный EC", Param::FLOAT, .defaultFloat = 1.0},
         {"ec_max", "Максимальный EC", Param::FLOAT, .defaultFloat = 2.0},

         {"ntc_e", "Контроль температуры воды (0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"ntc_min", "Мин. температура воды", Param::FLOAT, .defaultFloat = 15.0},
         {"ntc_max", "Макс. температура воды", Param::FLOAT, .defaultFloat = 40.0},

         {"rt_e", "Контроль температуры корней (0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"rt_min", "Мин. температура корней", Param::FLOAT, .defaultFloat = 18.0},
         {"rt_max", "Макс. температура корней", Param::FLOAT, .defaultFloat = 24.0},

         {"at_e", "Контроль температуры воздуха (0-off, 1-on)", Param::INT, .defaultInt = 1},
         {"at_min", "Мин. температура воздуха", Param::FLOAT, .defaultFloat = 18.0},
         {"at_max", "Макс. температура воздуха", Param::FLOAT, .defaultFloat = 30.0},
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

         {"httpAU", "Логин интерфейса", Param::STRING, .defaultString = httpAuthUser.c_str()},
         {"httpAP", "Пароль интерфейса", Param::STRING, .defaultString = httpAuthPass.c_str()},
         {"epo", "Вкл мктт поникс(0-off, 1-on)", Param::INT, .defaultInt = enable_ponics_online},
         {"epo_l", "Вкл лог поникс(0-off, 1-on)", Param::INT, .defaultInt = enable_ponics_online_logs},
         {"update_token", "Ключ обновления", Param::STRING, .defaultString = update_token.c_str()},
         {"HOSTNAME", "Имя хоста", Param::STRING, .defaultString = HOSTNAME.c_str()},
         {"vpdstage", "VPD стадия", Param::STRING, .defaultString = vpdstage.c_str()},
         {"disable_ntc", "Отключить NTC(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"calE", "Включить калибровку(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"change_pins", "поменять пины US025(HCR04)(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"clear_pref", "Сброс ВСЕХ настроек кроме wifi(0-off, 1-on)", Param::INT, .defaultInt = 0},
         {"vZapas", "Запас в трубе раствора", Param::FLOAT, .defaultFloat = 9.5},
         {"log_debug", "Включить логирование(0-off, 1-on)", Param::INT, .defaultInt = 0},

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
     35,
     {

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
         {"pH_lkorr", "Коррекция линейная pH", Param::FLOAT, .defaultFloat = pH_lkorr},

         {"max_l_level", "Макс. уровень жидкости", Param::FLOAT, .defaultFloat = max_l_level},
         {"max_l_raw", "Сырой макс. уровень жидкости", Param::FLOAT, .defaultFloat = max_l_raw},
         {"min_l_level", "Мин. уровень жидкости", Param::FLOAT, .defaultFloat = min_l_level},
         {"min_l_raw", "Сырой мин. уровень жидкости", Param::FLOAT, .defaultFloat = min_l_raw},

         {"pR_DAC", "Настройка DAC для освещенности", Param::FLOAT, .defaultFloat = pR_DAC},
         {"pR1", "Сопротивление R1 для освещенности", Param::FLOAT, .defaultFloat = pR1},
         {"pR_raw_p1", "Коэффициент p1 освещенности", Param::FLOAT, .defaultFloat = pR_raw_p1},
         {"pR_raw_p2", "Коэффициент p2 освещенности", Param::FLOAT, .defaultFloat = pR_raw_p2},
         {"pR_raw_p3", "Коэффициент p3 освещенности", Param::FLOAT, .defaultFloat = pR_raw_p3},
         {"pR_val_p1", "Значение p1 освещенности", Param::FLOAT, .defaultFloat = pR_val_p1},
         {"pR_val_p2", "Значение p2 освещенности", Param::FLOAT, .defaultFloat = pR_val_p2},
         {"pR_val_p3", "Значение p3 освещенности", Param::FLOAT, .defaultFloat = pR_val_p3},
         {"pR_Rx", "Сопротивление Rx освещенности", Param::FLOAT, .defaultFloat = pR_Rx},
         {"pR_T", "pR_T освещенности", Param::FLOAT, .defaultFloat = pR_T},
         {"pR_x", "pR_xосвещенности", Param::FLOAT, .defaultFloat = pR_x},
         {"pR_type", "Тип сенсора освещенности", Param::STRING, .defaultString = pR_type.c_str()},
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

#include <etc/update.h>
#include <preferences_local.h>
#include <mqtt/mqtt.h>

#include <etc/wifi_ap.h>
#include <web/new_settings.h>
#include <tgbot.h>
#include <setup.h>
