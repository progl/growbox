#include <web/styles.h>
String formatGPIOStatus(int driverIndex)
{
    String status = "";

    // Предполагается, что для каждого драйвера есть четыре GPIO-пина, которые
    // нужно проверить
    int baseIndex = driverIndex * 4;

    status += "A:" + String(bitRead(readGPIO, baseIndex)) + " ";
    status += "B:" + String(bitRead(readGPIO, baseIndex + 1)) + " ";
    status += "C:" + String(bitRead(readGPIO, baseIndex + 2)) + " ";
    status += "D:" + String(bitRead(readGPIO, baseIndex + 3));

    return status;
}
String calculateUptime(unsigned long t_millis)
{
    unsigned long seconds = t_millis / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    String uptime = "";

    if (days > 0)
    {
        uptime += String(days) + "d ";
    }
    if (hours % 24 > 0)
    {
        uptime += String(hours % 24) + "h ";
    }
    if (minutes % 60 > 0)
    {
        uptime += String(minutes % 60) + "m ";
    }
    if (seconds % 60 > 0 || uptime == "")
    {  // Показываем секунды только если нет других значений
        uptime += String(seconds % 60) + "s";
    }

    return uptime;
}
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
void handleRoot()
{
    syslog_ng("WEB /root");

    if (server.arg("make_update") != "" and server.arg("make_update").toInt() == 1)
    {
        syslog_ng("handleRoot - make_update");
        OtaStart = true;
        force_update = true;
        percentage = 1;
        server.sendHeader("Location", "/");
        server.send(302);
        update_f();
    }
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");

    // server.send_P(200, "text/html", settings_page);

    // Отправляем сжатый файл из PROGMEM
    server.send_P(200, "text/html", _Users_mmatveyev_PycharmProjects_web_calc_local_files_api_test_out_html_gz,
                  _Users_mmatveyev_PycharmProjects_web_calc_local_files_api_test_out_html_gz_len);
}

// Пример замены функции handleSettingsCommon

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

    JsonArray tasksArray = doc.createNestedArray("tasks_status");

    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i].taskName != nullptr)
        {
            JsonObject taskObj = tasksArray.createNestedObject();
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

    doc["name"] = wegadb;
    doc["name_text"] = "DB";

    doc["vpdstage"] = vpdstage;
    doc["vpdstage_text"] = "VPD";

    doc["ha_connected"] = e_ha == 1 ? mqttClientHA.connected() : false;
    doc["ha_connected_text"] = "HA";

    if (percentage != 0)
    {
        doc["updating"] = String(percentage) + "%";
        doc["updating_text"] = "Update";
    }

    doc["uptime"] = calculateUptime(t_millis);
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

    doc["airTemp"] = fFTS(AirTemp, 1);
    doc["airTemp_text"] = "AirTemp";

    doc["airTempRootTemp"] = fFTS(AirTemp - RootTemp, 1);
    doc["airTempRootTemp_text"] = "TempDiff";

    doc["ntcRootTemp"] = fFTS(wNTC - RootTemp, 2);
    doc["ntcRootTemp_text"] = "NTCDiff";

    doc["dist"] = fFTS(Dist, 1);
    doc["dist_text"] = "Distance";

    doc["lightSensor"] = fFTS(wPR, 1);
    doc["lightSensor_text"] = "Light";

    doc["ec"] = fFTS(wEC, 2) + " mS/cm";
    doc["ec_text"] = "EC";

    doc["ec_notermo"] = fFTS(ec_notermo, 2) + " mS/cm";
    doc["ec_notermo_text"] = "EC no termo";

    doc["r2"] = fFTS(wR2, 2);
    doc["r2_text"] = "R2";

    doc["ntc"] = fFTS(wNTC, 1);
    doc["ntc_text"] = "NTC";

    doc["cpuTemp"] = fFTS(CPUTemp, 1);
    doc["cpuTemp_text"] = "CPU";

    doc["airPress"] = fFTS(AirPress, 1);
    doc["airPress_text"] = "Pressure";

    doc["tvoc"] = fFTS(tVOC, 1);
    doc["tvoc_text"] = "TVOC";

    doc["co2"] = fFTS(CO2, 1);
    doc["co2_text"] = "CO2";

    doc["ph"] = fFTS(wpH, 1);
    doc["ph_text"] = "pH";

    doc["pwd1"] = String(PWD1);
    doc["pwd1_text"] = "PWD1";

    doc["pwd2"] = String(PWD2);
    doc["pwd2_text"] = "PWD2";

    // Датчики
    JsonArray sensorsArray = doc.createNestedArray("sensors");
    for (int i = 0; i < sensorCount; i++)
    {
        JsonObject sensor = sensorsArray.createNestedObject();
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
    doc["drv1"] = formatGPIOStatus(0);
    doc["drv1_text"] = "DRV1";

    doc["drv2"] = formatGPIOStatus(1);
    doc["drv2_text"] = "DRV2";

    doc["drv3"] = formatGPIOStatus(2);
    doc["drv3_text"] = "DRV3";

    doc["drv4"] = formatGPIOStatus(3);
    doc["drv4_text"] = "DRV4";

    doc["nextPompOff"] = fFTS((NextRootDrivePwdOff - t_millis) / 1000, 1);
    doc["nextPompOff_text"] = "PompOff";

    doc["nextPompOn"] = fFTS((NextRootDrivePwdOn - t_millis) / 1000, 1);
    doc["nextPompOn_text"] = "PompOn";

    doc["update_link"] = "/?make_update=1";
    doc["update_link_text"] = "Update";

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
        JsonObject debug = doc.createNestedObject("debug");

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
    DynamicJsonDocument doc(10000);

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
        DynamicJsonDocument doc(1024);
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
                config_preferences.begin("config", false, "config");

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
                    // Handle DRV, PWD, FREQ states, etc.
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
