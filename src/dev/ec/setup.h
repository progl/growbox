while (xSemaphoreTake(xSemaphore_C, (TickType_t)1) == pdFALSE);

digitalWrite(EC_DigitalPort1, HIGH);
digitalWrite(EC_DigitalPort2, LOW);

for (int i = 0; i < 1000; i++)
{
    if (adc1_get_raw(ADC1_CHANNEL_5) > 0)
    {
        EC_voidParams = {"EC", EC_void, 30000, xSemaphore_C};
        addTask(&EC_voidParams);
        setSensorDetected("EC", 1);
        break;
    }
    delay(1);
}

digitalWrite(EC_DigitalPort1, LOW);

xSemaphoreGive(xSemaphore_C);