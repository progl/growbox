#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>

// Внешние переменные
extern Preferences preferences;
extern String Firmware;
extern float wpH, wEC, wNTC, RootTemp, AirTemp;
extern String tg_bt;
extern String tg_cid;
extern bool tg_ph_en;
extern float tg_ph_min, tg_ph_max;
extern bool tg_ec_en;
extern float tg_ec_min, tg_ec_max;
extern bool tg_ntc_en;
extern float tg_ntc_min, tg_ntc_max;
extern bool tg_rt_en;
extern float tg_rt_min, tg_rt_max;
extern bool tg_at_en;
extern float tg_at_min, tg_at_max;

bool bot_connected = false;
WiFiClientSecure tgClient;

String getTimeString()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "--:--:--";
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return String(buf);
}

String escapeMarkdownV2(const String &text)
{
    String escaped;
    escaped.reserve(text.length() * 2);
    for (size_t i = 0; i < text.length(); i++)
    {
        char c = text.charAt(i);
        if (c == '_' || c == '*' || c == '[' || c == ']' || c == '(' || c == ')' || c == '~' || c == '`' || c == '>' ||
            c == '#' || c == '+' || c == '-' || c == '=' || c == '|' || c == '{' || c == '}' || c == '.' || c == '!' ||
            c == '\\')
        {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

struct SensorConfig
{
    const char *name;
    bool enabled;
    float minValue;
    float maxValue;
    bool alarmActive = false;
    bool alarmSent = false;
    bool okSent = false;
    String alarmStartTime = "";
    String alarmEndTime = "";
    String lastMsgId = "";
    String lastBaseText = "";
    unsigned long lastEdit = 0;
    int editCount = 0;  // 🆕 счётчик редактирований

    SensorConfig(const char *n, bool en, float mn, float mx) : name(n), enabled(en), minValue(mn), maxValue(mx) {}
};

SensorConfig tg_sensors[] = {
    SensorConfig("pH", true, tg_ph_min, tg_ph_max),
    SensorConfig("EC", true, tg_ec_min, tg_ec_max),
    SensorConfig("Темп. воды", true, tg_ntc_min, tg_ntc_max),
    SensorConfig("Темп. корней", true, tg_rt_min, tg_rt_max),
    SensorConfig("Темп. воздуха", true, tg_at_min, tg_at_max),
};

float getSensorValue(int i)
{
    switch (i)
    {
        case 0:
            return wpH;
        case 1:
            return wEC;
        case 2:
            return wNTC;
        case 3:
            return RootTemp;
        case 4:
            return AirTemp;
        default:
            return 0.0f;
    }
}

bool telegramApiRequest(const String &method, const String &params, String &response)
{
    if (tg_bt.isEmpty())
    {
        syslogf("TG API: Bot token not set");
        return false;
    }
    HTTPClient https;
    String url = "https://api.telegram.org/bot" + tg_bt + "/" + method;
    if (!https.begin(tgClient, url))
    {
        syslogf("TG API: Unable to connect");
        return false;
    }
    https.addHeader("Content-Type", "application/json");
    https.setTimeout(10000);
    int httpCode = https.POST(params);
    response = https.getString();
    https.end();
    if (httpCode != 200)
    {
        syslogf("TG API: Request failed, code: %d", httpCode);
        return false;
    }
    return true;
}

bool processMessageResponse(const String &response, String &outMsgId)
{
    StaticJsonDocument<512> doc;
    deserializeJson(doc, response);
    if (!doc.containsKey("result"))
    {
        syslogf("TG API: No 'result' in response");
        return false;
    }
    if (doc["result"].containsKey("message_id"))
    {
        outMsgId = String(doc["result"]["message_id"].as<int>());
        return true;
    }
    syslogf("TG API: message_id not found");
    return false;
}

bool sendMessage(const String &text, String &outMsgId)
{
    outMsgId = "";
    if (tg_cid.isEmpty() || text.isEmpty())
    {
        syslogf("TG send: Empty chat id or message");
        return false;
    }
    StaticJsonDocument<512> doc;
    doc["chat_id"] = tg_cid;
    doc["text"] = escapeMarkdownV2(HOSTNAME + ": " + text);
    doc["parse_mode"] = "MarkdownV2";
    String payload;
    serializeJson(doc, payload);
    String response;
    if (!telegramApiRequest("sendMessage", payload, response))
    {
        return false;
    }
    return processMessageResponse(response, outMsgId);
}

bool editMessageOrSendNew(SensorConfig &cfg, const String &text)
{
    if (tg_cid.isEmpty() || text.isEmpty())
    {
        syslogf("TG edit: Empty chat id or text");
        return false;
    }
    if (cfg.lastMsgId.isEmpty())
    {
        syslogf("TG edit: Sending new message");
        return sendMessage(text, cfg.lastMsgId);
    }
    StaticJsonDocument<512> doc;
    doc["chat_id"] = tg_cid;
    doc["message_id"] = cfg.lastMsgId.toInt();
    doc["text"] = escapeMarkdownV2(HOSTNAME + ": " + text);
    doc["parse_mode"] = "MarkdownV2";
    String payload;
    serializeJson(doc, payload);
    String response;
    if (!telegramApiRequest("editMessageText", payload, response))
    {
        StaticJsonDocument<512> errorDoc;
        if (deserializeJson(errorDoc, response))
        {
            if (errorDoc.containsKey("description"))
            {
                String desc = errorDoc["description"].as<String>();
                if (desc.indexOf("message to edit not found") >= 0 || desc.indexOf("message can't be edited") >= 0)
                {
                    syslogf("TG edit: Message not found, sending new");
                    return sendMessage(text, cfg.lastMsgId);
                }
            }
        }
        return false;
    }
    cfg.editCount++;  // 🆕 Увеличиваем счётчик
    return true;
}

void checkSensor(int i)
{
    SensorConfig &cfg = tg_sensors[i];
    if (!cfg.enabled)
    {
        syslogf("TG: Sensor %s monitoring disabled", cfg.name);
        return;
    }
    float value = getSensorValue(i);
    bool outOfRange = (value < cfg.minValue || value > cfg.maxValue);
    if (outOfRange)
    {
        cfg.okSent = false;
        if (!cfg.alarmActive)
        {
            cfg.alarmActive = true;
            cfg.alarmSent = false;
            cfg.alarmStartTime = getTimeString();
            cfg.alarmEndTime = "";
            cfg.editCount = 0;  // 🆕 сброс при новом аларме
            syslogf("TG: %s alarm started", cfg.name);
        }
        if (!cfg.alarmSent)
        {
            String base = "⚠️ *" + String(cfg.name) +
                          "* вне диапазона!\n"
                          "🕒 Начало: " +
                          cfg.alarmStartTime + "\n" + "Значение: " + String(value, 2) +
                          " (мин: " + String(cfg.minValue, 2) + ", макс: " + String(cfg.maxValue, 2) + ")";
            cfg.lastBaseText = base;
            if (sendMessage(base, cfg.lastMsgId))
            {
                cfg.alarmSent = true;
                cfg.lastEdit = millis();
                syslogf("TG: %s alarm sent", cfg.name);
            }
        }
        else if (cfg.lastMsgId.length() > 0 && millis() - cfg.lastEdit > 10000)
        {
            String newText =
                cfg.lastBaseText + "\n⏳ Проверка: " + getTimeString() + "\n✏️ Исправлений: " + String(cfg.editCount);
            if (editMessageOrSendNew(cfg, newText))
            {
                cfg.lastEdit = millis();
            }
        }
    }
    else if (cfg.alarmActive)
    {
        cfg.alarmEndTime = getTimeString();
        String duration = "🕒 Проблема: " + cfg.alarmStartTime + " - " + cfg.alarmEndTime;
        String okMsg = "✅ *" + String(cfg.name) + "* в норме\n" + duration + "\nЗначение: " + String(value, 2);
        String dummy;
        if (sendMessage(okMsg, dummy))
        {
            cfg.okSent = true;
            syslogf("TG: %s back to normal", cfg.name);
        }
        cfg.alarmActive = false;
        cfg.alarmSent = false;
    }
}

void checkAllSensors()
{
    if (tg_bt.isEmpty() || tg_cid.isEmpty() || !bot_connected)
    {
        syslogf("TG: Bot not ready");
        return;
    }
    for (int i = 0; i < (sizeof(tg_sensors) / sizeof(tg_sensors[0])); i++)
    {
        checkSensor(i);
        delay(200);
    }
}

void setupBot()
{
    if (tg_bt.isEmpty())
    {
        syslogf("TG: No bot token");
        return;
    }
    static bool sslClientInit = false;
    if (!sslClientInit)
    {
        tgClient.setInsecure();
        tgClient.setTimeout(15000);
        sslClientInit = true;
    }
    String dummy;
    if (sendMessage("🔔 GrowBox запущен! FW: " + Firmware, dummy))
    {
        bot_connected = true;
        syslogf("TG: Bot connected");
    }
}

void applyTelegramSettings()
{
    tg_sensors[0] = SensorConfig("pH", tg_ph_en, tg_ph_min, tg_ph_max);
    tg_sensors[1] = SensorConfig("EC", tg_ec_en, tg_ec_min, tg_ec_max);
    tg_sensors[2] = SensorConfig("Темп. воды", tg_ntc_en, tg_ntc_min, tg_ntc_max);
    tg_sensors[3] = SensorConfig("Темп. корней", tg_rt_en, tg_rt_min, tg_rt_max);
    tg_sensors[4] = SensorConfig("Темп. воздуха", tg_at_en, tg_at_min, tg_at_max);
    syslogf("TG: Settings applied");
}

void testTelegram()
{
    syslogf("[TG] Starting test...");
    String msgId1, msgId2;
    // Тест 1
    syslogf("[TG test] Test 1: Send and edit message");
    bool sent1 = sendMessage("Тестовое сообщение 1", msgId1);
    if (!sent1)
    {
        syslogf("[TG test] ERROR: Failed to send message 1");
        return;
    }
    delay(1000);
    tg_sensors[0].lastMsgId = msgId1;  // временно, чтобы проверить editCount
    editMessageOrSendNew(tg_sensors[0], "Тестовое сообщение 1 - отредактировано в " + getTimeString());
    // Тест 2
    bool sent2 = sendMessage("Тестовое сообщение 2", msgId2);
    if (!sent2)
    {
        syslogf("[TG test] ERROR: Failed to send message 2");
        return;
    }
    delay(3000);
    tg_sensors[0].lastMsgId = msgId2;
    editMessageOrSendNew(tg_sensors[0], "Тестовое сообщение 2 - отредактировано в " + getTimeString());
    syslogf("[TG test] Test completed");
}
