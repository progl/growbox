while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);
if (adc1_get_raw(NTC_port) > 0)
{
  EC_voidParams = {"EC", EC_void, 30000, xSemaphore_C};
  xTaskCreatePinnedToCore(TaskTemplate, "EC", stack_size, (void *)&EC_voidParams, 1, NULL, 1);
  setSensorDetected("EC", 1);
}
xSemaphoreGive(xSemaphore_C);