while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);
Wire.requestFrom(static_cast<uint16_t>(0x76), static_cast<uint8_t>(1));

if (Wire.available())
{
    BMP280addr = 0x76;
    BMP280Params76 = {"BMP280", BMP280, 30000, xSemaphore_C};
    addTask(&BMP280Params76);

    setSensorDetected("BMx280", 1);
}
else
{
    Wire.requestFrom(static_cast<uint16_t>(0x77), static_cast<uint8_t>(1));

    if (Wire.available())
    {
        BMP280addr = 0x77;
        BMP280Params77 = {"BMP280", BMP280, 30000, xSemaphore_C};
        addTask(&BMP280Params77);
        setSensorDetected("BMx280", 1);
    }
}
xSemaphoreGive(xSemaphore_C);