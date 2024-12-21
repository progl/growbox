sens18b20.begin();
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
    ;

sensorCount = sens18b20.getDeviceCount();
for (int attempt = 0; attempt < 10; ++attempt)
{
    sensorCount = sens18b20.getDeviceCount();
    if (sensorCount != 0)
    {
        break;
    }
    sens18b20.begin();
    vTaskDelay(100);
}

if (sensorCount > 0)
{
    RootTempFound = true;
    sens18b20.setResolution(12);
    syslog_ng("DS18B20 Found 1-W devices:" + fFTS(sensorCount, 0));

    int DS18B20_TaskErr = xTaskCreatePinnedToCore(TaskTemplate, "DS18B20", 10000, (void *)&DS18B20Params, 1, NULL, 0);

    DS18B20Params = {"DS18B20", DS18B20, 30000, xSemaphore_C};
    setSensorDetected("Dallas", 1);
}
else
{
    RootTempFound = false;
}
xSemaphoreGive(xSemaphore_C);