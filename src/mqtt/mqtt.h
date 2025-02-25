#include <mqtt/mqtt_ha.h>
#include <mqtt/mqtt_usual.h>

void TaskMqttForCal(void *parameters)
{
    for (;;)
    {
        unsigned long start_mqtt;
        start_mqtt = millis();
        syslog_ng("start mqtt TaskMqttForCal");
        while (OtaStart == true) vTaskDelay(1000);
        publish_params_all();
        syslog_ng("mqtt TaskMqttForCal end " + fFTS((millis() - start_mqtt), 0) + " ms");
        vTaskDelay(DEFAULT_DELAY);
    }
}