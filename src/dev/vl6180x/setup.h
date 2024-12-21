while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
  ;

Wire.requestFrom(static_cast<uint16_t>(VL6180Xaddr), static_cast<uint8_t>(1));
if (Wire.available())
{

  s_vl6180X.init();
  s_vl6180X.configureDefault();
  s_vl6180X.setScaling(1);
  s_vl6180X.setTimeout(100);
  // s_vl6180X.stopContinuous();
  s_vl6180X.startRangeContinuous();
  s_vl6180X.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 20);
  s_vl6180X.writeReg16Bit(VL6180X::SYSALS__INTEGRATION_PERIOD, 50);
  TaskVL6180XParams = {"TaskVL6180X", TaskVL6180X, 30000, xSemaphore_C};
  xTaskCreatePinnedToCore(TaskTemplate,
                          "TaskVL6180X",
                          stack_size,
                          (void *)&TaskVL6180XParams,
                          1,
                          NULL,
                          0);

 

  setSensorDetected("VL6180X", 1);
}

xSemaphoreGive(xSemaphore_C);