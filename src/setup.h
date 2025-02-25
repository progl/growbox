int stack_size = 4096;

// Обработчик событий WebSocket
void onWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
        case WStype_CONNECTED:
        {
            Serial.printf("Client %u connected\n", clientNum);
            webSocket.sendTXT(clientNum, "Welcome to WebSocket!");
            break;
        }
        case WStype_DISCONNECTED:
        {
            Serial.printf("Client %u disconnected\n", clientNum);
            break;
        }
        case WStype_TEXT:
        {
            Serial.printf("Client %u sent: %s\n", clientNum, payload);
            webSocket.sendTXT(clientNum, "Message received: " + String((char *)payload));
            break;
        }
        default:
            break;
    }
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

void setupTimers()
{
#if defined(ENABLE_PONICS_ONLINE)
    mqttReconnectTimer =
        xTimerCreate("MqttReconnectTimer", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, onMqttReconnectTimer);
    if (mqttReconnectTimer == NULL)
    {
        syslog_ng("Failed to create timer for MQTT reconnect");
    }
#endif
    mqttReconnectTimerHa =
        xTimerCreate("MqttReconnectTimerHa", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, onMqttReconnectTimerHa);

    if (mqttReconnectTimerHa == NULL)
    {
        syslog_ng("Failed to create timer for MQTT reconnect");
    }
    wifiReconnectTimer = xTimerCreate("WiFiReconnectTimer",
                                      pdMS_TO_TICKS(5000),  // 10 секунд
                                      pdTRUE,               // повторяющийся
                                      (void *)0, checkWiFiConnection);

    if (wifiReconnectTimer != NULL)
    {
        xTimerStart(wifiReconnectTimer, 0);
    }
}

void setupSyslog()
{
    Serial.println("before syslog config");
    syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
    syslog.deviceHostname(HOSTNAME.c_str());
    syslog.appName(appName.c_str());
    Serial.println("syslog config");
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
}

void setupOTA()
{
    ArduinoOTA.setHostname(HOSTNAME.c_str());

    ArduinoOTA
        .onStart(
            []()
            {
                OtaStart = true;
                preferences.putInt("rst_counter", 5);
                Serial.println("before OTA");
                syslog_ng("OTA: Start");

                pinMode(EC_DigitalPort1, INPUT);
                pinMode(EC_DigitalPort2, INPUT);
                pinMode(EC_AnalogPort, INPUT);

                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                    type = "sketch";
                else
                    type = "filesystem";
                syslog_ng("OTA:Start updating " + type);
            })
        .onEnd(
            []()
            {
                preferences.putInt("rst_counter", 0);
                syslog_ng("OTA:End");
            })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError(
            [](ota_error_t error)
            {
                syslog_err("OTA:Error: " + String(error));
                preferences.putInt("rst_counter", 5);
                switch (error)
                {
                    case OTA_AUTH_ERROR:
                        syslog_err("OTA:Auth Failed");
                        break;
                    case OTA_BEGIN_ERROR:
                        syslog_err("OTA:Begin Failed");
                        break;
                    case OTA_CONNECT_ERROR:
                        syslog_err("OTA:Connect Failed");
                        break;
                    case OTA_RECEIVE_ERROR:
                        syslog_err("OTA:Receive Failed");
                        break;
                    case OTA_END_ERROR:
                        syslog_err("OTA:End Failed");
                        break;
                }
                preferences.putString(pref_reset_reason, "OTA");
                ESP.restart();
            });

    ArduinoOTA.begin();
}

void setupMDNS()
{
    bool err = MDNS.begin(HOSTNAME.c_str());

    if (!err)
    {
        printf("MDNS Init failed: %d\n", err);
        syslog_ng("MDNS Init failed: " + String(err) + " HOSTNAME: " + String(HOSTNAME));
    }
    else
    {
        syslog_ng("MDNS Init sucess : " + String(err));
        syslog_ng("MDNS Init HOSTNAME: " + String(HOSTNAME));
    }
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

    syslog_ng("Reset_reason CPU0: " + Reset_reason0);
    syslog_ng("Reset_reason CPU1: " + Reset_reason1);
}

void setupI2C() { Wire.begin(I2C_SDA, I2C_SCL); }

void handleFileRequest()
{
    String path = server.uri();  // Получаем запрошенный путь
    Serial.println("Requested file: " + path);
    sendFileLittleFS(path);
}

void handleUpdateIndex()
{
    if (downloadAndUpdateIndexFile())
    {
        server.send(200, "text/plain", "Index file updated successfully");
    }
    else
    {
        server.send(500, "text/plain", "Failed to update index file");
    }
}

void setupServer()
{
    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleApiStatuses);
    server.on("/api/groups", HTTP_GET, handleApiGroups);
    server.on("/api/save-settings", HTTP_POST, saveSettings);
    server.on("/reset", handleReset);
    server.on("/update", update);
    server.on("/update-index", HTTP_GET, handleUpdateIndex);
    server.on("/core-dump", HTTP_GET, handleCoreDump);

    server.on("/generate_204", handleRedirect);               // Android
    server.on("/library/test/success.html", handleRedirect);  // iOS
    server.on("/hotspot-detect.html", handleRedirect);        // macOS

    server.onNotFound([]() { handleFileRequest(); });
    server.begin();

    MDNS.addService("http", "tcp", 80);
}

void setupMQTT()
{
#if defined(ENABLE_PONICS_ONLINE)
    syslog_ng("mqtt MQTT_HOST: " + String(MQTT_HOST) + "mqtt MQTT_PORT: " + String(MQTT_PORT));
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);  // Set the callback for received messages
    mqttClient.setClientId(HOSTNAME.c_str());
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCredentials(mqtt_mqtt_user,
                              mqtt_mqtt_password);  // Set login and password
    syslog_ng("mqtt end setupMQTT connected: " + String(mqttClient.connected()));
#endif
}

void setupHA_MQTT()
{
    if (e_ha == 1)
    {
        String client_id = HOSTNAME + "_ha";
        static const char *a_ha_c = a_ha.c_str();
        static const char *p_ha_c = p_ha.c_str();
        static const char *u_ha_c = u_ha.c_str();
        mqttClientHA.onConnect(onMqttConnectHA);
        mqttClientHA.onMessage(onMqttMessageHA);
        mqttClientHA.onDisconnect(onMqttDisconnectHA);
        mqttClientHA.setClientId(client_id.c_str());
        IPAddress mqttServerHA;
        syslog_ng("mqttClientHA HOSTNAME: \"" + String(HOSTNAME) + "\"");
        if (validateAndConvertIP(a_ha.c_str(), mqttServerHA))
        {
            syslog_ng("mqttClientHA validateAndConvertIP 1 mqttServerHA: " + String(mqttServerHA) +
                      "port_ha: " + String(port_ha));
            mqttClientHA.setServer(mqttServerHA, uint16_t(port_ha));
        }
        else
        {
            syslog_ng("mqttClientHA set str adress : " + String(mqttServerHA) + "port_ha: " + String(port_ha));
            mqttClientHA.setServer(a_ha_c, uint16_t(port_ha));
        }
        mqttClientHA.setCredentials(u_ha_c, p_ha_c);  // Set login and password
    }
}

void setupDevices()
{
    vTaskDelay(200);
    if (disable_ntc == 0)
    {
        get_acp();
        vTaskDelay(10);
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
#include <dev/us025/setup.h>

#include <dev/vcc/setup.h>
#include <dev/vl53l0x_us/setup.h>
#include <dev/vl6180x/setup.h>
}

void setupTaskMqttForCal()
{
    syslog_ng("before setupTaskMqtt");
    syslog_ng("before TaskMqttForCal");
    xTaskCreate(TaskMqttForCal, "TaskMqttForCal", 5000, NULL, 0, NULL);
    syslog_ng("after setupTaskMqtt");
}

void setup()
{
    Serial.begin(115200);
    mcp.writeGPIOAB(0);

    if (!LittleFS.begin())
    {
        Serial.println("Failed to mount LittleFS");
    }
    else
    {
        Serial.println("LittleFS mounted successfully");
    }

    initializeVariablePointers();
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    xSemaphore_C = xSemaphoreCreateMutex();

    setup_preferences();
    setupSyslog();
    connectToWiFi();
    int rst_counter = preferences.getInt("rst_counter", 0);
    int upd = preferences.getInt("upd", 0);
    if (upd != 0)
    {
        preferences.putInt("upd", 0);
    }
    preferences.putInt("rst_counter", rst_counter + 1);

    if (upd == 1)
    {
        server_make_update = force_update = true;
        syslog_ng("update: strting auto update ");
        make_update();
        syslog_ng("update: ended auto update ");
    }
    else
    {
        server_make_update = force_update = false;
        syslog_ng("update: no auto update ");
    }

    syslog_ng("before  setupOTA ");

    setupOTA();
    setupMDNS();

    setupResetReasons();

    if (rst_counter > 3)
    {
        while (millis() < 20000)
        {
            ArduinoOTA.handle();
            server.handleClient();
            delay(1);
            if (millis() % 1000 == 0)
            {
                Serial.println("waiting ota: " + String(millis() / 1000));
            }
        }
    }

    syslog_ng("readCoreDump ");
    syslog_ng("esp_core_dump_init ");

    mqttPrefix = update_token + "/";
    syslog_ng("setupHA_MQTT ");
    setupHA_MQTT();  // добавлен вызов функции для настройки MQTT для HA
    calibrate_adc();
    setupMQTT();

    setupTimers();
    syslog_ng("setupTimers ");
    setupDisplay();
    syslog_ng("setupDisplay ");
    KalmanNTC.setParameters(ntc_mea_e, ntc_est_e, ntc_q);
    KalmanDist.setParameters(dist_mea_e, dist_est_e, dist_q);
    KalmanEC.setParameters(ec_mea_e, ec_est_e, ec_q);
    KalmanEcUsual.setParameters(ec_mea_e, ec_est_e, ec_q);
    syslog_ng("setupServer");
    setupServer();
    syslog_ng("setupI2C");
    setupI2C();
    syslog_ng("setupDevices");
    setupTaskMqttForCal();
    xTimerStart(mqttReconnectTimerHa, 0);
#if defined(ENABLE_PONICS_ONLINE)
    xTimerStart(mqttReconnectTimer, 0);
#endif
    distanceSensor.begin();  // Инициализация датчика
    setupDevices();
    distanceSensor.begin();  // Инициализация датчика
    syslog_ng("endsetup");
    syslog_ng("Firmware " + Firmware);
    syslog_ng("commit " + firmware_commit);
    preferences.putInt("rst_counter", 0);
}
