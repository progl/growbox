#include <stdio.h>

#define USE_ESP32_VARIANT_ESP32 1
// Для ESP32
#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32)
// Нет официального API для оригинального ESP32
extern "C"
{
    uint8_t temprature_sens_read();
}
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C6) ||                                     \
    defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32H2) || \
    defined(USE_ESP32_VARIANT_ESP32C2)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/temp_sensor.h"
#else
#include "driver/temperature_sensor.h"
#endif
#endif
#endif

// Для RP2040
#ifdef USE_RP2040
#include "Arduino.h"
#endif

// Для BK72XX
#ifdef USE_BK72XX
extern "C"
{
    uint32_t temp_single_get_current_temperature(uint32_t *temp_value);
}
#endif

// Функция для считывания температуры
void TaskCPUTEMP()
{
    float CPUTemp = NAN;
    bool success = false;

#ifdef USE_ESP32
#if defined(USE_ESP32_VARIANT_ESP32)
    uint8_t raw = temprature_sens_read();
    CPUTemp = (raw - 32) / 1.8f;
    success = (raw != 128);
#elif defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C6) ||                                     \
    defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32H2) || \
    defined(USE_ESP32_VARIANT_ESP32C2)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    temp_sensor_config_t tsens = TSENS_CONFIG_DEFAULT();
    temp_sensor_set_config(tsens);
    temp_sensor_start();
    esp_err_t result = temp_sensor_read_celsius(&CPUTemp);
    temp_sensor_stop();
    success = (result == ESP_OK);
#else
    static temperature_sensor_handle_t tsensNew = NULL;
    esp_err_t result = temperature_sensor_get_celsius(tsensNew, &CPUTemp);
    success = (result == ESP_OK);
#endif
#endif
#endif

#ifdef USE_RP2040
    CPUTemp = analogReadTemp();
    success = (CPUTemp != 0.0f);
#endif

#ifdef USE_BK72XX
    uint32_t raw, result;
    result = temp_single_get_current_temperature(&raw);
    success = (result == 0);
#if defined(USE_LIBRETINY_VARIANT_BK7231N)
    CPUTemp = raw * -0.38f + 156.0f;
#else
    CPUTemp = raw / 100.0f;
#endif
#endif

    if (success)
    {
        syslog_ng("CPU Temperature:  " + fFTS(CPUTemp, 2));
        publish_parameter("CPUTemp", CPUTemp, 3, 1);
    }
    else
    {
        syslog_ng("Failed to read CPU temperature\n");
    }
}

TaskParams TaskCPUTEMPParams;
