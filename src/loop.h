#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declaration
extern bool shouldReboot;

void loop()
{
    if (isAPMode)
    {
        dnsServer.processNextRequest();  // для captive portal
    }
    // Check if reboot is requested
    if (shouldReboot)
    {
        // Delay to allow HTTP response to be sent
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP.restart();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
