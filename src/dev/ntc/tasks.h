
void get_ntc()
{
    if (tR_type == "direct")
    {
        float r = 0;
        if (NTC_KAL_E == 1)
        {
            r = log((-NTC_Kalman + tR_DAC) / NTC_Kalman);
        }
        else
        {
            r = log((-NTC + tR_DAC) / NTC);
        }

        wNTC = (tR_B * 25 - r * 237.15 * 25 - r * pow(237.15, 2)) / (tR_B + r * 25 + r * 237.15) + tR_val_korr;
        syslog_ng("make_raschet  EC tR_B:" + fFTS(tR_B, 3) + " EC r:" + fFTS(r, 3) + " NTC " + fFTS(NTC, 3) +
                  " NTC_KAL_E " + fFTS(NTC_KAL_E, 0) + " NTC_Kalman " + fFTS(NTC_Kalman, 2) + " tR_DAC " +
                  fFTS(tR_DAC, 3) + " wNTC " + fFTS(wNTC, 3));
    }
    else
    {
        syslog_ng("make_raschet error tr type: " + String(tR_type));
    }
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
    syslog_ng("Frequency kHz:" + fFTS(NTC_Freq, 3) + " NTC_RAW " + fFTS(NTC_RAW, 3));
}

void NTC_void()
{
    unsigned long NTC_time = millis();
    syslog_ng("NTC Start " + fFTS(NTC_LastTime, 0) + "ms");

    get_acp();

    NTC_Kalman = KalmanNTC.filtered(NTC_RAW);
    syslog_ng("NTC NTC_RAW " + fFTS(NTC_RAW, 0));
    NTC_time = millis() - NTC_time;
    syslog_ng("NTC " + fFTS(NTC_RAW, 3) + "  " + fFTS(NTC_time, 0) + "ms end.");
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
        NTC = NTC_Kalman;
    }
    else
    {
        NTC = NTC_RAW;
    }
    get_ntc();

    publish_parameter("NTC", NTC, 3, 1);
    publish_parameter("NTC_RAW", NTC_RAW, 3, 1);
    publish_parameter("NTC_Kalman", NTC_Kalman, 3, 1);
    publish_parameter("wNTC", wNTC, 3, 1);
}
TaskParams NTCParams;