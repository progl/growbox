#pragma once

#include <ESPAsyncWebServer.h>

// Прототип функции для настройки REST API RAM Saver
void setupRamSaverAPI();

// ============================
// File: ram_saver_api.cpp
// ============================
#include "ram_saver_api.h"
#include "ram_saver.h"
#include <ArduinoJson.h>
#include "freertos/semphr.h"
#include <LittleFS.h>

extern AsyncWebServer server;

void setupRamSaverAPI()
{
    // Статус
    server.on("/api/ram_saver/status", HTTP_GET,
              [](AsyncWebServerRequest* req)
              {
                  JsonDocument doc;
                  doc["isRunning"] = true;
                  doc["recordCount"] = RAM_SAVER_MAX_RECORDS;
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

    // Получить данные
    server.on("/api/ram_saver/data", HTTP_GET,
              [](AsyncWebServerRequest* req)
              {
                  JsonDocument doc;
                  JsonArray current = doc.createNestedArray("current");
                  JsonArray last_day = doc.createNestedArray("last_day");

                  // Основной буфер (последний час)
                  if (xSemaphoreTake(ramSaver.mutex, pdMS_TO_TICKS(10)))
                  {
                      for (int i = 0; i < RAM_SAVER_MAX_RECORDS; i++)
                      {
                          int idx = (ramSaver.head + i) % RAM_SAVER_MAX_RECORDS;
                          if (ramSaver.records[idx].timestamp > 0)
                          {
                              JsonObject rec = current.add<JsonObject>();
                              rec["timestamp"] = ramSaver.records[idx].timestamp;
                              rec["RootTemp"] = ramSaver.records[idx].RootTemp;
                              rec["AirTemp"] = ramSaver.records[idx].AirTemp;
                              rec["wNTC"] = ramSaver.records[idx].wNTC;
                              rec["wEC"] = ramSaver.records[idx].wEC;
                              rec["wpH"] = ramSaver.records[idx].wpH;
                              rec["wPR"] = ramSaver.records[idx].wPR;
                              rec["wLevel"] = ramSaver.records[idx].wLevel;
                              rec["AirHum"] = ramSaver.records[idx].AirHum;
                              rec["AirVPD"] = ramSaver.records[idx].AirVPD;
                              rec["PumpA_SUM"] = ramSaver.records[idx].PumpA_SUM;
                              rec["PumpB_SUM"] = ramSaver.records[idx].PumpB_SUM;
                          }
                      }
                      xSemaphoreGive(ramSaver.mutex);
                  }

                  // 24-часовые данные из флеш-памяти
                  const int MAX_24H_RECORDS = 288;  // 24 часа * 60 минут / 5 минут
                  RamMultiRecord dayRecords[MAX_24H_RECORDS];
                  int dayRecordCount = 0;

                  if (load_24h_records(dayRecords, MAX_24H_RECORDS, dayRecordCount))
                  {
                      for (int i = 0; i < dayRecordCount; i++)
                      {
                          if (dayRecords[i].timestamp > 0)
                          {
                              JsonObject rec = last_day.add<JsonObject>();
                              rec["timestamp"] = dayRecords[i].timestamp;
                              rec["RootTemp"] = dayRecords[i].RootTemp;
                              rec["AirTemp"] = dayRecords[i].AirTemp;
                              rec["wNTC"] = dayRecords[i].wNTC;
                              rec["wEC"] = dayRecords[i].wEC;
                              rec["wpH"] = dayRecords[i].wpH;
                              rec["wPR"] = dayRecords[i].wPR;
                              rec["wLevel"] = dayRecords[i].wLevel;
                              rec["AirHum"] = dayRecords[i].AirHum;
                              rec["AirVPD"] = dayRecords[i].AirVPD;
                              rec["PumpA_SUM"] = dayRecords[i].PumpA_SUM;
                              rec["PumpB_SUM"] = dayRecords[i].PumpB_SUM;
                          }
                      }
                  }

                  String resp;
                  serializeJson(doc, resp);
                  req->send(200, "application/json", resp);
              });
}
