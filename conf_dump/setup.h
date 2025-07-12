while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

Wire.requestFrom(static_cast<uint16_t>(CCS811addr), static_cast<uint8_t>(1));

if (Wire.available())
{
    CCS811Params = {"CCS811", CCS811, 30000, xSemaphore_C};
    ccs811.set_i2cdelay(50);
    bool ok = ccs811.begin();

    // Check if flashing should be executed
    if (ccs811.application_version() == 0x2000)
    {
        syslog_ng("CCS811: init... already has 2.0.0");
    }
    else
    {
        ok = ccs811.flash(image_data, sizeof(image_data));
        if (!ok) syslog_ng("CCS811: flash FAILED");
    }
    ccs811.start(CCS811_MODE_1SEC);

    addTask(TaskTemplate, "CCS811", stack_size, (void *)&CCS811Params, 1, NULL, CORE_SENSORS);

    setSensorDetected("CCS811", 1);
}
xSemaphoreGive(xSemaphore_C);