#include <mqtt/mqtt_common.h>
#include <mqtt/mqtt_ha.h>
#include <mqtt/mqtt_usual.h>
extern bool ram_saver_capture_current();
extern void checkAllSensors();
void Task60(void *parameters)
{
    unsigned long lastCommitTime = 0;
    int cycleCount = 0;

    for (;;)
    {
        unsigned long start_mqtt;
        start_mqtt = millis();
        syslogf("Task60 start mqtt Task60");
        while (OtaStart == true) vTaskDelay(1000);
        publish_params_all();
        char durationStr[32];
        if (fFTS((millis() - start_mqtt), 0, durationStr, sizeof(durationStr)))
        {
            syslogf("Task60 mqtt Task60 end %s", durationStr);
        }
        if (events.count() > 0)
        {
            events.send(String(update_token + "/") + "/data-timescale/uptime:::" + String(millis()), "mqtt-ponics",
                        millis());
        }

        cycleCount++;
        unsigned long currentTime = millis();

        if (cycleCount % 10 == 0)
        {
            syslogf("RAM_SAVER: Cycle %d, uptime: %lu s, heap: %u", cycleCount, currentTime / 1000, ESP.getFreeHeap());
        }

        if (currentTime - lastCommitTime >= 60000)  // Every minute
        {
            // Capture current sensor data before committing
            if (ram_saver_capture_current())
            {
                ram_saver_commit_minute();
                lastCommitTime = currentTime;
            }
            else
            {
                syslog_err("RAM_SAVER: Failed to capture current data, skipping commit");
            }
        }
        checkAllSensors();
        vTaskDelay(60000);
    }
}