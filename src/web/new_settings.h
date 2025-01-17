void setVPDStyles(String vpdstage)
{
    if (vpdstage == "Start")
    {
        rootStyle = getStyle(rootVPD, 0.8, 0.2, 1.4);  // Параметры для стадии клонов
        airStyle = getStyle(airVPD, 0.8, 0.2, 1.4);
    }
    else if (vpdstage == "Vega")
    {
        rootStyle = getStyle(rootVPD, 1.0, 0.5, 1.6);  // Параметры для вегетативной стадии
        airStyle = getStyle(airVPD, 1.0, 0.5, 1.6);
    }
    else if (vpdstage == "Fruit")
    {
        rootStyle = getStyle(rootVPD, 1.2, 0.7, 2.8);  // Параметры для стадии цветения
        airStyle = getStyle(airVPD, 1.2, 0.7, 2.8);
    }
    else
    {
        // Значения по умолчанию, если vpdstage не соответствует ни одному из
        // известных значений
        rootStyle = getStyle(rootVPD, 1.0, 0.5, 1.6);
        airStyle = getStyle(airVPD, 1.0, 0.5, 1.6);
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

// index

void handleApiStatuses()
{
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

    syslog_ng("WEB /handleSettingsCommon");
    unsigned long t_millis = millis();

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

    // Добавление общих параметров
    doc["hostname"] = String(HOSTNAME);
    doc["hostname_text"] = "Host";

    doc["firmware"] = String(Firmware);
    doc["firmware_text"] = "Firmware";

    doc["vpdstage"] = vpdstage;
    doc["vpdstage_text"] = "VPD";

    doc["ha_connected"] = e_ha == 1 ? mqttClientHA.connected() : false;
    doc["ha_connected_text"] = "HA";

    if (percentage != 0)
    {
        doc["updating"] = String(percentage) + "%";
        doc["updating_text"] = "Update";
    }

    doc["uptime"] = float(t_millis) / 1000;
    doc["uptime_text"] = "Uptime";

    // VPD Styles
    setVPDStyles(vpdstage);

    doc["rootVPD"] = fFTS(rootVPD, 1);
    doc["rootVPD_text"] = "RootVPD";

    doc["airVPD"] = fFTS(airVPD, 1);
    doc["airVPD_text"] = "AirVPD";

    doc["airHum"] = fFTS(AirHum, 1) + "%";
    doc["airHum_text"] = "Humidity";

    doc["rootTemp"] = fFTS(RootTemp, 1);
    doc["rootTemp_text"] = "RootTemp";

    doc["AirTemp"] = fFTS(AirTemp, 1);
    doc["AirTemp_text"] = "AirTemp";

    doc["Dist"] = fFTS(Dist, 1);
    doc["Dist_text"] = "Distance";

    doc["wPR"] = fFTS(wPR, 1);
    doc["wPR_text"] = "Light";

    doc["wEC"] = fFTS(wEC, 2) + " mS/cm";
    doc["wEC_text"] = "EC";

    doc["ec_notermo"] = fFTS(ec_notermo, 2) + " mS/cm";
    doc["ec_notermo_text"] = "EC no termo";

    doc["wR2"] = fFTS(wR2, 2);
    doc["wR2_text"] = "R2";

    doc["wNTC"] = fFTS(wNTC, 1);
    doc["wNTC_text"] = "NTC";

    doc["wLevel"] = fFTS(wLevel, 1);
    doc["wLevel_text"] = "Уровень литры";

    doc["CPUTemp"] = fFTS(CPUTemp, 1);
    doc["CPUTemp_text"] = "CPU Temp";

    doc["AirPress"] = fFTS(AirPress, 1);
    doc["AirPress_text"] = "Pressure";

    doc["tVOC"] = fFTS(tVOC, 1);
    doc["tVOC_text"] = "TVOC";

    doc["CO2"] = fFTS(CO2, 1);
    doc["CO2_text"] = "CO2";

    doc["wpH"] = fFTS(wpH, 1);
    doc["wpH_text"] = "pH";

    doc["PWD1"] = String(PWD1);
    doc["PWD1_text"] = "PWD1";

    doc["PWD2"] = String(PWD2);
    doc["PWD2_text"] = "PWD2";

    // Датчики
    JsonArray sensorsArray = doc["sensors"].to<JsonArray>();
    for (int i = 0; i < sensorCount; i++)
    {
        JsonObject sensor = sensorsArray.add<JsonObject>();
        sensor["name"] = "dallas_" + String(i);
        sensor["address"] = sensorArray[i].addressToString();
        sensor["temperature"] = fFTS(sensorArray[i].temperature, 3) + "°C";
    }

    doc["reset_reason"] = preferences.getString(pref_reset_reason);
    doc["reset_reason_text"] = "Причина перезагрузки";
    doc["detected_sensors"] = detected_sensors;
    doc["detected_sensors_text"] = "Sensors";

    doc["calibration"] = String(calE);
    doc["calibration_text"] = "Calibration";

    // Управление драйверами
    doc["DRV1"] = readGPIO;
    doc["DRV1_text"] = "DRV1";

    doc["DRV2"] = readGPIO;
    doc["DRV2_text"] = "DRV2";

    doc["DRV3"] = readGPIO;
    doc["DRV3_text"] = "DRV3";

    doc["DRV4"] = readGPIO;
    doc["DRV4_text"] = "DRV4";

    doc["old_ec"] = String(old_ec);
    doc["old_ec_text"] = "OldEC";

    doc["reboot_link"] = "/reset";
    doc["reboot_link_text"] = "Reboot";

    doc["debug_link"] = "/?debug=1";
    doc["debug_link_text"] = "Debug";

    doc["An"] = fFTS(An, 3);
    doc["Ap"] = fFTS(Ap, 3);
    doc["Dist"] = fFTS(Dist, 3);
    doc["pHmV"] = fFTS(pHmV, 3);
    doc["PR"] = fFTS(PR, 3);
    doc["RAW_NTC"] = fFTS(NTC, 3);

    doc["An_text"] = "АЦП An";
    doc["Ap_text"] = "АЦП Ap";
    doc["RAW_NTC_text"] = "АЦП NTC";
    doc["Dist_text"] = "Dist";
    doc["pHmV_text"] = "pHmV";
    doc["PR_text"] = "PR";

    // Добавление отладочных данных, если включен режим отладки
    if (server.arg("debug").toInt() == 1)
    {
        JsonObject debug = doc["debug"].to<JsonObject>();

        debug["not_detected_sensors"] = not_detected_sensors;
        debug["US025_CHANGED_PORTS"] = String(US025_CHANGE_PORTS);
        debug["freeHeap"] = String(ESP.getFreeHeap());
        debug["getFreePsram"] = String(ESP.getFreePsram());
        debug["wegareply"] = String(wegareply);
        debug["json_status"] = String(err_wegaapi_json);
        debug["RootTempFound"] = String(RootTempFound);
        debug["FAILED"] = String(failedCount);

        for (uint8_t i = 0; i < server.args(); i++)
        {
            debug[server.argName(i)] = String(server.arg(i));
        }
    }

    // Сериализация и отправка JSON
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
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
                groupJson[String(param.name) + "_text"] = param.label;
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
                Serial.println("ERROR: Could not find preferences for " + String(param.name));
            }
        }
    }

    // Сериализация и отправка JSON
    String output;
    serializeJson(doc, output);

    Serial.print("Serialized JSON length: ");
    Serial.println(output.length());

    server.send(200, "application/json", output);
}

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

            if (PreferenceItem *item = findPreferenceByKey(settingName))
            {
                switch (item->type)
                {
                    case DataType::STRING:
                        *(String *)item->variable = kv.value().as<String>();
                        s = item->preferences->putString(item->key, *(String *)item->variable);
                        break;
                    case DataType::INTEGER:
                        *(int *)item->variable = kv.value().as<int>();
                        s = item->preferences->putInt(item->key, *(int *)item->variable);
                        break;
                    case DataType::FLOAT:
                        *(float *)item->variable = kv.value().as<float>();
                        s = item->preferences->putFloat(item->key, *(float *)item->variable);
                        break;
                    case DataType::BOOLEAN:
                        *(bool *)item->variable = kv.value().as<bool>();
                        s = item->preferences->putBool(item->key, *(bool *)item->variable);
                        break;
                }

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
                    setupWiFi();
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

                if (s == 0)
                {
                    syslog_err("error saving settingName " + String(settingName) + " val " + kv.value().as<String>());
                    server.send(200, "application/json", "{\"status\":\"error memory\"}");
                }
                else
                {
                    server.send(200, "application/json", "{\"status\":\"success\"}");
                }
            }
            else
            {
                syslog_err("Setting not found: " + String(settingName));
                server.send(200, "application/json", "{\"status\":\"error not found\"}");
            }
        }

        publish_setting_groups();
        publishVariablesListToMQTT();
        KalmanNTC.setParameters(ntc_mea_e, ntc_est_e, ntc_q);
        KalmanDist.setParameters(dist_mea_e, dist_est_e, dist_q);
        KalmanEC.setParameters(ec_mea_e, ec_est_e, ec_q);
        KalmanEcUsual.setParameters(ec_mea_e, ec_est_e, ec_q);
    }
    else
    {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid request\"}");
    }
}
