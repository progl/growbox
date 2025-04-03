void publishVariablesListToMQTT()
{
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    int arraySize =
        sizeof(preferencesArray) / sizeof(preferencesArray[0]);  // Определяем количество элементов в массиве

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
    }
}
void onMqttReconnectTimer(TimerHandle_t xTimer)
{
    connectToMqtt();  // Попытка повторного подключения к MQTT
}

void onMqttConnect(bool sessionPresent)
{
    syslog_ng("mqtt onMqttConnect mqtt connected start subscribe");
    subscribe();
    syslog_ng("mqtt onMqttConnect subscribed");
    if (first_time)
    {
        String statusTopic = mqttPrefix + "status";
        enqueueMessage(statusTopic.c_str(), "connected");
        syslog_ng("Before publish_setting_groups ");
        publish_setting_groups();
        syslog_ng("Before publishVariablesListToMQTT ");
        publishVariablesListToMQTT();
        first_time = false;
        syslog_ng("After publishVariablesListToMQTT");
    }
    syslog_ng("mqtt onMqttConnect end");
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index,
                   size_t total)
{
    String message;
    JsonDocument jsonDoc;
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
                publish_parameter("NTC_RAW", NTC_RAW, 3, 0);
                publish_parameter("wNTC", wNTC, 3, 0);
                publish_parameter("now", wNTC, 0, 0);

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
                publish_parameter("NTC_RAW", NTC_RAW, 3, 0);
                publish_parameter("Ap", Ap, 3, 0);
                publish_parameter("An", An, 3, 0);
                publish_parameter("wNTC", wNTC, 3, 0);
                publish_parameter("wEC", wEC, 3, 0);
                publish_parameter("wECnt", ec_notermo, 3, 0);
                publish_parameter("wR2", wR2, 3, 0);
                publish_parameter("now", wNTC, 0, 0);
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
                publish_parameter("now", wNTC, 0, 0);
                syslog_ng("mqtt MCP3421");
            }

            if (!strcmp("photoresistor-tab", message.c_str()))
            {
                PR_void();  // Фоторезистор
                publish_parameter("PR", PR, 3, 0);
                publish_parameter("wPR", wPR, 3, 0);
                publish_parameter("now", wNTC, 0, 0);
                syslog_ng("mqtt PR_void");
            }

            if (!strcmp("bak-tab", message.c_str()))
            {
                TaskUS();  // pH метр
                publish_parameter("Dist", Dist, 3, 0);
                publish_parameter("DstRAW", DstRAW, 3, 0);
                publish_parameter("wLevel", wLevel, 3, 0);
                publish_parameter("now", wNTC, 0, 0);
                syslog_ng("mqtt bak");
            }

            syslog_ng("mqtt get_calibrate");

            calibrate_now = 0;

            return;
        }
    }

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
            }
        }
    }

    for (Group &group : groups)
    {
        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];

            String paramTopicGroup = mqttPrefix + "set/" + String(group.caption) + "/" + String(param.name);
            String paramTopic = mqttPrefix + "set/" + String(param.name);
            if (String(topic).startsWith(mqttPrefix) &&
                (paramTopic == topic || paramTopicGroup == topic))  // Check if topic starts with prefix
            {
                PreferenceItem *item = findPreferenceByKey(param.name);
                // Topic corresponds to parameter, processing message
                switch (param.type)
                {
                    case Param::INT:
                        if (message.length() > 0)
                        {  // Add your own validation function isInteger

                            item->preferences->putInt(param.name, message.toInt());
                            syslog_ng("mqtt int param " + String(param.name) + " message " + String(message.toInt()));
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
                            syslog_ng("mqtt float param " + String(param.name) + " message " +
                                      String(message.toFloat()));
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
                            syslog_ng("mqtt string param " + String(param.name) + " message " + String(message));
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
void publish_params_all(int all = 1)
{
    if (calE == 1)
    {
        syslog_ng("publich calibtate params");

        publish_parameter("RootTemp", RootTemp, 3, 0);
        publish_parameter("NTC_RAW", NTC_RAW, 3, 0);

        publish_parameter("pHmV", pHmV, 3, 0);
        publish_parameter("pHraw", pHraw, 3, 0);

        publish_parameter("Ap", Ap, 3, 0);
        publish_parameter("An", An, 3, 0);

        publish_parameter("DstRAW", DstRAW, 3, 0);

        publish_parameter("PumpA_SUM", SetPumpA_Ml_SUM, 3, 0);
        publish_parameter("PumpB_SUM", SetPumpB_Ml_SUM, 3, 0);
        publish_parameter("StepA_SUM", PumpA_Step_SUM, 3, 0);
        publish_parameter("StepB_SUM", PumpB_Step_SUM, 3, 0);
    }
}
