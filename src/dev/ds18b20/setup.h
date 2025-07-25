syslogf("DS18B20 start setup");
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

syslogf("DS18B20 Found 1-W devices:%.2f", sensorCount);

if (sensorCount > 0)
{
    RootTempFound = true;
    DS18B20Params = {"DS18B20", DS18B20, 30000, xSemaphore_C};
    addTask(&DS18B20Params);

    setSensorDetected("Dallas", 1);

    DeviceAddress deviceAdrCompareRootTemp;
    DeviceAddress deviceAdrCompareNTC;

    stringToDeviceAddress(rootTempAddressString, deviceAdrCompareRootTemp);
    stringToDeviceAddress(WNTCAddressString, deviceAdrCompareNTC);

    for (uint8_t i = 0; i < sensorCount && i < MAX_DS18B20_SENSORS; i++)
    {
        DeviceAddress currentAddress;
        sens18b20.getAddress(currentAddress, i);

        memcpy(dallasAdresses[i].address, currentAddress, sizeof(DeviceAddress));

        float tempC = sens18b20.getTempC(currentAddress);

        syslogf("DS18B20 Sensor %d adr: %s Temp: %.2f", i, deviceAddressToString(currentAddress).c_str(), tempC);

        for (uint8_t j = 0; j < MAX_DS18B20_SENSORS; j++)  // Adjust MAX_SENSOR_ARRAY_SIZE as needed
        {
            if (sensorArray[j].key == "")
            {
                if (memcmp(deviceAdrCompareRootTemp, currentAddress, sizeof(DeviceAddress)) == 0)
                {
                    memcpy(sensorArray[j].address, currentAddress, sizeof(DeviceAddress));
                    sensorArray[j].key = "RootTemp";
                    syslogf("DS18B20 Sensor %d adr: %s User adr RootTemp: %s", i, deviceAddressToString(currentAddress),
                            deviceAddressToString(sensorArray[j].address));
                    break;
                }
                if (memcmp(deviceAdrCompareNTC, currentAddress, sizeof(DeviceAddress)) == 0)
                {
                    memcpy(sensorArray[j].address, currentAddress, sizeof(DeviceAddress));
                    sensorArray[j].key = "wNTC";
                    syslogf("DS18B20 Sensor %d adr: %s User adr wNTC: %s", i, deviceAddressToString(currentAddress),
                            deviceAddressToString(sensorArray[j].address));
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