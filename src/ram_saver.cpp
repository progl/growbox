#include "ram_saver.h"
#include <cstring>

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

bool ram_saver_capture_current()
{
    if (!ramSaver.takeMutex(pdMS_TO_TICKS(100)))
    {
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

    ramSaver.giveMutex();
    return true;
}

void ram_saver_init()
{
    Serial.println("[RAM_SAVER] Initializing RAM saver with 200-record buffer...");

    // Initialize mutex
    if (!ramSaver.mutex)
    {
        ramSaver.mutex = xSemaphoreCreateMutex();
        if (!ramSaver.mutex)
        {
            Serial.println("[RAM_SAVER] ERROR: Failed to create mutex");
            return;
        }
    }

    // Initialize RAM buffer - protected by mutex
    if (ramSaver.takeMutex(pdMS_TO_TICKS(100)))
    {
        ramSaver.head = 0;
        ramSaver.count = 0;
        memset(ramSaver.records, 0, sizeof(ramSaver.records));
        memset(&ramSaver.currentMinute, 0, sizeof(ramSaver.currentMinute));
        ramSaver.giveMutex();
        Serial.println("[RAM_SAVER] RAM buffer initialized with 200 records");
    }
    else
    {
        Serial.println("[RAM_SAVER] ERROR: Could not initialize RAM buffer - mutex timeout");
        return;
    }

    Serial.println("[RAM_SAVER] Initialization complete");
}

void ram_saver_clear()
{
    if (ramSaver.takeMutex(pdMS_TO_TICKS(100)))
    {
        ramSaver.head = 0;
        ramSaver.count = 0;
        memset(ramSaver.records, 0, sizeof(ramSaver.records));
        ramSaver.giveMutex();
        Serial.println("[RAM_SAVER] RAM buffer cleared");
    }
}

// No-op function to maintain API compatibility
bool save_24h_record(const RamMultiRecord& record)
{
    // This is now a no-op as we're not saving to filesystem
    return true;
}

// Load records from the circular buffer
bool load_24h_records(RamMultiRecord* out, int max_count, int& count)
{
    if (!out || max_count <= 0)
    {
        count = 0;
        return false;
    }

    if (!ramSaver.takeMutex(pdMS_TO_TICKS(100)))
    {
        count = 0;
        return false;
    }

    int records_to_copy = (ramSaver.count < max_count) ? ramSaver.count : max_count;
    count = 0;  // Reset count to ensure we don't overflow the output buffer

    for (int i = 0; i < records_to_copy; i++)
    {
        int idx = (ramSaver.head - records_to_copy + i + RAM_SAVER_MAX_RECORDS) % RAM_SAVER_MAX_RECORDS;
        out[count++] = ramSaver.records[idx];
    }

    ramSaver.giveMutex();
    return true;
}

bool get_last_day_records(RamMultiRecord* out, int max_count, int& count)
{
    // For simplicity, just return the last hour records since we're not storing a full day
    int hour_count = 0;
    bool result = get_last_hour_records(out, hour_count);
    if (result)
    {
        count = (hour_count < max_count) ? hour_count : max_count;
    }
    else
    {
        count = 0;
    }
    return result;
}

bool get_last_hour_records(RamMultiRecord* out, int& count)
{
    if (!out)
    {
        count = 0;
        return false;
    }

    if (!ramSaver.takeMutex(pdMS_TO_TICKS(100)))
    {
        count = 0;
        return false;
    }

    count = ramSaver.count;
    if (count > 60) count = 60;  // Return at most 60 records (1 hour of data)

    // Copy records from circular buffer
    int start_idx = (ramSaver.head - count + RAM_SAVER_MAX_RECORDS) % RAM_SAVER_MAX_RECORDS;
    for (int i = 0; i < count; i++)
    {
        out[i] = ramSaver.records[(start_idx + i) % RAM_SAVER_MAX_RECORDS];
    }

    ramSaver.giveMutex();
    return true;
}

void ram_saver_commit_minute()
{
    if (!ramSaver.mutex)
    {
        return;
    }

    if (!ramSaver.takeMutex(pdMS_TO_TICKS(100)))
    {
        return;
    }

    // Capture current values
    ram_saver_capture_current();

    // Add to circular buffer
    ramSaver.records[ramSaver.head] = ramSaver.currentMinute;
    ramSaver.head = (ramSaver.head + 1) % RAM_SAVER_MAX_RECORDS;

    // Update count, but don't exceed max records
    if (ramSaver.count < RAM_SAVER_MAX_RECORDS)
    {
        ramSaver.count++;
    }

    // Clear current minute data
    memset(&ramSaver.currentMinute, 0, sizeof(ramSaver.currentMinute));
    ramSaver.giveMutex();
}

// Helper function to print memory stats
void print_memory_stats(const char* tag) {
    static uint32_t lastPrintTime = 0;
    uint32_t now = millis();
    
    // Only print every 30 seconds to avoid flooding the serial
    if (now - lastPrintTime > 30000) {
        lastPrintTime = now;
        
        // Get current task handle
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
        
        // Get stack high water mark for current task
        UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(currentTask);
        
        Serial.printf("[%s] Heap: %d free (min %d), Stack: %d free, Records: %d/%d\n",
                     tag,
                     ESP.getFreeHeap(),
                     ESP.getMinFreeHeap(),
                     stackHighWaterMark * 4,  // Convert to bytes
                     ramSaver.count,
                     RAM_SAVER_MAX_RECORDS);
    }
}

void ram_saver_task(void* param) {
    (void)param;
    unsigned long lastMinute = 0;
    
    // Print initial memory stats
    print_memory_stats("RAM_SAVER_INIT");
    
    // Wait for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    while (true) {
        unsigned long startTime = millis();
        
        // Check for system time wrap-around
        if (startTime < lastMinute) {
            lastMinute = 0;
        }
        
        // Check if a minute has passed
        if (startTime - lastMinute >= 60000) {  // 1 minute
            lastMinute = startTime;
            
            // Check for stack overflow
            if (uxTaskGetStackHighWaterMark(NULL) < 1024) {  // Less than 1KB stack remaining
                Serial.println("[RAM_SAVER] WARNING: Low stack space!");
            }
            
            // Commit the current minute's data
            ram_saver_commit_minute();
            
            // Print memory stats periodically
            print_memory_stats("RAM_SAVER");
        }
        
        // Calculate remaining time to sleep (1 minute total - processing time)
        unsigned long elapsed = millis() - startTime;
        if (elapsed < 59000) {  // Leave some margin for error
            vTaskDelay(pdMS_TO_TICKS(60000 - elapsed));
        } else {
            Serial.println("[RAM_SAVER] WARNING: Loop took too long!");
            vTaskDelay(pdMS_TO_TICKS(1000));  // If we're running late, don't delay too long
        }
        
        // Small delay to prevent watchdog triggers
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}