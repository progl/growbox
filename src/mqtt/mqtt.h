#include <mqtt/mqtt_common.h>
#include <mqtt/mqtt_ha.h>
#include <mqtt/mqtt_usual.h>

void Task60(void *parameters)
{
    for (;;)
    {
        unsigned long start_mqtt;
        start_mqtt = millis();
        syslog_ng("Task60 start mqtt Task60");
        while (OtaStart == true) vTaskDelay(1000);
        publish_params_all();
        syslog_ng("Task60 mqtt Task60 end " + fFTS((millis() - start_mqtt), 0) + " ms");
        events.send(String(update_token) + "/data-timescale/uptime:::" + String(millis()), "mqtt-ponics", millis());
        vTaskDelay(60000);
    }
}