
// Создание JSON-объекта
StaticJsonDocument<6144> docGroups;
StaticJsonDocument<1024> docApiTasks;
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

void handleReset(AsyncWebServerRequest *request)
{
    syslogf("WEB /reset");

    // Отправляем ответ клиенту
    request->send(200, "text/plain", "restart");

    // Сохраняем причину сброса
    preferences.putString(pref_reset_reason, "url");

    // Даём время на отправку ответа, затем перезагружаем

    // через 100 мс перезапустим
    restartTicker.once_ms(1000, []() { shouldReboot = true; });
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

void handleApiTasks(AsyncWebServerRequest *request)
{
    docApiTasks.clear();
    JsonArray tasksArray = docApiTasks.createNestedArray("tasks_status");

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
        }
    }

    String output;
    serializeJson(docApiTasks, output);
    request->send(200, "application/json", sanitizeString(output));
}

String ApiGroups(bool labels = false)
{
    // Проверка и инициализация списков не обнаруженных и обнаруженных датчиков

    syslogf("WEB /groups labels %d", labels);
    unsigned long t_millis = millis();
    docGroups.clear();

    // Добавление групп настроек
    JsonObject groupsJson = docGroups["groups"].to<JsonObject>();

    for (Group &group : groups)
    {
        JsonObject groupJson = groupsJson[group.caption].to<JsonObject>();

        for (int i = 0; i < group.numParams; i++)
        {
            Param &param = group.params[i];
            if (param.name == nullptr)
            {
                continue;
            }
            PreferenceItem *item = findPreferenceByKey(param.name);

            if (item != nullptr && item->preferences != nullptr)
            {
                if (labels)
                {
                    docGroups[String(param.name) + "_label"] = param.label;
                }
                else
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
            }
            else
            {
                syslogf("ERROR: Could not find preferences for %s", param.name);
            }
        }
    }

    JsonArray sensorsArray = docGroups["sensors"].to<JsonArray>();
    for (int i = 0; i < sensorCount; i++)
    {
        JsonObject sensor = sensorsArray.add<JsonObject>();
        sensor["name"] = "dallas_" + String(i);
        sensor["address"] = dallasAdresses[i].addressToString();
    }

    // Сериализация и отправка JSON
    String output;
    serializeJson(docGroups, output);

    Serial.print("Serialized JSON length: ");
    Serial.println(output.length());
    return output;
}
