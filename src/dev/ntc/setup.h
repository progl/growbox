while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

adc1_config_width(ADC_WIDTH_BIT_12);
adc1_config_channel_atten(NTC_port, ADC_ATTEN_DB_12);

if (adc1_get_raw(NTC_port) > 0)
{
    NTCParams = {"NTC", NTC_void, 30000, xSemaphore_C};
    addTask(&NTCParams);
    setSensorDetected("NTC", 1);
}
xSemaphoreGive(xSemaphore_C);