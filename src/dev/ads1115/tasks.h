
void ADS1115_void()
{
    pHraw = adc.getRawResult();

    long cont = 0;
    double sensorValue = 0;
    while (cont < ADS1115_MiddleCount)
    {
        cont++;
        sensorValue = adc.getResult_mV() + sensorValue;
    }
    pHmV = sensorValue / cont;

    char buf[64];
    snprintf(buf, sizeof(buf), "ADS1115 pHmV:%.3f", pHmV);
    syslogf(buf);
}
TaskParams ADS1115Params;