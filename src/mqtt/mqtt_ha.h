
bool updatePreferenceValue(PreferenceItem *item, const String &value, String mqtt_type = "all")
{
    String s;
    if (item == nullptr)
    {
        syslogf("ERROR: PreferenceItem is null.");
        return false;  // Exit if item is nullptr
    }

    if (item->preferences == nullptr)
    {
        syslogf("ERROR: Preferences are null for key: %s", item->key);
        return false;  // Exit if preferences are nullptr
    }

    if (item->variable == nullptr)
    {
        syslogf("ERROR: Variable is null for key: %s", item->key);
        return false;  // Exit if variable is nullptr
    }
    switch (item->type)
    {
        case DataType::STRING:
            *(String *)item->variable = value;  // Прямое присваивание строки
            s = item->preferences->putString(item->key, *(String *)item->variable);
            break;
        case DataType::INTEGER:
            *(int *)item->variable = value.toInt();  // Преобразование строки в int
            s = item->preferences->putInt(item->key, *(int *)item->variable);
            break;
        case DataType::FLOAT:
            *(float *)item->variable = value.toFloat();  // Преобразование строки в float
            s = item->preferences->putFloat(item->key, *(float *)item->variable);
            break;
        case DataType::BOOLEAN:
            *(bool *)item->variable = (value == "true");  // Преобразование строки в bool
            s = item->preferences->putBool(item->key, *(bool *)item->variable);
            break;
        default:
            syslogf("ERROR: Unsupported data type for key: %s", item->key);
            return false;  // Выход, если тип данных не поддерживается
    }

    // Логируем успешное обновление

    // Проверка длины строки
    if (s.length() > 0)
    {
        publish_one_data(item, mqtt_type);
        syslogf("Updated preference for key: %s with value: %s", item->key, value);
        return true;
    }
    else
    {
        syslogf("Failed to update preference for key: %s with value: %s", item->key, value);
    }
    return false;
}

bool updatePreference(const char *settingName, const JsonVariant &value, String mqtt_type = "all")
{
    if (PreferenceItem *item = findPreferenceByKey(settingName))
    {
        switch (item->type)
        {
            case DataType::STRING:
                return updatePreferenceValue(item, value.as<String>(), mqtt_type);
            case DataType::INTEGER:
                return updatePreferenceValue(item, String(value.as<int>()), mqtt_type);
            case DataType::FLOAT:
                return updatePreferenceValue(item, String(value.as<float>()), mqtt_type);
            case DataType::BOOLEAN:
                return updatePreferenceValue(item, value.as<bool>() ? "true" : "false", mqtt_type);
            default:
                syslogf("ERROR: Unsupported data type for key: %s", settingName);
                return false;  // Выход, если тип данных не поддерживается
        }
    }
    else
    {
        syslogf("ERROR: Preference item not found for key: %s", settingName);
        return false;
    }
    return false;
}

// Функция для работы со строкой
bool updatePreference(const char *settingName, const String &value, String mqtt_type = "all")
{
    if (PreferenceItem *item = findPreferenceByKey(settingName))
    {
        return updatePreferenceValue(item, value, mqtt_type);
    }
    else
    {
        syslogf("ERROR: Preference item not found for key: %s", settingName);
        return false;
    }
    return false;
}

void publish_discovery_payload(const char *sensor_name)
{
    StaticJsonDocument<512> docdiscovery;
    docdiscovery.clear();
    char buffer[512];
    char topic[256];
    char unique_id[128];

    // Format state topic
    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), "%s/%s%s", update_token.c_str(), timescale_prefix.c_str(), sensor_name);

    // Format unique_id
    snprintf(unique_id, sizeof(unique_id), "%s_%s", HOSTNAME, sensor_name);

    // Set JSON values
    docdiscovery["name"] = sensor_name;
    docdiscovery["state_topic"] = state_topic;
    docdiscovery["state_class"] = "measurement";
    docdiscovery["unique_id"] = unique_id;
    docdiscovery["device"]["identifiers"][0] = HOSTNAME;
    docdiscovery["device"]["name"] = HOSTNAME;
    docdiscovery["device"]["model"] = "GROWBOX";
    docdiscovery["device"]["manufacturer"] = "PONICS.ONLINE";

    // Serialize and publish
    serializeJson(docdiscovery, buffer);

    // Format discovery topic
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s__%s/config", sensor_name, HOSTNAME);

    enqueueMessage(topic, buffer, "", "ha");
}

void publish_switch_discovery_payload(Param param)
{
    // Check if param.name is valid
    if (strlen(param.name) == 0)
    {
        syslogf("mqttHA ERROR: Switch name is empty.");
        return;
    }

    StaticJsonDocument<512> doc;
    char buffer[512];
    char command_topic[128];
    char unique_id[128];
    char discovery_topic[192];

    // Format command topic
    snprintf(command_topic, sizeof(command_topic), "%s/set/%s", update_token.c_str(), param.name);

    // Format unique_id
    snprintf(unique_id, sizeof(unique_id), "%s_%s", HOSTNAME, param.name);

    // Format discovery topic
    snprintf(discovery_topic, sizeof(discovery_topic), "homeassistant/switch/%s__%s/config", param.name, HOSTNAME);

    // Build JSON document
    doc["name"] = param.name;              // Device name
    doc["command_topic"] = command_topic;  // Command topic
    doc["state_topic"] = command_topic;    // State topic
    doc["unique_id"] = unique_id;          // Unique device ID
    doc["payload_on"] = 1;                 // Value for ON
    doc["payload_off"] = 0;                // Value for OFF
    doc["retain"] = false;                 // Don't retain messages

    // Device information for Home Assistant
    doc["device"]["identifiers"][0] = HOSTNAME;
    doc["device"]["name"] = HOSTNAME;
    doc["device"]["model"] = "GROWBOX";
    doc["device"]["manufacturer"] = "PONICS.ONLINE";

    // Serialize and publish
    serializeJson(doc, buffer);
    enqueueMessage(discovery_topic, buffer, "", "ha");

    // Check for preference item
    PreferenceItem *item = findPreferenceByKey(param.name);

    if (item != nullptr)
    {
        if (item->variable != nullptr)
        {
            if (item->variable)
            {
                String value;

                switch (item->type)
                {
                    case DataType::STRING:
                        if (strlen((const char *)item->variable) > 0)
                        {
                            value = String((const char *)item->variable);
                        }
                        break;

                    case DataType::INTEGER:
                        value = String(*reinterpret_cast<int *>(item->variable));
                        break;

                    case DataType::FLOAT:
                        value = String(*reinterpret_cast<float *>(item->variable), 2);  // 2 знака после запятой
                        break;

                    case DataType::BOOLEAN:
                        value = (*reinterpret_cast<bool *>(item->variable)) ? "true" : "false";
                        break;

                    default:
                        value = "0";
                        break;
                }

                // Отправляем сообщение и логируем состояние
                if (!value.isEmpty())
                {
                    enqueueMessage(command_topic, value.c_str(), "", "ha");
                    syslogf("mqttHA mqttClientHA publish switch state %s value %s", param.name, value);
                }
            }

            mqttClientHA.subscribe(command_topic, qos);
        }
        else
        {
            syslogf("ERROR: mqtt publish switch  Variable is null for %s", param.name);
        }
    }
    else
    {
        syslogf("mqtt ERROR: mqtt publish switch  Could not find preferences for %s", param.name);
    }
}

void subscribe_param_ha(Param param)
{
    String command_topic = update_token + "/" + "set/" + String(param.name);
    mqttClientHA.subscribe(command_topic.c_str(), qos);
}

void onMqttDisconnectHA(AsyncMqttClientDisconnectReason reason)
{
    mqttClientHAConnected = false;
    syslogf("mqtt mqttClientHA Disconnected. Reason: %d", reason);
    syslogf("mqtt mqttClientHA: WiFi isConnected: %d", WiFi.isConnected());
    if (WiFi.isConnected())
    {
        if (not mqttClientHA.connected())
        {
            syslogf("mqtt: Starting reconnect timer HA");
            xTimerStart(mqttReconnectTimerHa, 0);
        }
    }
}

void connectToMqttHA()
{
    if (e_ha == 1)
    {
        if (not mqttClientHA.connected())
        {
            mqttHAConnected = 0;
            syslogf("mqtt mqttClientHA a_ha: \"%s\", p_ha: \"%s\", u_ha: \"%s\"", a_ha, p_ha, u_ha);
            syslogf("mqtt mqttClientHA connectToMqtt Connecting to MQTT...");
            mqttClientHA.connect();
            mqtt_not_connected_counter = mqtt_not_connected_counter + 1;
            if (mqtt_not_connected_counter > 100)
            {
                syslogf("mqtt mqttClientHA connectToMqtt ESP RESTART");
                preferences.putString(pref_reset_reason, "mqttHA error");
                mqtt_not_connected_counter = 0;
            }
        }
        else
        {
            mqtt_not_connected_counter = 0;
        }
    }
    else
    {
        syslogf("mqtt mqttClientHA not enabled");
    }
}

void onMqttReconnectTimerHa(TimerHandle_t xTimer)
{
    connectToMqttHA();  // Попытка повторного подключения к MQTT
}

void publish_setting_groups()
{
    StaticJsonDocument<16384> docsetting_groups;
    JsonObject root = docsetting_groups.to<JsonObject>();

    for (Group &group : groups)
    {
        JsonObject groupJson = root[group.caption].to<JsonObject>();  // Use the new syntax for creating nested objects

        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];
            PreferenceItem *item = findPreferenceByKey(param.name);

            if (item == nullptr)
            {
                if (param.name)
                {
                    syslogf("mqtt nullptr: %s", param.name);
                }
                else
                {
                    syslogf("mqtt nullptr: [unnamed parameter]");
                }
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
                if (param.name)
                {
                    syslogf("mqtt ERROR: Could not find preferences for %s", param.name);
                }
                else
                {
                    syslogf("mqtt ERROR: Could not find preferences for [unnamed parameter]");
                }
            }
        }
    }

    // Serialize JSON object to string
    String output;
    serializeJson(docsetting_groups, output);

    String topic = update_token + "/" + main_prefix + "all_groups";
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

            if (String(param.label).indexOf("(0-off, 1-on)") != -1)
            {
                publish_switch_discovery_payload(param);
                subscribe_param_ha(param);
            }
        }
    }
}

void mqttTaskHA(void *parameter)
{
    syslogf("mqtt mqttTaskHA start");
    publish_discovery_payload("AirHum");
    publish_discovery_payload("AirPress");
    publish_discovery_payload("AirTemp");
    publish_discovery_payload("wSalt");
    publish_discovery_payload("CPUTemp");
    publish_discovery_payload("Dist");
    publish_discovery_payload("Dist_Kalman");
    publish_discovery_payload("EC_Kalman");
    publish_discovery_payload("EC_notermo_Kalman");
    publish_discovery_payload("Kornevoe");
    publish_discovery_payload("NTC_Kalman");
    publish_discovery_payload("NTC_RAW");
    publish_discovery_payload("PR");
    publish_discovery_payload("PWD1");
    publish_discovery_payload("PWD2");
    publish_discovery_payload("RSSI");
    publish_discovery_payload("RootTemp");
    publish_discovery_payload("Vcc");
    publish_discovery_payload("AirVPD");
    publish_discovery_payload("commit");
    publish_discovery_payload("readGPIO");
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
    syslogf("mqtt mqttTaskHA processToggleParameters end");

    vTaskDelete(NULL);  // Удаляем задачу после выполнения
}

void onMqttConnectHA(bool sessionPresent)
{
    syslogf("mqtt onMqttConnectHA mqttHA connected");
    mqttClientHA.subscribe("homeassistant/status", 1);
    mqttClientHA.subscribe("homeassistant/growbox/status", 1);
    mqttHAConnected = 1;
    mqttClientHAConnected = true;
    xTaskCreatePinnedToCore(mqttTaskHA, "MQTT HA Task", 4096, NULL, 1, NULL, 1);
}
void onMqttMessageHA(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index,
                     size_t total)
{
    if (payload == nullptr)
    {
        syslogf("mqtt onMqttMessageHA topic: %s null ", topic);

        return;
    }
    syslogf("mqtt HA onMqttMessageHA topic: %s payload %s", topic, payload);

    String message = String(payload).substring(0, len);

    // Проверяем системные сообщения Home Assistant
    if (String(topic) == "homeassistant/status" || String(topic) == "homeassistant/growbox/status")
    {
        if (message == "online")
        {
            syslogf("mqtt Home Assistant is online.");
            syslogf("mqtt mqttHA server online");
            onMqttConnectHA(true);
        }
        else if (message == "offline")
        {
            syslogf("mqtt mqttHA server offline");
            syslogf("mqtt Home Assistant is offline.");
        }
        return;  // Выходим из функции для системных сообщений
    }

    // Обрабатываем сообщения для параметров
    for (Group &group : groups)
    {
        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];

            // Format the parameter topic using snprintf
            const size_t topicSize = update_token.length() + 1 + strlen("set/") + strlen(param.name) + 1;
            char paramTopic[topicSize];
            snprintf(paramTopic, topicSize, "%s/set/%s", update_token.c_str(), param.name);

            if (strcmp(paramTopic, topic) == 0)
            {
                if (updatePreference(param.name, message, "usual"))
                {
                    return;
                }
            }
        }
    }

    // Если сообщение не обработано
    syslogf("mqtt HA Unknown topic: %s", topic);
}
