#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "web/static_common.h"
#include <time.h>
extern void syslogf(const char *fmt, ...);
static uint8_t activeStaticStreams = 0;
const uint8_t MAX_STATIC_STREAMS = 3;

TimerHandle_t mqttWatchdogTimer;
extern void runDownloadTask();

void setupTime()
{
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // UTC+3, без летнего времени
    Serial.println("Ожидание синхронизации времени...");
    struct tm timeinfo;
    int i = 0;
    while (!getLocalTime(&timeinfo))
    {
        Serial.print(".");
        delay(500);
        i++;
        if (i > 3)
        {
            Serial.println("Время не синхронизировано!");
            return;
        }
    }
    Serial.println();
    Serial.println("Время синхронизировано!");
}

void calibrate_adc()
{
    // Настройка разрядности ADC
    adc1_config_width(width);

    // Инициализация калибровки
    adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, atten, width, 1100, adc_chars);

    // Конфигурация каждого канала
    for (size_t i = 0; i < channel_count; i++)
    {
        adc1_config_channel_atten(channels[i], atten);  // Здесь исправлен тип
    }
}

void setupWifi()
{
    // Start WiFi connection attempt if not already connected
    WiFi.mode(WIFI_STA);
    initWiFiEvents();
    connectToWiFi();
}
void setupTimers()
{
    BaseType_t timerCreated = pdFALSE;

    if (enable_ponics_online)
    {
        // Only try to connect to MQTT if WiFi is already connected
        if (WiFi.status() == WL_CONNECTED)
        {
            connectToMqtt();
        }
        else
        {
            syslogf("WiFi not connected, MQTT connection will be attempted after WiFi connects");
        }

        mqttReconnectTimer =
            xTimerCreate("MqttReconnectTimer", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, onMqttReconnectTimer);

        if (mqttReconnectTimer == NULL)
        {
            syslogf("CRITICAL: Failed to create MQTT reconnect timer - system may be unstable");
            // Consider rebooting if this is a critical timer
            // ESP.restart();
        }
        else
        {
            syslogf("Created MQTT reconnect timer successfully");
        }
    }

    // Create HA MQTT timer

    if (WiFi.status() == WL_CONNECTED)
    {
        connectToMqttHA();
    }
    mqttReconnectTimerHa =
        xTimerCreate("MqttReconnectTimerHa", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, onMqttReconnectTimerHa);

    if (mqttReconnectTimerHa == NULL)
    {
        syslogf("CRITICAL: Failed to create MQTT HA reconnect timer - system may be unstable");
    }
    else
    {
        syslogf("Created MQTT HA reconnect timer successfully");
    }

    // WiFi reconnection is now handled by tasks instead of timers
    syslogf("WiFi reconnection using task-based approach");
}

void setup_preferences()
{
    preferences.begin("settings", false);
    config_preferences.begin("config", false);

    for (const auto &setting : preferenceSettings)
    {
        if (preferences.getInt(setting.key, -1) == -1)
        {
            preferences.putInt(setting.key, setting.defaultValue);
        }
    }

    loadPreferences();

    appName = update_token + ":::" + HOSTNAME;
    sessionToken = preferences.getString("token", "");
}

void otaTask(void *parameter)
{
    while (true)
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            ArduinoOTA.handle();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  // 20 мс
    }
}

void setupMDNS()
{
    // First, stop any existing mDNS service
    MDNS.end();

    // Initialize mDNS with the hostname
    if (HOSTNAME == "growbox")
    {
        HOSTNAME = "growbox" + chipStr;
    }
    bool err = MDNS.begin(HOSTNAME.c_str());

    if (!err)
    {
        printf("MDNS Init failed for hostname: %s\n", HOSTNAME.c_str());
        syslogf("MDNS Init failed for hostname: %s", HOSTNAME.c_str());
        return;  // Exit if mDNS initialization fails
    }

    // Add HTTP service
    if (!MDNS.addService("http", "tcp", 80))
    {
        syslogf("Failed to add HTTP service to mDNS");
    }
    else
    {
        syslogf("Added HTTP service to mDNS");
    }

    // Add any additional services here
    // MDNS.addService("service", "tcp", port);

    // Set mDNS instance name (optional)
    MDNS.setInstanceName("GrowBox Controller");

    // Add TXT record (optional)
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");

    syslogf("mDNS responder started for %s.local", HOSTNAME.c_str());
}

void setupDisplay()
{
    Serial.println("c_LCD");
    oled.init();
    oled.clear();
    oled.home();
    oled.autoPrintln(true);
    oled.setScale(3);
    oled.println("WEGABOX");

    oled.setScale(1.3);
    oled.println();
    oled.println(Firmware);
    oled.println(WiFi.localIP().toString());
    oled.print(HOSTNAME);
    oled.println(".local");
    oled.update();
}

void setupResetReasons()
{
    Reset_reason0 = reset_reason(rtc_get_reset_reason(0));
    Reset_reason1 = reset_reason(rtc_get_reset_reason(1));

    syslogf("Reset_reason CPU0: %s", Reset_reason0.c_str());
    syslogf("Reset_reason CPU1: %s", Reset_reason1.c_str());
}

void setupI2C() { Wire.begin(I2C_SDA, I2C_SCL); }

extern IPAddress MQTT_HOST;
void setupMQTT()
{
    if (enable_ponics_online)
    {
        syslogf("mqtt MQTT: Initializing connection to %s:%d", MQTT_HOST.toString().c_str(), MQTT_PORT);

        // Generate unique client ID with random suffix
        clientId = get_client_id();
        mqttClient.setClientId(clientId.c_str());
        syslogf("mqtt MQTT: Client ID: %s", clientId.c_str());
        // Configure connection parameters
        mqttClient.setCredentials(mqtt_mqtt_user.c_str(), mqtt_mqtt_password.c_str());
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.setKeepAlive(30);        // Increased from 60 to 300 seconds (5 minutes)
        mqttClient.setCleanSession(true);   // Start with a clean session
        mqttClient.setMaxTopicLength(256);  // Ensure sufficient buffer for topics

        // Set callbacks
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onMessage(onMqttMessage);

        syslogf("MQTT: Using credentials for user: %s", mqtt_mqtt_user.c_str());
    }
}

void setupHA_MQTT()
{
    if (e_ha == 1)
    {
        // Generate a random 3-digit number (100-999)
        clientIdHa = get_client_id(true);
        mqttClientHA.setClientId(clientIdHa.c_str());
        syslogf("HA mqttClientHA Client ID: %s", clientIdHa.c_str());
        mqttClientHA.onConnect(onMqttConnectHA);
        mqttClientHA.onMessage(onMqttMessageHA);
        mqttClientHA.onDisconnect(onMqttDisconnectHA);
        mqttClientHA.setKeepAlive(30);  // Increased from 60 to 300 seconds (5 minutes)

        // Set max topic length (buffer size is not supported in this version)

        // Configure server connection
        IPAddress mqttServerHA;
        if (validateAndConvertIP(a_ha.c_str(), mqttServerHA))
        {
            // If IP is valid, use IP address
            syslogf("HA MQTT: Using IP address: %s:%d", mqttServerHA.toString().c_str(), port_ha);
            mqttClientHA.setServer(mqttServerHA, port_ha);
        }
        else
        {
            if (a_ha.length() > 0)
            {  // If not a valid IP, try with hostname
                syslogf("HA MQTT: Using hostname: %s:%d", a_ha.c_str(), port_ha);
                mqttClientHA.setServer(a_ha.c_str(), port_ha);
            }
        }

        // Set credentials if provided
        if (u_ha.length() > 0 && p_ha.length() > 0)
        {
            mqttClientHA.setCredentials(u_ha.c_str(), p_ha.c_str());
            syslogf("HA MQTT: Using credentials for user: %s", u_ha.c_str());
        }
    }
    else
    {
        syslogf("HA mqttClientHA: Not enabled");
    }
}

void setupDevices()
{
    if (disable_ntc == 0)
    {
        get_acp();
#include <dev/ntc/setup.h>
    }
#include <dev/ds18b20/setup.h>
#include <dev/ec/setup.h>
#include <dev/mcp23017/setup.h>
#include <dev/ads1115/setup.h>
#include <dev/aht10/setup.h>
#include <dev/am2320/setup.h>
#include <dev/bmp280/setup.h>
#include <dev/ccs811/setup.h>
#include <dev/cput/setup.h>
#include <dev/doser/setup.h>
#include <dev/hx710b/setup.h>
#include <dev/lcd/setup.h>
#include <dev/mcp3421/setup.h>
#include <dev/pr/setup.h>
#include <dev/sdc30/setup.h>
#include <dev/vcc/setup.h>
#include <dev/vl53l0x_us/setup.h>
#include <dev/vl6180x/setup.h>
#include <dev/us025/setup.h>
}

// Helper function to create tasks with error handling and logging
void setupTask60()
{
    syslogf("Setting up Task60");
    xTaskCreatePinnedToCore(Task60, "Task60", 8192, NULL, 1, NULL, 1);
    syslogf("Task60 setup complete");
}

void setup()
{
    // Initialize serial communication
    Serial.begin(115200);
    psramInit();  // обычно вызывается автоматически

    heap_caps_malloc_extmem_enable(32);

    for (Group &g : groups)
    {
        g.numParams = sizeof(g.params) / sizeof(g.params[0]);
    }
#if __has_include("ponics.online.h")
    syslogf("Including ponics.online.h");
    syslogf("ponics token: %s", update_token.c_str());
#include "ponics.online.h"
    syslogf("ponics.online.h included successfully");

#else
    syslogf("ponics.online.h not found, skipping");
#endif

    // Initialize file system
    if (!LittleFS.begin(true))
    {
        syslogf("Failed to mount LittleFS");
    }
    else
    {
        syslogf("LittleFS mounted successfully");
    }

    // Initialize RAM Saver
    ram_saver_init();
    initializeVariablePointers();
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    xSemaphore_C = xSemaphoreCreateMutex();
    setup_preferences();
    int rst_counter = preferences.getInt("rst_counter", 0);
    preferences.putInt("rst_counter", rst_counter + 1);
    setupWifi();
    setupTimers();

    setupLogging();

    // Check for OTA update
    if (preferences.getInt("upd", 0) == 1)
    {
        force_update = true;
        syslogf("update: starting auto update");
        make_update();
        syslogf("update: auto update completed");
    }
    else
    {
        force_update = false;
        syslogf("update: no auto update requested");
    }

    // Handle restart counter for OTA
    if (rst_counter >= 3)
    {
        rst_counter = 0;
        preferences.putInt("rst_counter", 0);

        for (int i = 0; i < 40; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            syslogf("waiting for OTA update: %d", i);
            if (OtaStart)
            {
                break;
            }
        }
    }

    if (preferences.getInt("upd_static", 0) == 1)
    {
        runDownloadTask();
        preferences.putInt("upd_static", 0);
    }
    while (OtaStart == true)
    {
        syslogf("wait ota update:  %d", millis());
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (millis() > 600000)
        {
            OtaStart = false;
        }
    }
    syslogf("Firmware: %s", Firmware.c_str());
    setupMQTT();
    setupHA_MQTT();
    calibrate_adc();

    setupDisplay();
    setupI2C();
    setupTask60();

    if (xTimerStart(mqttReconnectTimerHa, 0) != pdPASS)
    {
        syslogf("Failed to start MQTT HA reconnect timer");
    }

    if (enable_ponics_online == 1 && mqttReconnectTimer != NULL)
    {
        if (xTimerStart(mqttReconnectTimer, 0) != pdPASS)
        {
            syslogf("Failed to start MQTT reconnect timer");
        }
    }
    syslogf("setup setupDevices");
    setupDevices();
    setupTime();
    syslogf("setupTime");
    setupTaskScheduler();
    syslogf("setupTaskScheduler");
    setupBot();
    syslogf("setupBot");

    preferences.putInt("rst_counter", 0);
    syslogf("setup done");

    // testTelegram();
}
#include "loop.h"