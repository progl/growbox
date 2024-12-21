
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
  ;

Wire.requestFrom(static_cast<uint16_t>(ADS1115addr), static_cast<uint8_t>(1));
if (Wire.available())
{
  adc.reset();
  delay(100);
  adc.init();
  adc.setMeasureMode(ADS1115_CONTINUOUS);
  adc.setCompareChannels(ADS1115_COMP_0_3);
  adc.setVoltageRange_mV(ADS1115_RANGE_4096);
  adc.setConvRate(ADS1115_8_SPS);

  ADS1115Params = {"ADS1115", ADS1115_void, 30000, xSemaphore_C};
  xTaskCreatePinnedToCore(TaskTemplate,
                          "ADS1115",
                          stack_size,
                          (void *)&ADS1115Params,
                          1,
                          NULL,
                          1);

  setSensorDetected("ADS1115", 1);
}
xSemaphoreGive(xSemaphore_C);