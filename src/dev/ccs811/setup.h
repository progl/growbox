while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(CCS811addr), static_cast<uint8_t>(1));

if (Wire.available())
{
    CCS811Params = {"CCS811", CCS811, 30000, xSemaphore_C};
    ccs811.set_i2cdelay(50);
    bool ok = ccs811.begin();

    ccs811.start(CCS811_MODE_1SEC);

    addTask(&CCS811Params);

    setSensorDetected("CCS811", 1);
}
xSemaphoreGive(xSemaphore_C);