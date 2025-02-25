
bool updatePreferenceValue(PreferenceItem *item, const String &value)
{
    String s;
    if (item == nullptr)
    {
        syslog_ng("ERROR: PreferenceItem is null.");
        return false;  // Выход, если item равен nullptr
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
            syslog_ng("ERROR: Unsupported data type for key: " + String(item->key));
            return false;  // Выход, если тип данных не поддерживается
    }

    // Логируем успешное обновление
    syslog_ng("Updated preference for key: " + String(item->key) + " with value: " + String(value));
    return true;
}

bool updatePreference(const char *settingName, const JsonVariant &value)
{
    if (PreferenceItem *item = findPreferenceByKey(settingName))
    {
        switch (item->type)
        {
            case DataType::STRING:
                return updatePreferenceValue(item, value.as<String>());
            case DataType::INTEGER:
                return updatePreferenceValue(item, String(value.as<int>()));
            case DataType::FLOAT:
                return updatePreferenceValue(item, String(value.as<float>()));
            case DataType::BOOLEAN:
                return updatePreferenceValue(item, value.as<bool>() ? "true" : "false");
            default:
                syslog_ng("ERROR: Unsupported data type for key: " + String(settingName));
                return false;  // Выход, если тип данных не поддерживается
        }
    }
    else
    {
        syslog_ng("ERROR: Preference item not found for key: " + String(settingName));
        return false;
    }
}

// Функция для работы со строкой
bool updatePreference(const char *settingName, const String &value)
{
    if (PreferenceItem *item = findPreferenceByKey(settingName))
    {
        return updatePreferenceValue(item, value);
    }
    else
    {
        syslog_ng("ERROR: Preference item not found for key: " + String(settingName));
        return false;
    }
}

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

    String state_topic = mqttPrefix + timescale_prefix + switch_name;
    String command_topic = mqttPrefix + timescale_prefix + switch_name;  // Исправлено на "/set"

    // Проверка на наличие имени переключателя
    if (switch_name.length() == 0)
    {
        syslog_ng("ERROR: Switch name is empty.");
        return;
    }

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

    // Проверка наличия элемента предпочтений
    PreferenceItem *item = findPreferenceByKey(switch_name.c_str());

    if (item != nullptr)
    {
        if (item->variable != nullptr & item->variable != nullptr)
        {
            mqttClientHA.subscribe(command_topic.c_str(), qos);
            String value = (strlen((const char *)item->variable) > 0) ? String((const char *)item->variable) : "0";
            syslog_ng("mqttClientHA publish_switch_discovery_payload state " + String(switch_name) + " state_topic " +
                      state_topic + " (const char *)item->variable " + value);
            enqueueMessage(state_topic.c_str(), value.c_str(), "", false);
        }
        else
        {
            syslog_ng("ERROR: publish_switch_discovery_payload Variable is null for " + String(switch_name));
        }
    }
    else
    {
        syslog_ng("ERROR: publish_switch_discovery_payload Could not find preferences for " + String(switch_name));
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
        }
    }
}

void onMqttReconnectTimerHa(TimerHandle_t xTimer)
{
    connectToMqttHA();  // Попытка повторного подключения к MQTT
}


 
 void publishVariablesListToMQTT()
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    int arraySize = sizeof(preferencesArray) / sizeof(preferencesArray[0]);  // Определяем количество элементов в массиве

    for (int i = 0; i < arraySize; ++i)
    {
        String key = preferencesArray[i].key;

        PreferenceItem *item = findPreferenceByKey(key.c_str());

        if (item == nullptr)
        {
            syslog_ng("ERROR: Preference item not found for key: " + key);
            continue;
        }

        if (item->preferences != nullptr)
        {
            switch (item->type)
            {
                case DataType::STRING:
                    root[key] = item->preferences->getString(key.c_str(), *(String *)item->variable);
                    break;
                case DataType::INTEGER:
                    root[key] = item->preferences->getInt(key.c_str(), *(int *)item->variable);
                    break;
                case DataType::FLOAT:
                    root[key] = item->preferences->getFloat(key.c_str(), *(float *)item->variable);
                    break;
                case DataType::BOOLEAN:
                    root[key] = item->preferences->getBool(key.c_str(), *(bool *)item->variable);
                    break;
                default:
                    syslog_ng("ERROR: Unsupported data type for key: " + key);
                    break;
            }
        }
        else
        {
            syslog_ng("ERROR: Preferences not found for key: " + key);
        }
    }

    // Сериализуем JSON объект в строку
    String output;
    serializeJson(doc, output);

    // Создаем строку топика
    String topic = mqttPrefix + preferences_prefix + "all_variables";

    // Отправляем JSON полезную нагрузку через MQTT
    enqueueMessage(topic.c_str(), output.c_str(), "", false);
    vTaskDelay(1);
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
    vTaskDelay(1);
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
            }
        }
    }
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

void mqttTaskHA(void *parameter)
{
    publish_discovery_payload("AirHum");
    publish_discovery_payload("AirPress");
    publish_discovery_payload("AirTemp");
    publish_discovery_payload("An");
    publish_discovery_payload("Ap");
    publish_discovery_payload("CPUTemp");
    publish_discovery_payload("Dist");
    publish_discovery_payload("DistRaw");
    publish_discovery_payload("Dist_Kalman");
    publish_discovery_payload("DstRAW");
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
    publish_discovery_payload("db");
    publish_discovery_payload("ipadress");
    publish_discovery_payload("local_ip");
    publish_discovery_payload("pHmV");
    publish_discovery_payload("pHraw");
    publish_discovery_payload("readGPIO");
    publish_discovery_payload("RootVPD");
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
    xTaskCreatePinnedToCore(mqttTaskHA, "MQTT HA Task", 4096, NULL, 1, NULL, 1);
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
            String paramTopicHA = mqttPrefix + timescale_prefix + String(param.name);
            if (String(topic).startsWith(mqttPrefix) && (paramTopic == topic || paramTopicHA == topic))
            {
                if (updatePreference(param.name, message))
                {
                    // Останавливаем обработку, если нашли параметр
                    return;
                }
            }
        }
    }

    // Если сообщение не обработано
    syslog_ng("mqtt Unknown topic: " + String(topic));
}
