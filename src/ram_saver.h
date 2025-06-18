#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define RAM_SAVER_MAX_RECORDS 200  // 200 records in memory (about 3.3 hours at 1 minute intervals)

struct RamMultiRecord
{
    unsigned long timestamp;
    float RootTemp;
    float AirTemp;
    float wNTC;
    float wEC;
    float wpH;
    float wPR;
    float wLevel;
    float AirHum;
    float AirVPD;
    float PumpA_SUM;
    float PumpB_SUM;
};

struct RamSaver
{
    RamMultiRecord records[RAM_SAVER_MAX_RECORDS];
    int head;                      // Index for the next record to write
    int count;                     // Current number of records stored
    SemaphoreHandle_t mutex;       // Mutex for thread safety
    RamMultiRecord currentMinute;  // Buffer for current minute

    // Thread-safe methods
    bool takeMutex(TickType_t timeout = portMAX_DELAY) { return xSemaphoreTake(mutex, timeout) == pdTRUE; }

    void giveMutex() { xSemaphoreGive(mutex); }
};

extern RamSaver ramSaver;

void ram_saver_init();
void ram_saver_clear();
void ram_saver_commit_minute();
bool get_last_day_records(RamMultiRecord* out, int max_count, int& count);
bool get_last_hour_records(RamMultiRecord* out, int& count);
bool load_24h_records(RamMultiRecord* out, int max_count, int& count);
void ram_saver_task(void* param);