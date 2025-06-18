bool serverStarted = false;
TimerHandle_t mqttWatchdogTimer;

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
    if (enable_ponics_online)
    {
        connectToMqtt();
        mqttReconnectTimer =
            xTimerCreate("MqttReconnectTimer", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, onMqttReconnectTimer);
        if (mqttReconnectTimer == NULL)
        {
            syslog_ng("Failed to create timer for MQTT reconnect");
        }
    }
#endif
    connectToMqttHA();
    mqttReconnectTimerHa =
        xTimerCreate("MqttReconnectTimerHa", pdMS_TO_TICKS(10000), pdTRUE, (void *)0, onMqttReconnectTimerHa);

    if (mqttReconnectTimerHa == NULL)
    {
        syslog_ng("Failed to create timer for MQTT reconnect");
    }
    wifiReconnectTimer =
        xTimerCreate("WiFiReconnectTimer", pdMS_TO_TICKS(5000), pdTRUE, (void *)0, checkWiFiConnection);

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
        vTaskDelay(20 / portTICK_PERIOD_MS);  // 20 мс
    }
}

void setupOTA()
{
    ArduinoOTA.setHostname(HOSTNAME.c_str());

    // ArduinoOTA.setPassword("your_password"); // Если нужно

    ArduinoOTA
        .onStart(
            []()
            {
                OtaStart = true;
                preferences.putInt("rst_counter", 5);
                Serial.println("before OTA");
                syslog_ng("OTA: Start");

                // Перевод портов в безопасное состояние
                pinMode(EC_DigitalPort1, INPUT);
                pinMode(EC_DigitalPort2, INPUT);
                pinMode(EC_AnalogPort, INPUT);

                // Определяем тип обновления
                String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
                syslog_ng("OTA:Start updating " + type);
            })
        .onEnd(
            []()
            {
                preferences.putInt("rst_counter", 0);
                syslog_ng("OTA:End");
            })
        .onProgress(
            [](unsigned int progress, unsigned int total)
            {
                if (total > 0)
                {
                    Serial.printf("Progress: %u%%\r", (progress * 100) / total);
                }
            })
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
                shouldReboot = true;
            });

    ArduinoOTA.begin();
    xTaskCreatePinnedToCore(otaTask, "OTA Task", 8192, NULL, 1, NULL, 0);  // Core 0
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
        syslog_ng("MDNS Init failed for hostname: " + String(HOSTNAME));
        return;  // Exit if mDNS initialization fails
    }

    // Add HTTP service
    if (!MDNS.addService("http", "tcp", 80))
    {
        syslog_ng("Failed to add HTTP service to mDNS");
    }
    else
    {
        syslog_ng("Added HTTP service to mDNS");
    }

    // Add any additional services here
    // MDNS.addService("service", "tcp", port);

    // Set mDNS instance name (optional)
    MDNS.setInstanceName("GrowBox Controller");

    // Add TXT record (optional)
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");

    syslog_ng("mDNS responder started for " + String(HOSTNAME) + ".local");
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
bool isAuthorized(AsyncWebServerRequest *request)
{
    if (!request->hasHeader("Authorization")) return false;
    String authHeader = request->getHeader("Authorization")->value();
    return authHeader == "Bearer " + sessionToken;
}

void handleBoardInfo(AsyncWebServerRequest *request)
{
    DynamicJsonDocument doc(2048);

    // Heap info
    doc["heap"]["free_bytes"] = ESP.getFreeHeap();
    doc["heap"]["max_alloc_bytes"] = ESP.getMaxAllocHeap();

    // FS (LittleFS) info
    size_t fsTotal = LittleFS.totalBytes();
    size_t fsUsed = LittleFS.usedBytes();
    doc["fs"]["total_kb"] = fsTotal / 1024;
    doc["fs"]["used_kb"] = fsUsed / 1024;
    doc["fs"]["free_kb"] = (fsTotal - fsUsed) / 1024;

    unsigned long uptime = millis();
    // Uptime
    doc["uptime_ms"] = uptime;

    // Uptime in human-readable format

    unsigned long days = uptime / 86400000;               // 1000 * 60 * 60 * 24
    unsigned long hours = (uptime % 86400000) / 3600000;  // 1000 * 60 * 60
    unsigned long minutes = (uptime % 3600000) / 60000;   // 1000 * 60
    unsigned long seconds = (uptime % 60000) / 1000;
    unsigned long milliseconds = uptime % 1000;

    char uptimeStr[64];
    snprintf(uptimeStr, sizeof(uptimeStr), "%lud %02lu:%02lu:%02lu.%03lu", days, hours, minutes, seconds, milliseconds);
    doc["uptime"]["formatted"] = uptimeStr;

    // Core temperature
    doc["core_temp_c"] = temperatureRead();  // может вернуть NAN на некоторых модулях

    // Sketch info
    doc["sketch"]["size_bytes"] = ESP.getSketchSize();
    doc["sketch"]["free_space_bytes"] = ESP.getFreeSketchSpace();

    // Reset reason
    doc["reset_reason"] = (int)esp_reset_reason();  // можно декодировать в текст позже

    // SDK, chip info
    doc["sdk_version"] = ESP.getSdkVersion();
    doc["chip_model"] = ESP.getChipModel();
    doc["chip_revision"] = ESP.getChipRevision();
    doc["cpu_freq_mhz"] = ESP.getCpuFreqMHz();

    // Wi-Fi info (если подключено)
    if (WiFi.isConnected())
    {
        doc["wifi"]["ssid"] = WiFi.SSID();
        doc["wifi"]["rssi"] = WiFi.RSSI();
        doc["wifi"]["ip"] = WiFi.localIP().toString();
    }
    else
    {
        doc["wifi"]["connected"] = false;
    }

    // Ответ
    String output;
    serializeJsonPretty(doc, output);
    request->send(200, "application/json", output);
}

#define OTA_TIMEOUT_MS 15000  // например 15 секунд
static const char *otaIndex = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <head><title>OTA Update http</title></head>
      <body>
        <h1>OTA Update</h1>
        <form method="POST" action="/ota" enctype="multipart/form-data">
          <label>File: <input type="file" name="update"></label><br/><br/>
          <label>Destination:<br/>
            <select name="fs">
              <option value="flash">Firmware (flash)</option>
              <option value="spiffs">SPIFFS</option>
              <option value="littlefs">LittleFS</option>
            </select>
          </label><br/><br/>
          <input type="submit" value="Upload">
        </form>
      </body>
    </html>
    )rawliteral";

void setupServer()
{
    // Страница OTA загрузки (firmware или файловой системы)

    // Обработчик страницы загрузки
    server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200, "text/html", otaIndex); });

    // Then modify the OTA handler

    server.on(
        "/ota", HTTP_POST,
        [&](AsyncWebServerRequest *request)
        {
            // Защита от сбоя в upload (вдруг таймаут или обрыв)
            if (otaTimedOut)
            {
                Serial.println("[OTA] Timeout occurred before POST finalized");
                AsyncWebServerResponse *response = request->beginResponse(408, "text/plain", "OTA timeout");
                response->addHeader("Connection", "close");
                request->send(response);
                otaTimedOut = false;  // сбрасываем, но НЕ ребутим!
                return;
            }

            // multi=1 → ждем второй файл
            if (request->hasParam("multi", true) && request->getParam("multi", true)->value() == "1")
            {
                waitingSecondOta = true;
            }
            else
            {
                waitingSecondOta = false;
            }

            AsyncWebServerResponse *response =
                request->beginResponse(200, "text/html", "<html><body><h2>Update completed</h2></body></html>");
            response->addHeader("Connection", "close");
            response->addHeader("X-OTA-Success", "1");
            request->send(response);

            // Перезагружаемся только после второй части
            if (!waitingSecondOta)
            {
                Serial.println("[OTA] All parts uploaded, reboot requested");
                shouldReboot = true;
            }
            else
            {
                Serial.println("[OTA] First part uploaded, waiting for second...");
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            static size_t totalWritten = 0;
            static size_t totalExpected = 0;

            if (index == 0)
            {
                lastDataTime = millis();
                totalWritten = 0;
                totalExpected = request->contentLength();  // <- получаем ожидаемый полный размер
                syslog_ng("[OTA] Begin upload, expected size: " + String(totalExpected));
            }

            // Обновляем время каждый раз при приёме
            lastDataTime = millis();

            // Проверка таймаута
            if (millis() - lastDataTime > OTA_TIMEOUT_MS)
            {
                Serial.println("[OTA] Upload timed out");
                if (updateBegun)
                {
                    Update.abort();
                    updateBegun = false;
                }
                otaTimedOut = true;
                return;  // НЕ вызываем request->send здесь!
            }

            OtaStart = true;

            if (!updateBegun)
            {
                updateBegun = true;

                String dest = "firmware";
                if (request->hasParam("fs", true))
                {
                    dest = request->getParam("fs", true)->value();
                }

                if (dest == "spiffs" || dest == "littlefs")
                {
                    Serial.println("[OTA] Starting FS OTA update...");
                    Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
                }
                else
                {
                    Serial.println("[OTA] Starting firmware OTA update...");
                    Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
                }
            }

            if (len)
            {
                size_t written = Update.write(data, len);
                totalWritten += written;

                float pct = 100.0f * totalWritten / (float)totalExpected;
                char buf[64];
                snprintf(buf, sizeof(buf), "[OTA] Progress: %.1f%% (%u/%u)", pct, totalWritten, totalExpected);
                syslog_ng(buf);

                Serial.printf("[OTA] Written %u/%u bytes\n", written, len);
            }

            if (final)
            {
                bool success = Update.end(true);
                if (success && !Update.hasError())
                {
                    Serial.println("[OTA] OTA Update Success");
                    syslog_ng("[OTA] OTA Update Success");
                }
                else
                {
                    String err = "[OTA] OTA Failed: " + String(Update.errorString());
                    Serial.println(err);
                    syslog_ng(err);
                }

                updateBegun = false;
                OtaStart = false;
            }
        });

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
                DynamicJsonDocument doc(256);
                DeserializationError error = deserializeJson(doc, body);
                if (error)
                {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }

                const char *username = doc["username"];
                const char *password = doc["password"];

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

    server.addHandler(new AsyncCallbackJsonWebHandler(
        "/api/save-settings",
        [](AsyncWebServerRequest *request, JsonVariant &json)
        {
            if (!isAuthorized(request))
            {
                request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                return;
            }
            syslog_ng("saveSettings (via AsyncCallbackJsonWebHandler)");

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

                    syslog_ng("clear_pref");

                    preferences.begin("settings", false);
                    config_preferences.begin("config", false);

                    preferences.putString("ssid", ssid);
                    preferences.putString("password", password);

                    request->send(200, "application/json", "{\"status\":\"cleared\"}");
                    return;
                }

                String groupName = getGroupNameByParameter(settingName);
                syslog_ng("Group name for " + String(settingName) + ": " + groupName);

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

                    // Успешное сохранение
                    request->send(200, "application/json", "{\"status\":\"success\"}");
                }
                else
                {
                    syslog_err("Setting not found or error: " + String(settingName));
                    request->send(400, "application/json",
                                  "{\"status\":\"error\",\"message\":\"Setting not found or memory error\"}");
                }
            }

            // Apply updated Kalman filter parameters
            KalmanNTC.setParameters(ntc_mea_e, ntc_est_e, ntc_q);
            KalmanDist.setParameters(dist_mea_e, dist_est_e, dist_q);
            KalmanEC.setParameters(ec_mea_e, ec_est_e, ec_q);
            KalmanEcUsual.setParameters(ec_mea_e, ec_est_e, ec_q);

            for (uint8_t i = 0; i < sensorCount && i < MAX_DS18B20_SENSORS; i++)
            {
                sensorArray[i].KalmanDallasTmp.setParameters(dallas_mea_e, dallas_est_e, dallas_q);
            }
        }));

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

    server.on("/api/download-assets", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  if (!isAuthorized(request))
                  {
                      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
                      return;
                  }

                  if (downloadAndUpdateIndexFile())
                  {
                      syslog_ng("update: Successfully updated index.html.gz");
                      request->send(200, "application/json",
                                    "{\"status\":\"success\", \"message\":\"Assets updated successfully\"}");
                  }
                  else
                  {
                      syslog_ng("update: Failed to update index.html.gz");
                      request->send(500, "application/json",
                                    "{\"status\":\"error\", \"message\":\"Failed to update assets\"}");
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

    setupRamSaverAPI();

    server.on("/", HTTP_GET,
              [](AsyncWebServerRequest *request)
              {
                  // Debug output
                  Serial.println("HTTP GET /");

                  if (request->hasHeader("Accept-Encoding") &&
                      request->getHeader("Accept-Encoding")->value().indexOf("gzip") != -1)
                  {
                      // Create response with binary data
                      AsyncWebServerResponse *response = request->beginResponse_P(
                          200, "text/html", (const uint8_t *)indexHtml.c_str(), indexHtml.length());

                      if (!response)
                      {
                          request->send(500, "text/plain", "Failed to create response");
                          return;
                      }

                      response->addHeader("Content-Encoding", "gzip");
                      response->addHeader("Content-Length", String(indexHtml.length()));
                      response->addHeader("Vary", "Accept-Encoding");

                      // Debug headers
                      Serial.println("Sending gzipped response");
                      Serial.printf("Content-Length: %d\n", indexHtml.length());

                      request->send(response);
                  }
                  else
                  {
                      // Fallback for non-gzip clients
                      request->send(200, "text/plain", "Please use a browser that supports gzip compression");
                  }
              });

#include "web/static_common.h"

    server.onNotFound(
        [](AsyncWebServerRequest *request)
        {
            if (request->method() == HTTP_OPTIONS)
            {
                AsyncWebServerResponse *response = request->beginResponse(200);
                response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                request->send(response);
            }
            else
            {
                request->send(404, "text/plain", "Not found");
            }
        });

    server.addHandler(&events);
}

void setupMQTT()
{
#if defined(ENABLE_PONICS_ONLINE)
    if (enable_ponics_online)
    {
        syslog_ng("mqtt MQTT: Initializing connection to " +

                  MQTT_HOST.toString() + ":" + String(MQTT_PORT));

        // Generate unique client ID with random suffix
        clientId = get_client_id();
        mqttClient.setClientId(clientId.c_str());
        syslog_ng("mqtt MQTT: Client ID: " + clientId);
        // Configure connection parameters
        mqttClient.setCredentials(mqtt_mqtt_user, mqtt_mqtt_password);
        mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        mqttClient.setKeepAlive(60);
        mqttClient.setCleanSession(true);   // Start with a clean session
        mqttClient.setMaxTopicLength(256);  // Ensure sufficient buffer for topics

        // Set callbacks
        mqttClient.onConnect(onMqttConnect);
        mqttClient.onDisconnect(onMqttDisconnect);
        mqttClient.onMessage(onMqttMessage);

        syslog_ng("MQTT: Using credentials for user: " + String(mqtt_mqtt_user));
    }
#endif
}

void setupHA_MQTT()
{
    if (e_ha == 1)
    {
        // Generate a random 3-digit number (100-999)
        clientIdHa = get_client_id(true);
        mqttClientHA.setClientId(clientIdHa.c_str());
        syslog_ng("HA MQTT Client ID: " + clientIdHa);
        mqttClientHA.onConnect(onMqttConnectHA);
        mqttClientHA.onMessage(onMqttMessageHA);
        mqttClientHA.onDisconnect(onMqttDisconnectHA);
        mqttClientHA.setKeepAlive(30);

        // Set max topic length (buffer size is not supported in this version)

        // Configure server connection
        IPAddress mqttServerHA;
        if (validateAndConvertIP(a_ha.c_str(), mqttServerHA))
        {
            // If IP is valid, use IP address
            syslog_ng("HA MQTT: Using IP address: " + mqttServerHA.toString() + ":" + String(port_ha));
            mqttClientHA.setServer(mqttServerHA, port_ha);
        }
        else
        {
            // If not a valid IP, try with hostname
            syslog_ng("HA MQTT: Using hostname: " + String(a_ha) + ":" + String(port_ha));
            mqttClientHA.setServer(a_ha.c_str(), port_ha);
        }

        // Set credentials if provided
        if (u_ha.length() > 0 && p_ha.length() > 0)
        {
            mqttClientHA.setCredentials(u_ha.c_str(), p_ha.c_str());
            syslog_ng("HA MQTT: Using credentials for user: " + String(u_ha));
        }
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

#include <dev/vcc/setup.h>
#include <dev/vl53l0x_us/setup.h>
#include <dev/vl6180x/setup.h>
#include <dev/us025/setup.h>
}

void setupTask60()
{
    syslog_ng("before setupTask60");
    xTaskCreate(Task60, "Task60", 3000, NULL, 1, NULL);
    syslog_ng("after setupTask60");
}

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin())
    {
        Serial.println("Failed to mount LittleFS");
    }
    else
    {
        Serial.println("LittleFS mounted successfully");
    }
    // Попытка открыть /index.html
    File f = LittleFS.open("/index.html.gz", "rb");  // Note the "rb" for binary mode
    if (f)
    {
        // Clear the string first
        indexHtml = "";
        // Reserve space for the binary data
        indexHtml.reserve(f.size());
        // Read the file into the String as binary data
        while (f.available())
        {
            indexHtml += (char)f.read();
        }
        f.close();
        Serial.printf("index.html.gz loaded into RAM, size: %d bytes\n", indexHtml.length());
        // Debug: Print first few bytes to verify they look like gzip header
        Serial.printf("First bytes: %02X %02X %02X\n", (byte)indexHtml[0], (byte)indexHtml[1], (byte)indexHtml[2]);
    }
    else
    {
        Serial.println("Failed to open index.html.gz");
    }

    // Initialize RAM Saver
    ram_saver_init();

    initializeVariablePointers();
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    xSemaphore_C = xSemaphoreCreateMutex();

    setup_preferences();
    mqttPrefix = update_token + "/";
    int rst_counter = preferences.getInt("rst_counter", 0);
    preferences.putInt("rst_counter", rst_counter + 1);

    setupSyslog();
    setupServer();

    WiFi.onEvent(
        [](arduino_event_id_t event, arduino_event_info_t info)
        {
            if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP && !serverStarted)
            {
                Serial.println("WiFi up, starting server");
                server.begin();
                setupMDNS();
                serverStarted = true;
            }
        },
        ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(
        [](arduino_event_id_t event, arduino_event_info_t info)
        {
            if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP && !serverStarted)
            {
                Serial.println("WiFi up, starting server");
                server.begin();
                serverStarted = true;
            }
        },
        ARDUINO_EVENT_WIFI_AP_START);

    // Initialize WiFi client
    initWiFiClient();
    connectToWiFi();
    syslog_ng("OTA initialized successfully");
    syslog_ng("Firmware: " + String(Firmware));

    if (preferences.getInt("upd", 0) == 1)
    {
        force_update = true;
        syslog_ng("update: strting auto update ");
        make_update();
        syslog_ng("update: ended auto update ");
    }
    else
    {
        force_update = false;
        syslog_ng("update: no auto update init ");
    }

    if (rst_counter >= 3)
    {
        rst_counter = 0;
        for (int i = 0; i < 40; i++)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            syslog_ng("wait ota update:  " + String(i));
            if (OtaStart == true)
            {
                break;
            }
        }
    }

    while (OtaStart == true)
    {
        delay(100);
    }

    setupMQTT();
    setupHA_MQTT();

    calibrate_adc();
    setupTimers();
    setupDisplay();
    setupI2C();
    setupTask60();
    xTimerStart(mqttReconnectTimerHa, 0);
#if defined(ENABLE_PONICS_ONLINE)
    if (enable_ponics_online)
    {
        xTimerStart(mqttReconnectTimer, 0);
    }
#endif

    setupDevices();
    
    // Create RAM saver task with larger stack
    if (xTaskCreate(ram_saver_task, "RamSaver", 12288, NULL, 1, NULL) != pdPASS) {
        Serial.println("[ERROR] Failed to create RamSaver task");
    }

    preferences.putInt("rst_counter", 0);
    
    // Print initial memory stats
    Serial.printf("[MEM] Free heap: %d, Min free: %d\n", 
                 esp_get_free_heap_size(), 
                 esp_get_minimum_free_heap_size());
}
