void publish_discovery_payload(const char *sensor_name)
{
    JsonDocument doc;
    char buffer[512];

    doc["name"] = sensor_name;
    doc["state_topic"] = mqttPrefix + timescale_prefix + sensor_name;
    doc["state_class"] = "measurement";

    doc["unique_id"] = String(HOSTNAME) + "_" + sensor_name;
    doc["device"]["identifiers"][0] = HOSTNAME;
    doc["device"]["name"] = HOSTNAME;
    doc["device"]["model"] = "GROWBOX";
    doc["device"]["manufacturer"] = "PONICS.ONLINE";

    serializeJson(doc, buffer);
    String discovery_topic = "homeassistant/sensor/" + String(sensor_name) + "__" + String(HOSTNAME) + "/config";
    enqueueMessage(discovery_topic.c_str(), buffer, "", false);
}

void publish_switch_discovery_payload(Param param)
{
    String switch_name = String(param.name);
    syslog_ng("Toggle parameter found: " + switch_name);
    JsonDocument doc;
    char buffer[512];
    String state_topic = String(mqttPrefix) + "switch/" + switch_name + "/state";
    String command_topic = String(mqttPrefix) + "switch/" + switch_name + "/state";
    doc["name"] = switch_name;                                // Имя устройства
    doc["command_topic"] = command_topic;                     // Топик для команд
    doc["state_topic"] = state_topic;                         // Топик для состояния
    doc["unique_id"] = String(HOSTNAME) + "_" + switch_name;  // Уникальный ID устройства
    doc["payload_on"] = "1";                                  // Значение для включения
    doc["payload_off"] = "0";                                 // Значение для выключения
    doc["retain"] = true;                                     // Сообщения сохраняются

    // Информация об устройстве (для Home Assistant Device Registry)
    doc["device"]["identifiers"][0] = HOSTNAME;
    doc["device"]["name"] = HOSTNAME;
    doc["device"]["model"] = "GROWBOX";
    doc["device"]["manufacturer"] = "PONICS.ONLINE";

    // Сериализуем JSON
    serializeJson(doc, buffer);

    // Публикуем сообщение в топик Discovery
    String discovery_topic = "homeassistant/switch/" + String(switch_name) + "__" + String(HOSTNAME) + "/config";

    enqueueMessage(discovery_topic.c_str(), buffer, "", false);
    PreferenceItem *item = findPreferenceByKey(switch_name.c_str());

    if (item != nullptr && item->variable != nullptr)
    {
        mqttClientHA.subscribe(command_topic.c_str(), qos);
        String value = (item->variable != nullptr && strlen((const char *)item->variable) > 0)
                           ? String((const char *)item->variable)
                           : "0";
        syslog_ng("mqttClientHA publish_switch_discovery_payload  state " + String(switch_name) + " state_topic " +
                  String(state_topic) + " (const char *)item->variable " + value);
        enqueueMessage(state_topic.c_str(), value.c_str(), "", false);
    }
    else
    {
        syslog_ng(
            "ERROR: publish_switch_discovery_payload Could not find "
            "preferences for " +
            String(switch_name));
    }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    syslog_ng("Disconnected. Reason: " + String((int)reason));
    if (reason == AsyncMqttClientDisconnectReason::TCP_DISCONNECTED)
    {
        syslog_ng("TCP_DISCONNECTED: клиент сам разорвал соединение.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION)
    {
        syslog_ng("Ошибка: неверная версия протокола.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED)
    {
        syslog_ng("Ошибка: идентификатор клиента отклонен.");
    }
    // Добавьте дополнительные случаи для анализа

    syslog_ng("mqtt: Disconnected. Reason: " + String(static_cast<int>(reason)));
    syslog_ng("mqtt: WiFi isConnected: " + String(static_cast<int>(WiFi.isConnected())));
    if (WiFi.isConnected())
    {
        if (not mqttClient.connected())
        {
            syslog_ng("mqtt: Starting reconnect timer");
            xTimerStart(mqttReconnectTimer, 0);
        }
    }
}

void onMqttDisconnectHA(AsyncMqttClientDisconnectReason reason)
{
    syslog_ng("mqttClientHA Disconnected. Reason: " + String((int)reason));
    if (reason == AsyncMqttClientDisconnectReason::TCP_DISCONNECTED)
    {
        syslog_ng("mqttClientHA TCP_DISCONNECTED: клиент сам разорвал соединение.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION)
    {
        syslog_ng("mqttClientHA Ошибка: неверная версия протокола.");
    }
    else if (reason == AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED)
    {
        syslog_ng("mqttClientHA Ошибка: идентификатор клиента отклонен.");
    }
    // Добавьте дополнительные случаи для анализа

    syslog_ng("mqttClientHA: Disconnected. Reason: " + String(static_cast<int>(reason)));
    syslog_ng("mqttClientHA: WiFi isConnected: " + String(static_cast<int>(WiFi.isConnected())));
    if (WiFi.isConnected())
    {
        if (not mqttClientHA.connected())
        {
            syslog_ng("mqtt: Starting reconnect timer HA");
            xTimerStart(mqttReconnectTimerHa, 0);
        }
    }
}

void connectToMqtt()
{
    if (not mqttClient.connected())
    {
        syslog_ng("mqtt connectToMqtt Connecting to MQTT...");
        mqttClient.connect();
        syslog_ng("mqtt connectToMqtt Connected " + String(mqttClient.connected()));
        mqtt_not_connected_counter = mqtt_not_connected_counter + 1;
        if (mqtt_not_connected_counter > 100)
        {
            syslog_ng("mqtt connectToMqtt ESP RESTART");
            preferences.putString(pref_reset_reason, "mqtt error");
        }
    }
    else
    {
        mqtt_not_connected_counter = 0;
        syslog_ng("mqtt connectToMqtt ALREADY Connected " + String(mqttClient.connected()));
    }
}
void connectToMqttHA()
{
    if (e_ha == 1)
    {
        if (not mqttClientHA.connected())
        {
            mqttHAConnected = 0;
            syslog_ng("mqttClientHA a_ha: \"" + String(a_ha) + "\", p_ha: \"" + String(p_ha) + "\", u_ha: \"" +
                      String(u_ha) + "\"");
            syslog_ng("mqttClientHA connectToMqtt Connecting to MQTT...");
            mqttClientHA.connect();
            syslog_ng("mqttClientHA connectToMqtt Connected " + String(mqttClientHA.connected()));
            mqtt_not_connected_counter = mqtt_not_connected_counter + 1;
            if (mqtt_not_connected_counter > 100)
            {
                syslog_ng("mqttClientHA connectToMqtt ESP RESTART");
                preferences.putString(pref_reset_reason, "mqttHA error");
            }
        }
        else
        {
            mqtt_not_connected_counter = 0;
            syslog_ng("mqttClientHA connectToMqtt ALREADY Connected " + String(mqttClientHA.connected()));
        }
    }
}
void onMqttReconnectTimer(TimerHandle_t xTimer)
{
    connectToMqtt();  // Попытка повторного подключения к MQTT
}

void onMqttReconnectTimerHa(TimerHandle_t xTimer)
{
    connectToMqttHA();  // Попытка повторного подключения к MQTT
}

void publishVariablesListToMQTT()
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    int arraySize =
        sizeof(preferencesArray) / sizeof(preferencesArray[0]);  // Determine the number of elements in the array

    for (int i = 0; i < arraySize; ++i)
    {
        String key = preferencesArray[i].key;

        if (preferencesArray[i].variable != nullptr)
        {
            switch (preferencesArray[i].type)
            {
                case DataType::FLOAT:
                    root[key] = *(float *)preferencesArray[i].variable;
                    // varObject["type"] = "FLOAT";
                    break;
                case DataType::STRING:
                    root[key] = *(String *)preferencesArray[i].variable;
                    // varObject["type"] = "STRING";
                    break;
                case DataType::BOOLEAN:
                    root[key] = *(bool *)preferencesArray[i].variable;
                    // varObject["type"] = "BOOLEAN";
                    break;
                case DataType::INTEGER:
                    root[key] = *(int *)preferencesArray[i].variable;
                    // varObject["type"] = "INTEGER";
                    break;
            }
        }
        else
        {
            JsonObject varObject = root[key].to<JsonObject>();  // Use the new syntax for creating nested
                                                                // objects
            varObject["value"] = "NaN";
            // varObject["type"] = "UNKNOWN";
        }
    }

    // Serialize JSON object to string
    String output;
    serializeJson(doc, output);

    // Create the topic string
    String topic = mqttPrefix + preferences_prefix + "all_variables";

    // Send the JSON payload via MQTT
    enqueueMessage(topic.c_str(), output.c_str(), "", false);
    vTaskDelay(10);
}
void publish_setting_groups()
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    for (Group &group : groups)
    {
        JsonObject groupJson = root[group.caption].to<JsonObject>();  // Use the new syntax for creating nested objects

        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];
            PreferenceItem *item = findPreferenceByKey(param.name);

            if (item == nullptr)
            {
                syslog_ng("nullptr: " + String(param.name));
                continue;
            }

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

    // Serialize JSON object to string
    String output;
    serializeJson(doc, output);
    String topic = mqttPrefix + main_prefix + "all_groups";
    enqueueMessage(topic.c_str(), output.c_str());
    vTaskDelay(10);
}

void processToggleParameters()
{
    for (Group &group : groups)
    {
        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];

            // Проверяем, содержит ли label строку "(0-off, 1-on)"
            if (String(param.label).indexOf("(0-off, 1-on)") != -1)
            {
                // Формируем уникальное имя переключателя

                // Публикуем Discovery payload для Home Assistant
                publish_switch_discovery_payload(param);
                delay(1);

                // Логируем найденные параметры
            }
        }
    }
}

void sendToggleState(const char *groupCaption, const Param &param)
{
    // Формируем уникальное имя переключателя
    String switchName = String(groupCaption) + "_" + String(param.name);

    // Определяем текущее состояние (из defaultInt для INT)
    bool state = param.defaultInt != 0;

    // Формируем топик состояния
    String stateTopic = String(mqttPrefix) + "/switch/" + switchName + "/state";

    // Публикуем состояние

    enqueueMessage(stateTopic.c_str(), state ? "ON" : "OFF", "", false);

    // Логируем отправку
    syslog_ng("State sent: " + switchName + " = " + (state ? "ON" : "OFF"));
}

void subscribe()
{
    String mqttPrefixSet = mqttPrefix + "set/#";
    String mqttPrefixtest = mqttPrefix + "test/#";
    syslog_ng("mqtt subscribe mqttPrefixSet: " + mqttPrefixSet);
    syslog_ng("mqtt subscribe mqttPrefixSet: " + mqttPrefixtest);
    mqttClient.subscribe(mqttPrefixSet.c_str(), qos);  // Subscribing to topic with prefix
    mqttClient.subscribe(mqttPrefixtest.c_str(), qos);
    syslog_ng("mqtt end subscribe");
}
void publish_one_data(const PreferenceItem &item)
{
    String topic = mqttPrefix + preferences_prefix + item.key;  // Use appropriate topic prefix
    String valueStr;
    if (item.variable != nullptr)
    {
        syslog_ng("before publish_one_data topic: " + String(item.key));
        // Определите тип переменной и получите ее значение в зависимости от типа
        switch (item.type)
        {
            case DataType::FLOAT:
                valueStr = String(*(float *)item.variable);
                break;
            case DataType::STRING:
                valueStr = *(String *)item.variable;
                break;
            case DataType::BOOLEAN:
                valueStr = (*(bool *)item.variable) ? "true" : "false";
                break;
            case DataType::INTEGER:
                valueStr = String(*(int *)item.variable);
                break;
        }

        // Опубликуйте значение в MQTT
        syslog_ng("publish_one_data topic: " + topic + " value: " + valueStr);
        enqueueMessage(topic.c_str(), valueStr.c_str());
    }
    syslog_ng("after publish_one_data topic: " + String(item.key));
}
void publish_params_all(int all = 1)
{
    if (all == 1)
    {
        publish_parameter("rootVPD", rootVPD, 3, 1);
        publish_parameter("airVPD", airVPD, 3, 1);
        publish_parameter("uptime", millis() / 1000, 0, 1);
        publish_parameter("ip-adress", WiFi.localIP().toString(), 1);
        publish_parameter("RSSI", String(WiFi.RSSI()), 1);
        publish_parameter("Kornevoe", AirTemp - RootTemp, 3, 1);
        publish_parameter("local_ip", String(HOSTNAME) + ".local", 1);
    }
    if (calE == 1)
    {
        syslog_ng("publich calibtate params");

        publish_parameter("local_ip", String(HOSTNAME) + ".local", 0);
        publish_parameter("commit", String(Firmware), 0);
        // publish_parameter("RSSI", String(WiFi.RSSI()), 0);
        publish_parameter("RootTemp", RootTemp, 3, 0);
        publish_parameter("AirTemp", AirTemp, 3, 0);
        // publish_parameter("AirHum", AirHum, 3, 0);
        publish_parameter("pHmV", pHmV, 3, 0);
        publish_parameter("pHraw", pHraw, 3, 0);
        publish_parameter("CO2", CO2, 3, 0);
        publish_parameter("tVOC", tVOC, 3, 0);
        publish_parameter("NTC", NTC, 3, 0);
        publish_parameter("Ap", Ap, 3, 0);
        publish_parameter("An", An, 3, 0);
        publish_parameter("Dist", Dist, 3, 0);
        publish_parameter("DstRAW", DstRAW, 3, 0);
        publish_parameter("PR", PR, 3, 0);
        // publish_parameter("AirPress", AirPress, 3, 0);
        // publish_parameter("CPUTemp", CPUTemp, 3, 0);
        publish_parameter("wNTC", wNTC, 3, 0);
        publish_parameter("wR2", wR2, 3, 0);
        publish_parameter("wEC", wEC, 3, 0);
        publish_parameter("wECnt", ec_notermo, 3, 0);
        publish_parameter("wpH", wpH, 3, 0);
        publish_parameter("wLevel", wLevel, 3, 0);
        publish_parameter("wECnt", ec_notermo, 3, 0);
        publish_parameter("wPR", wPR, 3, 0);
        publish_parameter("PumpA_SUM", SetPumpA_Ml_SUM, 3, 0);
        publish_parameter("PumpB_SUM", SetPumpB_Ml_SUM, 3, 0);
        publish_parameter("StepA_SUM", PumpA_Step_SUM, 3, 0);
        publish_parameter("StepB_SUM", PumpB_Step_SUM, 3, 0);
        publish_parameter("eRAW", eRAW, 3, 0);
        // publish_parameter("readGPIO", readGPIO, 3, 0);
        // publish_parameter("PWD1", PWD1, 3, 0);
        // publish_parameter("PWD2", PWD2, 3, 0);
        // publish_parameter("ECStabOn", ECStabOn, 3, 0);
        // publish_parameter("Vcc", Vcc, 3, 0);
        // publish_parameter("restart", 0, 0, 0);
        publish_parameter("uptime", millis() / 1000, 0, 0);
    }
}
void onMqttConnect(bool sessionPresent)
{
    syslog_ng("onMqttConnect mqtt connected start subscribe");
    subscribe();
    syslog_ng("mqtt onMqttConnect subscribed");
    if (first_time)
    {
        String statusTopic = mqttPrefix + "status";
        enqueueMessage(statusTopic.c_str(), "connected");

        publish_parameter("commit", String(Firmware), 1);
        syslog_ng("Before publish_setting_groups ");
        publish_setting_groups();
        syslog_ng("Before publishVariablesListToMQTT ");
        publishVariablesListToMQTT();
        first_time = false;
        syslog_ng("After suspected line");
    }
    syslog_ng("mqtt onMqttConnect end");
}

void mqttTask(void *parameter)
{
    publish_discovery_payload("AirHum");
    publish_discovery_payload("AirPress");
    publish_discovery_payload("AirTemp");
    publish_discovery_payload("An");
    publish_discovery_payload("Ap");
    publish_discovery_payload("CPUTemp");
    publish_discovery_payload("CPUTemp1");
    publish_discovery_payload("CPUTemp2");
    publish_discovery_payload("DebugInfo");
    publish_discovery_payload("Dist");
    publish_discovery_payload("DistRaw");
    publish_discovery_payload("Dist_Kalman");
    publish_discovery_payload("DstRAW");
    publish_discovery_payload("EC_Kalman");
    publish_discovery_payload("EC_notermo_Kalman");
    publish_discovery_payload("Kornevoe");
    publish_discovery_payload("NTC");
    publish_discovery_payload("NTC_Kalman");
    publish_discovery_payload("NTC_RAW");
    publish_discovery_payload("PR");
    publish_discovery_payload("PWD1");
    publish_discovery_payload("PWD2");
    publish_discovery_payload("RSSI");
    publish_discovery_payload("RootTemp");
    publish_discovery_payload("Vcc");
    publish_discovery_payload("airVPD");
    publish_discovery_payload("commit");
    publish_discovery_payload("db");
    publish_discovery_payload("ipadress");
    publish_discovery_payload("local_ip");
    publish_discovery_payload("pHmV");
    publish_discovery_payload("pHraw");
    publish_discovery_payload("readGPIO");
    publish_discovery_payload("rootVPD");
    publish_discovery_payload("uptime");
    publish_discovery_payload("wEC");
    publish_discovery_payload("wEC_ususal");
    publish_discovery_payload("wECnt");
    publish_discovery_payload("wLevel");
    publish_discovery_payload("wNTC");
    publish_discovery_payload("wPR");
    publish_discovery_payload("wR2");

    publish_discovery_payload("wpH");

    processToggleParameters();

    vTaskDelete(NULL);  // Удаляем задачу после выполнения
}

void onMqttConnectHA(bool sessionPresent)
{
    syslog_ng("onMqttConnectHA mqttHA connected");
    mqttClientHA.subscribe("homeassistant/status", 1);
    mqttClientHA.subscribe("homeassistant/growbox/status", 1);

    mqttHAConnected = 1;
    xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 4096, NULL, 1, NULL, 1);
}
void onMqttMessageHA(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index,
                     size_t total)
{
    syslog_ng("onMqttMessageHA topic: " + String(topic) + " payload " + String(payload));

    String message = String(payload).substring(0, len);

    // Проверяем системные сообщения Home Assistant
    if (String(topic) == "homeassistant/status" || String(topic) == "homeassistant/growbox/status")
    {
        if (message == "online")
        {
            syslog_ng("Home Assistant is online.");
            syslog_ng("mqttHA server online");
            onMqttConnectHA(true);
        }
        else if (message == "offline")
        {
            syslog_ng("mqttHA server offline");
            syslog_ng("Home Assistant is offline.");
        }
        return;  // Выходим из функции для системных сообщений
    }

    // Обрабатываем сообщения для параметров
    for (Group &group : groups)
    {
        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];
            String paramTopic = mqttPrefix + "set/" + "main/" + String(group.caption) + "/" + String(param.name);
            String paramTopicHA = mqttPrefix + "switch/" + String(param.name) + "/set";

            if (String(topic).startsWith(mqttPrefix) && (paramTopic == topic || paramTopicHA == topic))
            {
                // Находим соответствующий параметр в хранилище
                PreferenceItem *item = findPreferenceByKey(param.name);
                if (!item)
                {
                    syslog_ng("Preference not found for: " + String(param.name));
                    return;
                }

                // Обновляем параметр в зависимости от его типа
                switch (param.type)
                {
                    case Param::INT:
                        if (message.length() > 0)
                        {
                            int newValue = message.toInt();
                            param.defaultInt = newValue;
                            item->preferences->putInt(param.name, newValue);
                            publish_switch_discovery_payload(param);
                            syslog_ng("Updated INT: " + String(param.name) + " = " + String(newValue));
                        }
                        else
                        {
                            syslog_ng("Invalid INT message");
                        }
                        break;

                    case Param::FLOAT:
                        if (message.length() > 0)
                        {
                            float newValue = message.toFloat();
                            param.defaultFloat = newValue;
                            item->preferences->putFloat(param.name, newValue);
                            publish_switch_discovery_payload(param);
                            syslog_ng("Updated FLOAT: " + String(param.name) + " = " + String(newValue));
                        }
                        else
                        {
                            syslog_ng("Invalid FLOAT message");
                        }
                        break;

                    case Param::STRING:
                        if (message.length() > 0)
                        {
                            String newValue = message;
                            param.defaultString = newValue.c_str();
                            item->preferences->putString(param.name, newValue);
                            publish_switch_discovery_payload(param);
                            syslog_ng("Updated STRING: " + String(param.name) + " = " + newValue);
                        }
                        else
                        {
                            syslog_ng("Invalid STRING message");
                        }
                        break;

                    default:
                        syslog_ng("Unknown parameter type");
                        break;
                }

                // Останавливаем обработку, если нашли параметр
                return;
            }
        }
    }

    // Если сообщение не обработано
    syslog_ng("Unknown topic: " + String(topic));
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index,
                   size_t total)
{
    String message;
    for (int i = 0; i < len; i++)
    {
        message += (char)payload[i];
    }
    syslog_ng("mqttt: onMqttMessage " + String(topic) + " msg: " + String(message));

    if (strcmp(topic, (mqttPrefix + String("set/Управление/restart")).c_str()) == 0)
    {
        if (strcmp("1", message.c_str()) == 0)
        {
            syslog_ng("mqtt restart");
            enqueueMessage(topic, "0");

            preferences.putString(pref_reset_reason, "mqtt rest");
            ESP.restart();
            return;  // After restarting, no need to proceed further
        }
    }

    if (strcmp(topic, (mqttPrefix + String("set/Управление/update")).c_str()) == 0)
    {
        if (strcmp("1", message.c_str()) == 0)
        {
            syslog_ng("mqtt update");
            update();
        }
    }

    if (strcmp(topic, (mqttPrefix + String("set/Управление/preferences_clear")).c_str()) == 0)
    {
        if (strcmp("1", message.c_str()) == 0)
        {
            syslog_ng("mqtt preferences_clear");
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
            return;  // After restarting, no need to proceed further
        }
    }

    if (strcmp(topic, (mqttPrefix + String("set/get_calibrate")).c_str()) == 0)
    {
        syslog_ng("mqtt calibrate_now calibrate_now " + String(calibrate_now));
        if (calibrate_now == 0)
        {
            calibrate_now = 1;
            syslog_ng("mqtt calibrate_now message.c_str() " + String(message.c_str()));

            if (!strcmp("temp-tab", message.c_str()))
            {
                NTC_void();  // Датчик температуры NTC
                DS18B20();   // Температурный сенсор Dallas
                publish_parameter("RootTemp", RootTemp, 3, 0);
                publish_parameter("AirTemp", AirTemp, 3, 0);
                publish_parameter("NTC", NTC, 3, 0);
                publish_parameter("wNTC", wNTC, 3, 0);

                syslog_ng("mqtt calibrate_now temp");
            }

            if (!strcmp("ec-tab", message.c_str()))
            {
                int enabled_kalman = 0;
                if (EC_KAL_E == 1)
                {
                    enabled_kalman = 1;
                }
                EC_KAL_E = 0;
                NTC_void();  // Датчик температуры NTC
                EC_void();   // Электропроводимость
                publish_parameter("RootTemp", RootTemp, 3, 0);
                publish_parameter("NTC", NTC, 3, 0);
                publish_parameter("Ap", Ap, 3, 0);
                publish_parameter("An", An, 3, 0);
                publish_parameter("wNTC", wNTC, 3, 0);
                publish_parameter("wEC", wEC, 3, 0);
                publish_parameter("wECnt", ec_notermo, 3, 0);
                publish_parameter("wR2", wR2, 3, 0);
                publish_parameter("uptime", millis() / 1000, 0, 0);
                syslog_ng("mqtt calibrate_now ec");
                if (enabled_kalman == 1)
                {
                    EC_KAL_E = 1;
                }
            }

            if (!strcmp("ph-tab", message.c_str()))
            {
                MCP3421();  // Температурный сенсор
                publish_parameter("pHmV", pHmV, 3, 0);
                publish_parameter("pHraw", pHraw, 3, 0);
                publish_parameter("wpH", wpH, 3, 0);
                publish_parameter("uptime", millis() / 1000, 0, 0);
                syslog_ng("mqtt MCP3421");
            }

            if (!strcmp("photoresistor-tab", message.c_str()))
            {
                PR_void();  // Фоторезистор
                publish_parameter("PR", PR, 3, 0);
                publish_parameter("wPR", wPR, 3, 0);
                publish_parameter("uptime", millis() / 1000, 0, 0);
                syslog_ng("mqtt PR_void");
            }

            if (!strcmp("bak-tab", message.c_str()))
            {
                TaskUS();  // pH метр
                publish_parameter("Dist", Dist, 3, 0);
                publish_parameter("DstRAW", DstRAW, 3, 0);
                publish_parameter("wLevel", wLevel, 3, 0);
                publish_parameter("uptime", millis() / 1000, 0, 0);
                syslog_ng("mqtt bak");
            }

            syslog_ng("mqtt get_calibrate");

            calibrate_now = 0;

            return;
        }
    }

    JsonDocument jsonDoc;

    if (strcmp(topic, (mqttPrefix + String("set/preferences/all")).c_str()) == 0)
    {
        // Декодируем JSON из строки
        DeserializationError error = deserializeJson(jsonDoc, message.c_str());
        syslog_ng("JSON mqtt preferences/all");
        if (error)
        {
            syslog_ng("JSON decode failed: " + String(error.c_str()));
            return;
        }

        // Итерируйтесь по вашему массиву preferencesArray
        for (int i = 0; i < sizeof(preferencesArray) / sizeof(PreferenceItem); ++i)
        {
            // Проверяем, есть ли значение для текущего ключа в JSON
            if (jsonDoc[preferencesArray[i].key].is<JsonVariant>())
            {
                // Определяем тип переменной и обновляем ее значение в зависимости от
                // типа
                switch (preferencesArray[i].type)
                {
                    case DataType::FLOAT:
                        if (preferencesArray[i].preferences->getFloat(preferencesArray[i].key) !=
                            jsonDoc[preferencesArray[i].key].as<float>())
                        {
                            *(float *)preferencesArray[i].variable = jsonDoc[preferencesArray[i].key].as<float>();
                            if (preferencesArray[i].preferences->putFloat(
                                    preferencesArray[i].key, jsonDoc[preferencesArray[i].key].as<float>()) == 0)
                            {
                                syslog_ng("mqtt error save pref " + String(preferencesArray[i].key));
                            }
                        }
                        break;
                    case DataType::STRING:
                        if (preferencesArray[i].preferences->getString(preferencesArray[i].key) !=
                            jsonDoc[preferencesArray[i].key].as<const char *>())
                        {
                            *(String *)preferencesArray[i].variable =
                                jsonDoc[preferencesArray[i].key].as<const char *>();
                            if (preferencesArray[i].preferences->putString(
                                    preferencesArray[i].key, jsonDoc[preferencesArray[i].key].as<const char *>()) == 0)
                            {
                                syslog_ng("mqtt error save pref " + String(preferencesArray[i].key));
                            }
                        }
                        break;
                    case DataType::BOOLEAN:
                        if (preferencesArray[i].preferences->getBool(preferencesArray[i].key) !=
                            jsonDoc[preferencesArray[i].key].as<bool>())
                        {
                            *(bool *)preferencesArray[i].variable = jsonDoc[preferencesArray[i].key].as<bool>();
                            if (preferencesArray[i].preferences->putBool(
                                    preferencesArray[i].key, jsonDoc[preferencesArray[i].key].as<bool>()) == 0)
                            {
                                syslog_ng("mqtt error save pref " + String(preferencesArray[i].key));
                            }
                        }
                        break;
                    case DataType::INTEGER:
                        if (preferencesArray[i].preferences->getInt(preferencesArray[i].key) !=
                            jsonDoc[preferencesArray[i].key].as<int>())
                        {
                            *(int *)preferencesArray[i].variable = jsonDoc[preferencesArray[i].key].as<int>();
                            if (preferencesArray[i].preferences->putInt(
                                    preferencesArray[i].key, jsonDoc[preferencesArray[i].key].as<int>()) == 0)
                            {
                                syslog_ng("mqtt error save pref " + String(preferencesArray[i].key));
                            }
                        }
                        break;
                }
                if (is_setup == 0)
                {
                    publish_one_data(preferencesArray[i]);
                }
            }
        }
    }

    for (Group &group : groups)
    {
        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];

            String paramTopic = mqttPrefix + "set/" + String(group.caption) + "/" + String(param.name);
            if (String(topic).startsWith(mqttPrefix) && paramTopic == topic)  // Check if topic starts with prefix
            {
                PreferenceItem *item = findPreferenceByKey(param.name);
                // Topic corresponds to parameter, processing message
                switch (param.type)
                {
                    case Param::INT:
                        if (message.length() > 0)
                        {  // Add your own validation function isInteger

                            item->preferences->putInt(param.name, message.toInt());
                            syslog_ng("mqtt " + String(param.name) + " message " + message);
                            publish_switch_discovery_payload(param);
                        }
                        else
                        {
                            syslog_ng("mqtt Received INT message is invalid");
                        }
                        break;
                    case Param::FLOAT:
                        if (message.length() > 0)
                        {  // Add your own validation function isFloat
                            item->preferences->putFloat(param.name, message.toFloat());
                            syslog_ng("mqtt " + String(param.name) + " message " + message);
                            publish_switch_discovery_payload(param);
                        }
                        else
                        {
                            syslog_ng("mqtt Received FLOAT message is invalid");
                        }
                        break;
                    case Param::STRING:
                        if (message.length() > 0)
                        {
                            item->preferences->putString(param.name, message);
                            syslog_ng("mqtt " + String(param.name) + " message " + message);
                            publish_switch_discovery_payload(param);
                        }
                        else
                        {
                            syslog_ng("mqtt Received STRING message is empty");
                        }
                        break;
                    default:
                        syslog_ng("mqtt Unknown parameter type");
                        break;
                }
            }
        }
    }
}
void TaskMqtt(void *parameters)
{
    for (;;)
    {
        unsigned long start_mqtt;
        start_mqtt = millis();
        syslog_ng("start mqtt TaskMqtt");
        while (OtaStart == true) vTaskDelay(1000);
        publish_params_all();
        make_update();
        syslog_ng("mqtt TaskMqtt end " + fFTS((millis() - start_mqtt), 0) + " ms");
        vTaskDelay(DEFAULT_DELAY);
    }
}