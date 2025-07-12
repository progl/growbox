#include <mqtt/mqtt_common.h>
#include <mqtt/mqtt_ha.h>
#include <mqtt/mqtt_usual.h>
extern bool ram_saver_capture_current();
void Task60(void *parameters)
{
    unsigned long lastCommitTime = 0;
    int cycleCount = 0;

    for (;;)
    {
        unsigned long start_mqtt;
        start_mqtt = millis();
        syslog_ng("Task60 start mqtt Task60");
        while (OtaStart == true) vTaskDelay(1000);
        publish_params_all();
        syslog_ng("Task60 mqtt Task60 end " + fFTS((millis() - start_mqtt), 0) + " ms");
        if (events.count() > 0)
        {
            events.send(String(update_token + "/") + "/data-timescale/uptime:::" + String(millis()), "mqtt-ponics",
                        millis());
        }

        cycleCount++;
        unsigned long currentTime = millis();

        if (cycleCount % 10 == 0)
        {
            String logMsg = "RAM_SAVER: Cycle " + String(cycleCount) + ", uptime: " + String(currentTime / 1000) +
                            "s, heap: " + String(ESP.getFreeHeap());
            syslog_ng(logMsg);
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

        vTaskDelay(60000);
    }
}