String get_client_id(bool ha = false)
{
    String cId;
    uint64_t chipid = ESP.getEfuseMac();

    // Convert chip ID to hex string
    chipStr = String(chipid, HEX);

    if (ha)
    {
        cId = "ha-" + chipStr;
    }
    else
    {
        cId = "esp32-" + chipStr;
    }

    // Replace any remaining non-alphanumeric characters with hyphens
    for (int i = 0; i < cId.length(); i++)
    {
        if (!isalnum(cId[i]) && cId[i] != '-')
        {
            cId.setCharAt(i, '-');
        }
    }

    // Remove consecutive hyphens
    while (cId.indexOf("--") != -1)
    {
        cId.replace("--", "-");
    }

    // Remove leading/trailing hyphens
    cId.trim();
    while (cId.endsWith("-")) cId.remove(cId.length() - 1);
    while (cId.startsWith("-")) cId.remove(0, 1);

    return cId;
}
void publish_one_data(const PreferenceItem *item, String mqtt_type = "all")
{
    String topic = update_token + "/" + preferences_prefix + item->key;  // Use appropriate topic prefix
    String valueStr;
    if (item->variable != nullptr)
    {
        syslog_ng("mqtt before publish_one_data topic: " + String(item->key));
        // Определите тип переменной и получите ее значение в зависимости от типа
        switch (item->type)
        {
            case DataType::FLOAT:
                valueStr = String(*(float *)item->variable);
                break;
            case DataType::STRING:
                valueStr = *(String *)item->variable;
                break;
            case DataType::BOOLEAN:
                valueStr = (*(bool *)item->variable) ? "true" : "false";
                break;
            case DataType::INTEGER:
                valueStr = String(*(int *)item->variable);
                break;
        }

        // Опубликуйте значение в MQTT
        syslog_ng("mqtt publish_one_data topic: " + topic + " value: " + valueStr);
        enqueueMessage(topic.c_str(), valueStr.c_str(), "", mqtt_type);
    }
    syslog_ng("mqtt after publish_one_data topic: " + String(item->key));
}
