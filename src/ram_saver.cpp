#include "ram_saver.h"
#include <cstring>
#include <Arduino.h>

// Forward declarations
extern void syslogf(const char* fmt, ...);
// Global variables from the project
extern float RootTemp;
extern float AirTemp;
extern float wNTC;
extern float wEC;
extern float wpH;
extern float wPR;
extern float wLevel;
extern float AirHum;
extern float AirVPD;
extern float PumpA_SUM;
extern float PumpB_SUM;

// Define the ramSaver instance
RamSaver ramSaver;

// ------------------ Внутренние утилиты ------------------
static inline bool _takeMutexOrFail(TickType_t ticks)
{
    // Если мьютекс не создан — сразу «не получилось»
    if (!ramSaver.mutex) return false;
    return ramSaver.takeMutex(ticks);
}

static inline void _giveMutexIfHeld()
{
    if (ramSaver.mutex) ramSaver.giveMutex();
}

// ------------------ Инициализация ------------------
void ram_saver_init()
{
    syslogf("RAM_SAVER: Initializing RAM saver...");

    // Initialize mutex
    if (!ramSaver.mutex)
    {
        ramSaver.mutex = xSemaphoreCreateRecursiveMutex();
        if (!ramSaver.mutex)
        {
            syslogf("RAM_SAVER: Failed to create mutex");
            return;
        }
        syslogf("RAM_SAVER: Recursive mutex created successfully");
    }
    else
    {
        syslogf("RAM_SAVER: Mutex already exists");
    }

    // Initialize RAM buffer
    if (_takeMutexOrFail(pdMS_TO_TICKS(100)))
    {
        ramSaver.head = 0;
        ramSaver.count = 0;
        memset(ramSaver.records, 0, sizeof(ramSaver.records));
        memset(&ramSaver.currentMinute, 0, sizeof(ramSaver.currentMinute));
        _giveMutexIfHeld();
        syslogf("RAM_SAVER: RAM buffer initialized with 200 records");
    }
    else
    {
        syslogf("RAM_SAVER: Could not initialize RAM buffer - mutex timeout");
        return;
    }

    syslogf("RAM_SAVER: Initialization complete");
}

// ------------------ Очистка ------------------
void ram_saver_clear()
{
    if (_takeMutexOrFail(pdMS_TO_TICKS(100)))
    {
        ramSaver.head = 0;
        ramSaver.count = 0;
        memset(ramSaver.records, 0, sizeof(ramSaver.records));
        _giveMutexIfHeld();
        Serial.println("[RAM_SAVER] RAM buffer cleared");
    }
}

// ------------------ Захват данных ------------------
bool ram_saver_capture_current()
{
    syslogf("RAM_SAVER: Capturing current data...");

    if (!_takeMutexOrFail(pdMS_TO_TICKS(1000)))
    {
        syslogf("RAM_SAVER: Could not take mutex in capture_current");
        return false;
    }

    ramSaver.currentMinute.timestamp = millis();
    ramSaver.currentMinute.RootTemp = RootTemp;
    ramSaver.currentMinute.AirTemp = AirTemp;
    ramSaver.currentMinute.wNTC = wNTC;
    ramSaver.currentMinute.wEC = wEC;
    ramSaver.currentMinute.wpH = wpH;
    ramSaver.currentMinute.wPR = wPR;
    ramSaver.currentMinute.wLevel = wLevel;
    ramSaver.currentMinute.AirHum = AirHum;
    ramSaver.currentMinute.AirVPD = AirVPD;
    ramSaver.currentMinute.PumpA_SUM = PumpA_SUM;
    ramSaver.currentMinute.PumpB_SUM = PumpB_SUM;

    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "RAM_SAVER: Data captured - Temp: %.1f°C, Hum: %.1f%%",
             ramSaver.currentMinute.AirTemp, ramSaver.currentMinute.AirHum);
    syslogf(logMsg);

    _giveMutexIfHeld();
    return true;
}

// ------------------ Commit записи ------------------
void ram_saver_commit_minute()
{
    if (!_takeMutexOrFail(pdMS_TO_TICKS(1000)))
    {
        syslogf("RAM_SAVER: Could not take mutex in commit_minute");
    }
    else
    {
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "RAM_SAVER: Storing record %d at position %d", ramSaver.count, ramSaver.head);
        syslogf(logMsg);

        ramSaver.records[ramSaver.head] = ramSaver.currentMinute;
        ramSaver.head = (ramSaver.head + 1) % RAM_SAVER_MAX_RECORDS;
        if (ramSaver.count < RAM_SAVER_MAX_RECORDS) ramSaver.count++;

        snprintf(logMsg, sizeof(logMsg), "RAM_SAVER: Record stored. Total records: %d", ramSaver.count);
        syslogf(logMsg);
        _giveMutexIfHeld();
    }

    // Сброс currentMinute вне мьютекса
    memset(&ramSaver.currentMinute, 0, sizeof(ramSaver.currentMinute));
}

// ------------------ Загрузка всех записей ------------------
bool load_24h_records(RamMultiRecord* out, int max_count, int& count)
{
    if (!out || max_count <= 0 || !_takeMutexOrFail(pdMS_TO_TICKS(100)))
    {
        count = 0;
        return false;
    }

    int to_copy = min(ramSaver.count, max_count);
    count = 0;
    int start = (ramSaver.head - to_copy + RAM_SAVER_MAX_RECORDS) % RAM_SAVER_MAX_RECORDS;
    for (int i = 0; i < to_copy; ++i) out[count++] = ramSaver.records[(start + i) % RAM_SAVER_MAX_RECORDS];

    _giveMutexIfHeld();
    return true;
}

// ------------------ Последний час ------------------
bool get_last_hour_records(RamMultiRecord* out, int& count)
{
    if (!out || !_takeMutexOrFail(pdMS_TO_TICKS(100)))
    {
        count = 0;
        return false;
    }

    int total = ramSaver.count;
    count = (total > 60 ? 60 : total);
    int start = (ramSaver.head - count + RAM_SAVER_MAX_RECORDS) % RAM_SAVER_MAX_RECORDS;
    for (int i = 0; i < count; ++i) out[i] = ramSaver.records[(start + i) % RAM_SAVER_MAX_RECORDS];

    _giveMutexIfHeld();
    return true;
}

// ------------------ Последние 24 часа (через часовые данные) ------------------
bool get_last_day_records(RamMultiRecord* out, int max_count, int& count)
{
    if (!out || !_takeMutexOrFail(pdMS_TO_TICKS(100)))
    {
        count = 0;
        return false;
    }

    int hour_count = 0;
    bool ok = get_last_hour_records(out, hour_count);
    count = ok ? min(hour_count, max_count) : 0;
    _giveMutexIfHeld();
    return ok;
}

// ------------------ Печать статистики памяти ------------------
void print_memory_stats(const char* tag)
{
    static uint32_t lastPrintTime = 0;
    uint32_t now = millis();
    if (now - lastPrintTime > 30000)
    {
        lastPrintTime = now;
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(currentTask);
        char logMsg[256];
        snprintf(logMsg, sizeof(logMsg), "%s: Heap: %d free (min %d), Stack: %d free, Records: %d/%d", tag,
                 ESP.getFreeHeap(), ESP.getMinFreeHeap(), stackHighWaterMark * 4, ramSaver.count,
                 RAM_SAVER_MAX_RECORDS);
        syslogf(logMsg);
    }
}
