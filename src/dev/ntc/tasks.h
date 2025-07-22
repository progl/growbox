
void get_ntc()
{
    float r = 0;
    if (NTC_KAL_E == 1)
    {
        r = log((-NTC_Kalman + tR_DAC) / NTC_Kalman);
    }
    else
    {
        r = log((-NTC_RAW + tR_DAC) / NTC_RAW);
    }

    wNTC = (tR_B * 25 - r * 237.15 * 25 - r * pow(237.15, 2)) / (tR_B + r * 25 + r * 237.15) + tR_val_korr;

    syslogf(
        "make_raschet get_ntc EC tR_B:%.3f EC r:%.3f NTC_RAW:%.3f NTC_KAL_E:%d NTC_Kalman:%.2f tR_DAC:%.3f wNTC:%.3f",
        tR_B, r, NTC_RAW, NTC_KAL_E, NTC_Kalman, tR_DAC, wNTC);
}
void get_acp()
{
    __wega_adcAttachPin(32);
    adc_power_acquire();
    vTaskDelay(10);
    unsigned long ntc_probe_time = micros();
    for (long i = 0; i < NTC_MiddleCount; i++)
    {
        __wega_adcStart(32);
        delayMicroseconds(ntc_daly_ms);
        NTC_RAW = NTC_RAW + float(__wega_adcEnd(32));
    }
    adc_power_release();
    ntc_probe_time = micros() - ntc_probe_time;
    float NTC_Freq = NTC_MiddleCount * 1000 / float(ntc_probe_time);

    NTC_RAW = NTC_RAW / NTC_MiddleCount;
    KalmanNTC.filtered(NTC_RAW);

    syslogf("Frequency kHz:%.3f NTC_RAW:%.3f", NTC_Freq, NTC_RAW);
}

void NTC_void()
{
    unsigned long NTC_time = millis();
    syslogf("NTC Start %dms", NTC_LastTime);

    get_acp();

    NTC_Kalman = KalmanNTC.filtered(NTC_RAW);

    syslogf("NTC NTC_RAW %.3f", NTC_RAW);

    NTC_time = millis() - NTC_time;

    syslogf("NTC %dms end.", NTC_time);
    NTC_old = millis();
    if (NTC_KAL_E == 1)
    {
        if (first_ntc)
        {
            first_ntc = false;
            int ntc_c = 0;
            while (ntc_c < 50)
            {
                ntc_c += 1;
                NTC_Kalman = KalmanNTC.filtered(NTC_RAW);
            }
        }
        NTC_RAW = NTC_Kalman;
    }

    get_ntc();

    publish_parameter("NTC_RAW", NTC_RAW, 3, 1);
    publish_parameter("NTC_Kalman", NTC_Kalman, 3, 1);
    publish_parameter("wNTC", wNTC, 3, 1);
}
TaskParams NTCParams;