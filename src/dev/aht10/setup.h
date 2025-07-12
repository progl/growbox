while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(AHT10addr), static_cast<uint8_t>(1));
if (Wire.available())
{
    if (!myAHT10.begin())
    {
        syslog_err("AHTX0 not found");
        xSemaphoreGive(xSemaphore_C);
        return;
    }

    AHT10Params = {"AHT10", AHT10, 30000, xSemaphore_C};
    addTask(&AHT10Params);

    setSensorDetected("AHT10", 1);
}
xSemaphoreGive(xSemaphore_C);