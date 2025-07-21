#include <map>
#include <string>

// Map to store last update times for each parameter
std::map<std::string, unsigned long> paramLastUpdate;
// 5 seconds cooldown between parameter updates
const unsigned long PARAM_UPDATE_COOLDOWN = 5000;

void publishVariablesListToMQTT()
{
    StaticJsonDocument<4096> doc;
    JsonObject root = doc.to<JsonObject>();

    int arraySize =
        sizeof(preferencesArray) / sizeof(preferencesArray[0]);  // Определяем количество элементов в массиве

    for (int i = 0; i < arraySize; ++i)
    {
        String key = preferencesArray[i].key;

        PreferenceItem *item = findPreferenceByKey(key.c_str());

        if (item == nullptr)
        {
            syslogf("ERROR: Preference item not found for key: %s", String(key));
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
                    syslogf("ERROR: Unsupported data type for key: %s", String(key));
                    break;
            }
        }
        else
        {
            syslogf("ERROR: Preferences not found for key: %s", String(key));
        }
    }

    // Сериализуем JSON объект в строку
    String output;
    serializeJson(doc, output);

    // Создаем строку топика
    String topic = update_token + "/" + preferences_prefix + "all_variables";

    // Отправляем JSON полезную нагрузку через MQTT
    enqueueMessage(topic.c_str(), output.c_str(), "", "all");
    vTaskDelay(10);
}

void subscribe()
{
    String mqttPrefixSet = update_token + "/" + "set/#";
    String mqttPrefixtest = update_token + "/" + "test/#";
    syslogf("mqtt subscribe mqttPrefixSet: %s", mqttPrefixSet);
    syslogf("mqtt subscribe mqttPrefixSet: %s", mqttPrefixtest);
    mqttClient.subscribe(mqttPrefixSet.c_str(), qos);  // Subscribing to topic with prefix
    mqttClient.subscribe(mqttPrefixtest.c_str(), qos);
    syslogf("mqtt end subscribe");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    mqttClientPonicsConnected = true;
    static uint8_t reconnectAttempts = 0;
    const uint8_t MAX_RECONNECT_ATTEMPTS = 100;  // Maximum number of reconnection attempts

    syslogf("Disconnected from MQTT. Reason: %d", (int)reason);

    // Check if WiFi is still connected
    if (WiFi.status() != WL_CONNECTED)
    {
        syslogf("WiFi not connected, waiting for WiFi before MQTT reconnection");
        return;
    }

    // Increment reconnect attempts
    reconnectAttempts++;

    if (reconnectAttempts > MAX_RECONNECT_ATTEMPTS)
    {
        syslogf("Max MQTT reconnection attempts reached. Restarting...");
        shouldReboot = true;
        return;
    }

    // Exponential backoff with max 5 minutes
    if (reconnectDelay < 300000)
    {  // 5 minutes max
        reconnectDelay = min(reconnectDelay * 2, 300000);
    }

    syslogf("Reconnect attempt %d/%d in %d seconds", reconnectAttempts, MAX_RECONNECT_ATTEMPTS, reconnectDelay / 1000);

    // Clean up any existing MQTT state
    mqttClient.disconnect(true);

    // Schedule reconnection
    xTimerChangePeriod(mqttReconnectTimer, pdMS_TO_TICKS(reconnectDelay), 0);
    xTimerStart(mqttReconnectTimer, 0);
}

void connectToMqtt()
{
    // Don't attempt MQTT connection if WiFi isn't connected
    if (WiFi.status() != WL_CONNECTED)
    {
        syslogf("MQTT: WiFi not connected, skipping MQTT connection attempt");
        return;
    }

    if (not mqttClient.connected())
    {
        syslogf("MQTT: Attempting to connect to MQTT broker...");

        // Ensure TCP/IP stack is ready
        if (!WiFi.isConnected())
        {
            syslogf("MQTT: WiFi not ready, delaying MQTT connection");
            return;
        }

        // Reset the connection state
        mqttClient.disconnect(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        // Attempt to connect
        mqttClient.connect();
        vTaskDelay(pdMS_TO_TICKS(100));
        if (mqttClient.connected())
        {
            syslogf("MQTT: Connection attempt initiated");
            mqtt_not_connected_counter = 0;
        }
        else
        {
            mqtt_not_connected_counter++;
            syslogf("MQTT: Connection attempt failed (%d)", mqtt_not_connected_counter);

            if (mqtt_not_connected_counter > 100)
            {
                syslogf("MQTT: Too many connection failures, scheduling restart");
                preferences.putString(pref_reset_reason, "mqtt error");
                delay(100);
                ESP.restart();
            }
        }
    }
    else
    {
        // Reset counter on successful connection
        mqtt_not_connected_counter = 0;
    }
}
void onMqttReconnectTimer(TimerHandle_t xTimer)
{
    connectToMqtt();  // Попытка повторного подключения к MQTT
}

void onMqttConnect(bool sessionPresent)
{
    reconnectDelay = 1000;
    reconnectAttempts = 0;
    mqttClientPonicsConnected = true;

    syslogf("mqtt onMqttConnect mqtt connected start subscribe");
    subscribe();
    syslogf("mqtt onMqttConnect subscribed");
    if (first_time)
    {
        String statusTopic = update_token + "/" + "status";
        enqueueMessage(statusTopic.c_str(), "connected");
        syslogf("Before publish_setting_groups ");
        publish_setting_groups();
        syslogf("Before publishVariablesListToMQTT ");
        publishVariablesListToMQTT();
        first_time = false;
        syslogf("After publishVariablesListToMQTT");
    }
    syslogf("mqtt onMqttConnect end");
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index,
                   size_t total)
{
    String message;

    StaticJsonDocument<512> jsonDoc;  // зарезервировать память
    for (int i = 0; i < len; i++)
    {
        message += (char)payload[i];
    }
    syslogf("mqttt: onMqttMessage %s msg: %s", String(topic), String(message));

    if (strcmp(topic, (update_token + "/" + String("set/Управление/restart")).c_str()) == 0)
    {
        if (strcmp("1", message.c_str()) == 0)
        {
            syslogf("mqtt restart");
            enqueueMessage(topic, "0");
            preferences.putString(pref_reset_reason, "mqtt rest");
            shouldReboot = true;
            return;  // After restarting, no need to proceed further
        }
    }

    if (strcmp(topic, (update_token + "/" + String("set/Управление/update")).c_str()) == 0)
    {
        if (strcmp("1", message.c_str()) == 0)
        {
            syslogf("mqtt update");
            preferences.putInt("upd", 1);
            force_update = true;
            OtaStart = true;
            make_update();
        }
    }

    if (strcmp(topic, (update_token + "/" + String("set/Управление/preferences_clear")).c_str()) == 0)
    {
        if (strcmp("1", message.c_str()) == 0)
        {
            syslogf("mqtt preferences_clear");
            preferences.clear();
            config_preferences.clear();
            config_preferences.end();
            preferences.end();
            nvs_flash_erase_partition("nvs");  // Reformats
            nvs_flash_init_partition("nvs");   // Initializes
            syslogf("clear_pref");
            preferences.begin("settings", false);
            config_preferences.begin("config", false, "config");
            preferences.putString("ssid", ssid);
            preferences.putString("password", password);
            return;  // After restarting, no need to proceed further
        }
    }

    if (strcmp(topic, (update_token + "/" + String("set/get_calibrate")).c_str()) == 0)
    {
        syslogf("mqtt calibrate_now calibrate_now %d", calibrate_now);
        if (calibrate_now == 0)
        {
            calibrate_now = 1;
            syslogf("mqtt calibrate_now message.c_str() %s", message.c_str());

            if (!strcmp("temp-tab", message.c_str()))
            {
                NTC_void();  // Датчик температуры NTC
                DS18B20();   // Температурный сенсор Dallas
                publish_parameter("RootTemp", RootTemp, 3, 0);
                publish_parameter("AirTemp", AirTemp, 3, 0);
                publish_parameter("NTC_RAW", NTC_RAW, 3, 0);
                publish_parameter("wNTC", wNTC, 3, 0);
                publish_parameter("now", wNTC, 0, 0);

                syslogf("mqtt calibrate_now temp");
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
                publish_parameter("NTC_RAW", NTC_RAW, 3, 0);
                publish_parameter("Ap", Ap, 3, 0);
                publish_parameter("An", An, 3, 0);
                publish_parameter("wNTC", wNTC, 3, 0);
                publish_parameter("wEC", wEC, 3, 0);
                publish_parameter("wECnt", ec_notermo, 3, 0);
                publish_parameter("wR2", wR2, 3, 0);
                publish_parameter("now", millis(), 0, 0);
                syslogf("mqtt calibrate_now ec");
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
                publish_parameter("now", millis(), 0, 0);
                syslogf("mqtt MCP3421");
            }

            if (!strcmp("photoresistor-tab", message.c_str()))
            {
                PR_void();  // Фоторезистор
                publish_parameter("PR", PR, 3, 0);
                publish_parameter("wPR", wPR, 3, 0);
                publish_parameter("now", millis(), 0, 0);
                syslogf("mqtt PR_void");
            }

            if (!strcmp("bak-tab", message.c_str()))
            {
                TaskUS();  // pH метр
                publish_parameter("Dist", Dist, 3, 0);
                publish_parameter("DstRAW", DstRAW, 3, 0);
                publish_parameter("wLevel", wLevel, 3, 0);
                publish_parameter("now", millis(), 0, 0);
                syslogf("mqtt bak");
            }

            syslogf("mqtt get_calibrate");

            calibrate_now = 0;

            return;
        }
    }

    if (strcmp(topic, (update_token + "/" + String("set/preferences/all")).c_str()) == 0)
    {
        // Декодируем JSON из строки
        DeserializationError error = deserializeJson(jsonDoc, message.c_str());
        syslogf("JSON mqtt preferences/all");
        if (error)
        {
            syslogf("JSON decode failed: %s", String(error.c_str()));
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
                                syslogf("mqtt error save pref %s", String(preferencesArray[i].key));
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
                                syslogf("mqtt error save pref %s", String(preferencesArray[i].key));
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
                                syslogf("mqtt error save pref %s", String(preferencesArray[i].key));
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
                                syslogf("mqtt error save pref %s", String(preferencesArray[i].key));
                            }
                        }
                        break;
                }
            }
        }
    }

    unsigned long currentTime = millis();
    bool found_update = false;
    for (Group &group : groups)
    {
        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];
            String paramTopic = update_token + "/" + "set/" + String(group.caption) + "/" + String(param.name);
            if (paramTopic == topic)
            {
                found_update = true;
                syslogf("MQTT: Update  found %s %s", String(param.name), String(message));

                std::string paramKey(param.name);

                // Check if enough time has passed since last update
                if (paramLastUpdate.find(paramKey) == paramLastUpdate.end() ||
                    (currentTime - paramLastUpdate[paramKey]) >= PARAM_UPDATE_COOLDOWN)
                {
                    // Update the last update time
                    paramLastUpdate[paramKey] = currentTime;

                    if (updatePreference(param.name, message, "ha"))
                    {
                        syslogf("MQTT: updatePreference  %s %s", String(param.name), String(message));
                        return;
                    }
                }
                else
                {
                    syslogf("MQTT: Update ignored for %s, cooldown active. Wait %f s", String(param.name),
                            (PARAM_UPDATE_COOLDOWN - (currentTime - paramLastUpdate[paramKey])) / 1000.0);
                }
            }
        }
    }
    if (!found_update)
    {
        syslogf("MQTT: Update not found %s", String(message));
    }
}
void publish_params_all(int all = 1)
{
    publish_parameter("uptime", millis(), 0, 0);

    if (calE == 1)
    {
        syslogf("publich calibtate params");

        publish_parameter("RootTemp", RootTemp, 3, 0);
        publish_parameter("NTC_RAW", NTC_RAW, 3, 0);

        publish_parameter("pHmV", pHmV, 3, 0);
        publish_parameter("pHraw", pHraw, 3, 0);

        publish_parameter("Ap", Ap, 3, 0);
        publish_parameter("An", An, 3, 0);

        publish_parameter("DstRAW", DstRAW, 3, 0);

        publish_parameter("PumpA_SUM", PumpA_SUM, 3, 0);
        publish_parameter("PumpB_SUM", PumpB_SUM, 3, 0);
        publish_parameter("StepA_SUM", StepA_SUM, 3, 0);
        publish_parameter("StepB_SUM", StepB_SUM, 3, 0);
    }
}
