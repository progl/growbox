while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
  ;

Wire.begin(US_SDA, US_SCL);

Wire.requestFrom(static_cast<uint16_t>(0x29), static_cast<uint8_t>(1));

if (Wire.available())
{
  syslog_ng("I2C-US found: 0x29 laser distance sensor");
  if (s_VL53L0X.init())
  {

    s_VL53L0X.startContinuous();
    s_VL53L0X.setTimeout(5000);
    s_VL53L0X.setMeasurementTimingBudget(400000);

    TaskVL53L0XParams = {"TaskVL53L0X", TaskVL53L0X, 30000, xSemaphore_C};
    xTaskCreatePinnedToCore(TaskTemplate,
                            "TaskVL53L0X",
                            stack_size,
                            (void *)&TaskVL53L0XParams,
                            1,
                            NULL,
                            1);

    setSensorDetected("VL53L0X", 1);
  }
}

Wire.begin(I2C_SDA, I2C_SCL);
xSemaphoreGive(xSemaphore_C);