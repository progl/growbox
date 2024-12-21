
while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE)
  ;

adc1_config_width(ADC_WIDTH_BIT_12);
adc1_config_channel_atten(PR_AnalogPort, ADC_ATTEN_DB_12);
PRParams = {"PR", PR_void, 30000, xSemaphore_C};
xTaskCreatePinnedToCore(TaskTemplate,
                        "PR",
                        stack_size,
                        (void *)&PRParams,
                        1,
                        NULL,
                        1);
 
setSensorDetected("PR", 1);
xSemaphoreGive(xSemaphore_C);