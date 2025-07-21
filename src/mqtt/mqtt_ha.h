
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
        syslogf("ERROR: Preferences are null for key: %s", String(item->key));
        return false;  // Exit if preferences are nullptr
    }

    if (item->variable == nullptr)
    {
        syslogf("ERROR: Variable is null for key: %s", String(item->key));
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
            syslogf("ERROR: Unsupported data type for key: %s", String(item->key));
            return false;  // Выход, если тип данных не поддерживается
    }

    // Логируем успешное обновление

    // Проверка длины строки
    if (s.length() > 0)
    {
        publish_one_data(item, mqtt_type);
        syslogf("Updated preference for key: %s with value: %s", String(item->key), String(value));
        return true;
    }
    else
    {
        syslogf("Failed to update preference for key: %s with value: %s", String(item->key), String(value));
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
                syslogf("ERROR: Unsupported data type for key: %s", String(settingName));
                return false;  // Выход, если тип данных не поддерживается
        }
    }
    else
    {
        syslogf("ERROR: Preference item not found for key: %s", String(settingName));
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
        syslogf("ERROR: Preference item not found for key: %s", String(settingName));
        return false;
    }
    return false;
}

void publish_discovery_payload(const char *sensor_name)
{
    StaticJsonDocument<512> doc;
    char buffer[512];

    doc["name"] = sensor_name;
    doc["state_topic"] = update_token + "/" + timescale_prefix + sensor_name;
    doc["state_class"] = "measurement";
    doc["unique_id"] = String(HOSTNAME) + "_" + sensor_name;
    doc["device"]["identifiers"][0] = HOSTNAME;
    doc["device"]["name"] = HOSTNAME;
    doc["device"]["model"] = "GROWBOX";
    doc["device"]["manufacturer"] = "PONICS.ONLINE";

    serializeJson(doc, buffer);
    String discovery_topic = "homeassistant/sensor/" + String(sensor_name) + "__" + String(HOSTNAME) + "/config";
    enqueueMessage(discovery_topic.c_str(), buffer, "", "ha");
}

void publish_switch_discovery_payload(Param param)
{
    String switch_name = String(param.name);
    StaticJsonDocument<512> doc;
    char buffer[512];
    String command_topic = update_token + "/" + "set/" + String(param.name);

    // Проверка на наличие имени переключателя
    if (switch_name.length() == 0)
    {
        syslogf("mqttHA ERROR: Switch name is empty.");
        return;
    }

    doc["name"] = switch_name;             // Имя устройства
    doc["command_topic"] = command_topic;  // Топик для команд
    doc["state_topic"] = command_topic;    // Топик для состояния

    doc["unique_id"] = String(HOSTNAME) + "_" + switch_name;  // Уникальный ID устройства
    doc["payload_on"] = 1;                                    // Значение для включения
    doc["payload_off"] = 0;                                   // Значение для выключения
    doc["retain"] = false;                                    // Сообщения сохраняются

    // Информация об устройстве (для Home Assistant Device Registry)
    doc["device"]["identifiers"][0] = HOSTNAME;
    doc["device"]["name"] = HOSTNAME;
    doc["device"]["model"] = "GROWBOX";
    doc["device"]["manufacturer"] = "PONICS.ONLINE";

    // Сериализуем JSON
    serializeJson(doc, buffer);

    // Публикуем сообщение в топик Discovery
    String discovery_topic = "homeassistant/switch/" + String(switch_name) + "__" + String(HOSTNAME) + "/config";

    enqueueMessage(discovery_topic.c_str(), buffer, "", "ha");

    // Проверка наличия элемента предпочтений
    PreferenceItem *item = findPreferenceByKey(switch_name.c_str());

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
                    enqueueMessage(command_topic.c_str(), value.c_str(), "", "ha");
                    syslogf("mqttHA mqttClientHA publish switch state %s value %s", String(switch_name), String(value));
                }
            }

            mqttClientHA.subscribe(command_topic.c_str(), qos);
        }
        else
        {
            syslogf("ERROR: mqtt publish switch  Variable is null for %s", String(switch_name));
        }
    }
    else
    {
        syslogf("mqtt ERROR: mqtt publish switch  Could not find preferences for %s", String(switch_name));
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
    syslogf("mqtt mqttClientHA Disconnected. Reason: %s", String((int)reason));
    syslogf("mqtt mqttClientHA: WiFi isConnected: %s", String(WiFi.isConnected()));
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
            syslogf("mqtt mqttClientHA a_ha: \"%s\", p_ha: \"%s\", u_ha: \"%s\"", String(a_ha), String(p_ha),
                    String(u_ha));
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
    StaticJsonDocument<4096> doc;
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
                syslogf("mqtt nullptr: %s", String(param.name));
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
                syslogf("mqtt ERROR: Could not find preferences for %s", String(param.name));
            }
        }
    }

    // Serialize JSON object to string
    String output;
    serializeJson(doc, output);
    enqueueMessage((update_token + "/" + main_prefix + "all_groups").c_str(), output.c_str());
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
        syslogf("mqtt onMqttMessageHA topic: %s null ", String(topic));

        return;
    }
    syslogf("mqtt HA onMqttMessageHA topic: %s payload %s", String(topic), String(payload));

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
            String paramTopic = update_token + "/" + "set/" + String(param.name);
            if (paramTopic == topic)
            {
                if (updatePreference(param.name, message, "usual"))
                {
                    return;
                }
            }
        }
    }

    // Если сообщение не обработано
    syslogf("mqtt HA Unknown topic: %s", String(topic));
}
