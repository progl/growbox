void setVPDStyles(String vpdstage)
{
    if (vpdstage == "Start")
    {
        airStyle = getStyle(AirVPD, 0.8, 0.2, 1.4);
    }
    else if (vpdstage == "Vega")
    {
        airStyle = getStyle(AirVPD, 1.0, 0.5, 1.6);
    }
    else if (vpdstage == "Fruit")
    {
        airStyle = getStyle(AirVPD, 1.2, 0.7, 2.8);
    }
    else
    {
        // Значения по умолчанию, если vpdstage не соответствует ни одному из
        // известных значений

        airStyle = getStyle(AirVPD, 1.0, 0.5, 1.6);
    }
}

void handleReset()
{
    syslog_ng("WEB /reset");
    server.send(200, "text/plain", "restart");
    delay(100);
    preferences.putString(pref_reset_reason, "url");
    ESP.restart();
}

// Функция очистки строки
String sanitizeString(const String &input)
{
    String sanitized = input;
    for (size_t i = 0; i < sanitized.length(); i++)
    {
        char c = sanitized[i];
        if (c < 32 || c == 127)
        {  // Удаляем символы управления
            sanitized.remove(i, 1);
            i--;
        }
    }
    return sanitized;
}

void handleApiTasks()
{
    // Создание JSON-объекта
    JsonDocument doc;

    JsonArray tasksArray = doc["tasks_status"].to<JsonArray>();

    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].taskName != nullptr)
        {
            JsonObject taskObj = tasksArray.add<JsonObject>();
            taskObj["taskName"] = tasks[i].taskName;
            taskObj["lastExecutionTime"] = tasks[i].lastExecutionTime;
            taskObj["totalExecutionTime"] = tasks[i].totalExecutionTime;
            taskObj["executionCount"] = tasks[i].executionCount;
            taskObj["minFreeStackSize"] = tasks[i].minFreeStackSize;
            // xSemaphore не сериализуется, так как он является указателем на объект.
        }
    }
    // Сериализация и отправка JSON
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", sanitizeString(output));
}
void handle_calibrate()
{
    syslog_ng("WEB /handle_calibrate");
    unsigned long t_millis = millis();

    // Создание JSON-объекта
    JsonDocument doc;
    doc["wR2"] = fFTS(wR2, 2);
    doc["An"] = fFTS(An, 3);
    doc["Ap"] = fFTS(Ap, 3);
    doc["Dist"] = fFTS(Dist, 3);
    doc["pHmV"] = fFTS(pHmV, 3);
    doc["PR"] = fFTS(PR, 3);
    doc["RAW_NTC"] = fFTS(NTC_RAW, 3);
    doc["Dist"] = fFTS(Dist, 1);
    doc["ec_notermo"] = fFTS(ec_notermo, 2) + " mS/cm";
    doc["not_detected_sensors"] = not_detected_sensors;

    // Сериализация и отправка JSON
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", sanitizeString(output));
}

void handleApiStatuses()
{
    syslog_ng("WEB /handleApiStatuses");
    unsigned long t_millis = millis();

    // Создание JSON-объекта
    JsonDocument doc;

    // Проверка и инициализация списков не обнаруженных и обнаруженных датчиков
    if (not_detected_sensors == "")
    {
        for (int i = 0; i < sensorsCount; ++i)
        {
            if (sensors[i].detected == 0)
            {
                not_detected_sensors += "\n" + String(sensors[i].name);
            }
        }
    }

    if (detected_sensors == "")
    {
        for (int i = 0; i < sensorsCount; ++i)
        {
            if (sensors[i].detected == 1)
            {
                detected_sensors += "\n" + String(sensors[i].name);
            }
        }
    }

    // Добавление общих параметров
    doc["hostname"] = String(HOSTNAME);

    doc["firmware"] = String(Firmware);

    doc["vpdstage"] = vpdstage;

    doc["ha"] = e_ha == 1 ? mqttClientHA.connected() : false;

    doc["uptime"] = float(t_millis) / 1000;

    // VPD Styles
    setVPDStyles(vpdstage);

    doc["AirVPD"] = fFTS(AirVPD, 1);
    doc["AirHum"] = fFTS(AirHum, 1) + "%";
    doc["RootTemp"] = fFTS(RootTemp, 1);
    doc["AirTemp"] = fFTS(AirTemp, 1);
    doc["wPR"] = fFTS(wPR, 0);
    doc["wEC"] = fFTS(wEC, 2) + " mS/cm";
    doc["wNTC"] = fFTS(wNTC, 1);
    doc["wLevel"] = fFTS(wLevel, 1);
    doc["CPUTemp"] = fFTS(CPUTemp, 1);
    doc["AirPress"] = fFTS(AirPress, 1);
    doc["tVOC"] = fFTS(tVOC, 1);
    doc["CO2"] = fFTS(CO2, 1);
    doc["wpH"] = fFTS(wpH, 1);
    doc["PWD"] = String(PWD1) + " : " + String(PWD2);
    // Датчики
    JsonArray sensorsArray = doc["sensors"].to<JsonArray>();
    for (int i = 0; i < sensorCount; i++)
    {
        doc["name"] = "dallas_" + String(i);
        doc["address"] = sensorArray[i].addressToString();
        doc["temperature"] = fFTS(sensorArray[i].temperature, 3) + "°C";
    }

    doc["detected_sensors"] = detected_sensors;

    // Управление драйверами
    doc["DRV1"] = readGPIO;

    doc["DRV2"] = readGPIO;

    doc["DRV3"] = readGPIO;

    doc["DRV4"] = readGPIO;

    // Сериализация и отправка JSON
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", sanitizeString(output));
}

void handleApiGroups()
{
    // Проверка и инициализация списков не обнаруженных и обнаруженных датчиков

    syslog_ng("WEB /groups");
    unsigned long t_millis = millis();

    // Создание JSON-объекта
    JsonDocument doc;

    // Добавление групп настроек
    JsonObject groupsJson = doc.createNestedObject("groups");

    for (Group &group : groups)
    {
        JsonObject groupJson = groupsJson.createNestedObject(group.caption);

        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];
            PreferenceItem *item = findPreferenceByKey(param.name);

            if (item != nullptr && item->preferences != nullptr)
            {
                switch (param.type)
                {
                    case Param::INT:
                        groupJson[param.name] = item->preferences->getInt(param.name, *(int *)item->variable);
                        break;
                    case Param::FLOAT:
                        groupJson[param.name] = item->preferences->getFloat(param.name, *(float *)item->variable);
                        break;
                    case Param::STRING:
                        groupJson[param.name] = item->preferences->getString(param.name, *(String *)item->variable);
                        break;
                }
            }
            else
            {
                syslog_ng("ERROR: Could not find preferences for " + String(param.name));
            }
        }
    }

    // Сериализация и отправка JSON
    String output;
    serializeJson(doc, output);

    Serial.print("Serialized JSON length: ");
    Serial.println(output.length());

    server.send(200, "application/json", sanitizeString(output));
}

// Функция для работы с JsonVariant

void saveSettings()
{
    syslog_ng("saveSettings");

    if (server.hasArg("plain"))
    {
        String body = server.arg("plain");
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, body);

        if (error)
        {
            Serial.println("Failed to parse JSON");
            server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Failed to parse JSON\"}");
            return;
        }

        for (JsonPair kv : doc.as<JsonObject>())
        {
            const char *settingName = kv.key().c_str();
            int s = 0;

            if (strcmp(settingName, "clear_pref") == 0 && kv.value().as<int>() == 1)
            {
                // Handle the clear_pref logic
                preferences.clear();
                config_preferences.clear();
                config_preferences.end();
                preferences.end();
                nvs_flash_erase_partition("nvs");  // Reformats
                nvs_flash_init_partition("nvs");   // Initializes
                syslog_ng("clear_pref");
                preferences.begin("settings", false);
                config_preferences.begin("config", false);

                preferences.putString("ssid", ssid);
                preferences.putString("password", password);

                server.send(200, "application/json", "{\"status\":\"cleared\"}");
                return;
            }

            String groupName = getGroupNameByParameter(settingName);
            syslog_ng("Group name for " + String(settingName) + ": " + groupName);

            if (updatePreference(settingName, kv.value()))
            {
                // Handle specific settings that require additional logic
                if (strcmp(settingName, "RDDelayOn") == 0)
                {
                    NextRootDrivePwdOff = millis() + (RDDelayOn * 1000);             // Set timer for turning off
                    NextRootDrivePwdOn = NextRootDrivePwdOff + (RDDelayOff * 1000);  // Set timer for turning on
                }
                else if (strcmp(settingName, "RDDelayOff") == 0)
                {
                    NextRootDrivePwdOn = millis() + (RDDelayOff * 1000);            // Set timer for turning on
                    NextRootDrivePwdOff = NextRootDrivePwdOn + (RDDelayOn * 1000);  // Set timer for turning off
                }
                else if (strcmp(settingName, "password") == 0)
                {
                    connectToWiFi();
                }

                else if (strcmp(settingName, "SetPumpA_Ml") == 0 || strcmp(settingName, "SetPumpB_Ml") == 0)
                {
                    if (kv.value().as<int>() > 0)
                    {
                        run_doser_now();  // Trigger dosing pump
                    }
                }
                else if (strncmp(settingName, "DRV", 3) == 0 || strncmp(settingName, "PWD", 3) == 0 ||
                         strncmp(settingName, "FREQ", 4) == 0)
                {
                    MCP23017();
                }

                server.send(200, "application/json", "{\"status\":\"success\"}");
            }
            else
            {
                syslog_err("Setting not found: " + String(settingName));
                server.send(200, "application/json", "{\"status\":\"error not found or error memory \"}");
            }
        }

        publish_setting_groups();

        publishVariablesListToMQTT();

        KalmanNTC.setParameters(ntc_mea_e, ntc_est_e, ntc_q);
        KalmanDist.setParameters(dist_mea_e, dist_est_e, dist_q);
        KalmanEC.setParameters(ec_mea_e, ec_est_e, ec_q);
        KalmanEcUsual.setParameters(ec_mea_e, ec_est_e, ec_q);

        for (uint8_t i = 0; i < sensorCount && i < MAX_DS18B20_SENSORS; i++)
        {
            sensorArray[i].KalmanDallasTmp.setParameters(dallas_mea_e, dallas_est_e, dallas_q);
        }
    }
}