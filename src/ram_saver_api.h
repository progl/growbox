#pragma once

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "freertos/semphr.h"
#include <LittleFS.h>
#include <ram_saver.h>

extern AsyncWebServer server;

// Вынесенный в функцию хендлер
static void handleRamSaverData(AsyncWebServerRequest* req)
{
    // Используем AsyncJsonResponse для «стриминга» JSON без громоздких буферов
    auto* response = new AsyncJsonResponse();
    JsonObject root = response->getRoot().to<JsonObject>();

    // status
    JsonObject status = root["status"].to<JsonObject>();
    status["isRunning"] = true;
    status["recordCount"] = ramSaver.count;

    // current
    JsonArray current = root["current"].to<JsonArray>();
    for (size_t i = 0; i < ramSaver.count; i++)
    {
        size_t idx = (ramSaver.head + RAM_SAVER_MAX_RECORDS - 1 - i) % RAM_SAVER_MAX_RECORDS;
        auto& r = ramSaver.records[idx];
        if (!r.timestamp) continue;
        JsonObject item = current.add<JsonObject>();
        item["timestamp"] = r.timestamp;
        item["RootTemp"] = r.RootTemp;
        item["AirTemp"] = r.AirTemp;
        item["wNTC"] = r.wNTC;
        item["wEC"] = r.wEC;
        item["wpH"] = r.wpH;
        item["wPR"] = r.wPR;
        item["wLevel"] = r.wLevel;
        item["AirHum"] = r.AirHum;
        item["AirVPD"] = r.AirVPD;
        item["PumpA_SUM"] = r.PumpA_SUM;
        item["PumpB_SUM"] = r.PumpB_SUM;
    }

    // last_day - load up to 24 most recent hourly records
    JsonArray lastDay = root["last_day"].to<JsonArray>();
    static RamMultiRecord dayBuf[24];
    int readCnt;

    // Load records just once
    if (load_24h_records(dayBuf, 24, readCnt) && readCnt > 0)
    {
        for (int i = 0; i < readCnt; i++)
        {
            const auto& d = dayBuf[i];
            if (!d.timestamp) continue;

            JsonObject item = lastDay.add<JsonObject>();
            item["timestamp"] = d.timestamp;
            item["RootTemp"] = d.RootTemp;
            item["AirTemp"] = d.AirTemp;
            item["wNTC"] = d.wNTC;
            item["wEC"] = d.wEC;
            item["wpH"] = d.wpH;
            item["wPR"] = d.wPR;
            item["wLevel"] = d.wLevel;
            item["AirHum"] = d.AirHum;
            item["AirVPD"] = d.AirVPD;
            item["PumpA_SUM"] = d.PumpA_SUM;
            item["PumpB_SUM"] = d.PumpB_SUM;
        }
    }

    // Библиотека сама посчитает длину и выполнит send
    response->setLength();
    req->send(response);
}

// Test endpoint to generate sample RAM saver data
static void handleTestRamSaverData(AsyncWebServerRequest* request)
{
    if (!request->hasParam("params"))
    {
        request->send(400, "text/plain", "Missing 'params' parameter with record count");
        return;
    }

    int recordCount = request->getParam("params")->value().toInt();
    if (recordCount <= 0 || recordCount > 1000)
    {
        request->send(400, "text/plain", "Invalid record count. Must be between 1 and 1000");
        return;
    }

    // Clear existing data
    ram_saver_clear();

    // Generate test data
    uint32_t currentTime = time(nullptr);
    bool success = true;

    for (int i = 0; i < recordCount; i++)
    {
        // Create a test record
        RamMultiRecord record = {0};  // Zero-initialize the struct

        // Fill with test data
        record.timestamp = currentTime - (recordCount - i) * 60;  // 1 minute intervals
        record.RootTemp = 20.0f + (i % 30) * 0.1f;
        record.AirTemp = 22.0f + (i % 20) * 0.15f;
        record.wNTC = 1000.0f + (i % 500);
        record.wEC = 0.5f + (i % 100) * 0.01f;
        record.wpH = 5.5f + (i % 50) * 0.02f;
        record.wPR = 100.0f + (i % 200);
        record.wLevel = 50.0f + (i % 100) * 0.5f;
        record.AirHum = 40.0f + (i % 60) * 0.3f;
        record.AirVPD = 1.0f + (i % 50) * 0.02f;
        record.PumpA_SUM = (i % 10) * 0.1f;
        record.PumpB_SUM = (i % 5) * 0.2f;

        // Add the record - we'll need to use the ramSaver instance directly
        if (!ramSaver.takeMutex(1000 / portTICK_PERIOD_MS))
        {
            success = false;
            break;
        }

        // Add the record to the circular buffer
        ramSaver.records[ramSaver.head] = record;
        ramSaver.head = (ramSaver.head + 1) % RAM_SAVER_MAX_RECORDS;
        if (ramSaver.count < RAM_SAVER_MAX_RECORDS)
        {
            ramSaver.count++;
        }
        ramSaver.giveMutex();
    }

    // Small delay to prevent watchdog issues
    delay(10);

    // Return response
    JsonDocument doc;
    doc["status"] = success ? "success" : "error";
    doc["message"] = success ? "Generated test data" : "Failed to generate test data";
    doc["recordCount"] = success ? recordCount : 0;

    String response;
    serializeJson(doc, response);
    request->send(success ? 200 : 500, "application/json", response);
}

void setupRamSaverAPI()
{
    // Статус
    server.on("/api/ram_saver/status", HTTP_GET,
              [](AsyncWebServerRequest* req)
              {
                  JsonDocument doc;
                  doc["isRunning"] = true;
                  String resp;
                  serializeJson(doc, resp);
                  req->send(200, "application/json", resp);
              });

    // Старт (очистка)
    server.on("/api/ram_saver/start", HTTP_POST,
              [](AsyncWebServerRequest* req)
              {
                  ram_saver_clear();
                  JsonDocument doc;
                  doc["status"] = "success";
                  doc["message"] = "RAM Saver started";
                  doc["recordCount"] = 0;
                  String resp;
                  serializeJson(doc, resp);
                  req->send(200, "application/json", resp);
              });

    // Стоп (фиктивно)
    server.on("/api/ram_saver/stop", HTTP_POST,
              [](AsyncWebServerRequest* req)
              {
                  JsonDocument doc;
                  doc["status"] = "success";
                  doc["message"] = "RAM Saver stopped";
                  doc["recordCount"] = RAM_SAVER_MAX_RECORDS;
                  String resp;
                  serializeJson(doc, resp);
                  req->send(200, "application/json", resp);
              });

    // Очистка данных
    server.on("/api/ram_saver/clear", HTTP_POST,
              [](AsyncWebServerRequest* req)
              {
                  ram_saver_clear();
                  JsonDocument doc;
                  doc["status"] = "success";
                  doc["message"] = "RAM Saver data cleared";
                  doc["recordCount"] = 0;
                  String resp;
                  serializeJson(doc, resp);
                  req->send(200, "application/json", resp);
              });

    server.on("/api/ram_saver/data", HTTP_GET, handleRamSaverData);
    server.on("/api/ram_saver/test", HTTP_GET, handleTestRamSaverData);
}
