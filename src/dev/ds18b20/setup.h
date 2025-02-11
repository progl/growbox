sens18b20.begin();
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

sensorCount = sens18b20.getDeviceCount();
sens18b20.requestTemperatures();
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

// sens18b20.requestTemperatures();
syslog_ng("DS18B20 Found 1-W devices:" + fFTS(sensorCount, 0));
if (sensorCount > 0)
{
    RootTempFound = true;

    int DS18B20_TaskErr = xTaskCreatePinnedToCore(TaskTemplate, "DS18B20", 10000, (void *)&DS18B20Params, 1, NULL, 0);

    DS18B20Params = {"DS18B20", DS18B20, 30000, xSemaphore_C};
    setSensorDetected("Dallas", 1);

    DeviceAddress deviceAdrCompareRootTemp;
    DeviceAddress deviceAdrCompareNTC;

    stringToDeviceAddress(rootTempAddressString, deviceAdrCompareRootTemp);
    stringToDeviceAddress(WNTCAddressString, deviceAdrCompareNTC);

    for (uint8_t i = 0; i < sensorCount && i < MAX_DS18B20_SENSORS; i++)
    {
        DeviceAddress currentAddress;
        sens18b20.getAddress(currentAddress, i);
        syslog_ng("DS18B20 Sensor " + String(i) + " adr: " + deviceAddressToString(currentAddress) +
                  " Temp: " + fFTS(sens18b20.getTempC(currentAddress), 3));
        for (uint8_t j = 0; j < MAX_DS18B20_SENSORS; j++)  // Adjust MAX_SENSOR_ARRAY_SIZE as needed
        {
            if (sensorArray[j].key == "")
            {
                if (memcmp(deviceAdrCompareRootTemp, currentAddress, sizeof(DeviceAddress)) == 0)
                {
                    memcpy(sensorArray[j].address, currentAddress, sizeof(DeviceAddress));
                    sensorArray[j].key = "RootTemp";
                    syslog_ng("DS18B20 Sensor " + String(i) + " adr: " + deviceAddressToString(currentAddress) +
                              " User adr RootTemp: " + deviceAddressToString(sensorArray[j].address));
                    break;
                }
                if (memcmp(deviceAdrCompareNTC, currentAddress, sizeof(DeviceAddress)) == 0)
                {
                    memcpy(sensorArray[j].address, currentAddress, sizeof(DeviceAddress));
                    sensorArray[j].key = "wNTC";
                    syslog_ng("DS18B20 Sensor " + String(i) + " adr: " + deviceAddressToString(currentAddress) +
                              " User adr wNTC: " + deviceAddressToString(sensorArray[j].address));
                    break;
                }
            }
        }
    }
}
else
{
    RootTempFound = false;
}
xSemaphoreGive(xSemaphore_C);