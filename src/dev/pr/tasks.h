
float pr(float my_arg)
{
    if (isnan(my_arg) || my_arg == 0.0) return 0.0f;  // Check if my_arg is not a number or zero

    float calc_pr = 0.0;

    if (pR_type == "direct")
    {
        calc_pr = -pR1 * (my_arg - pR_DAC) / my_arg;
    }
    else if (pR_type == "reverse")
    {
        if (my_arg > pR_DAC) return NAN;  // Return NaN for invalid range in reverse mode
        calc_pr = -my_arg * pR1 / (my_arg - pR_DAC);
    }

    return calc_pr;
}

float fpr(float my_arg)
{
    // Precompute values used in the quadratic interpolation
    float ppx1 = pR_raw_p1;
    float ppx2 = pR_raw_p2;
    float ppx3 = pR_raw_p3;
    float ppy1 = pR_val_p1;
    float ppy2 = pR_val_p2;
    float ppy3 = pR_val_p3;

    float ppx1_sq = pow(ppx1, 2);
    float ppx2_sq = pow(ppx2, 2);
    float ppx3_sq = pow(ppx3, 2);

    float denominator = (ppx2 * ppx3 - ppx2 * ppx1 + ppx1_sq - ppx3 * ppx1);
    float denominator_inv = 1.0 / denominator;

    float ppa = -(-ppx1 * ppy3 + ppx1 * ppy2 - ppx3 * ppy2 + ppy3 * ppx2 + ppy1 * ppx3 - ppy1 * ppx2) *
                denominator_inv /
                (ppx1_sq * ppx3 - ppx1_sq * ppx2 + ppx1 * ppx2_sq - ppx1 * ppx3_sq + ppx3_sq * ppx2 - ppx3 * ppx2_sq);
    float ppb = (ppy3 * ppx2_sq - ppx2_sq * ppy1 + ppx3_sq * ppy1 + ppy2 * ppx1_sq - ppy3 * ppx1_sq - ppy2 * ppx3_sq) *
                denominator_inv;
    float ppc = (ppy3 * ppx1_sq * ppx2 - ppy2 * ppx1_sq * ppx3 - ppx2_sq * ppx1 * ppy3 + ppx3_sq * ppx1 * ppy2 +
                 ppx2_sq * ppy1 * ppx3 - ppx3_sq * ppy1 * ppx2) *
                denominator_inv;

    if (pR_type == "other")
    {
        return ppa * pow(my_arg, 2) + ppb * my_arg + ppc;
    }

    float R = pr(my_arg);
    syslogf("PR: fpr R %s", fFTS(R, 3));

    if (pR_type == "direct" || pR_type == "reverse")
    {
        if (R == 0) return R;
        return exp(log(pR_Rx / R) / pR_T) * pR_x;
    }

    if (pR_type == "digital")
    {
        return my_arg;
    }

    return 0;  // Return 0 as a default case instead of None
}

void PR_void()
{
    unsigned long PR_time = millis();
    unsigned long pr_probe_time = micros();
    unsigned long PR0 = 0;
    syslogf("PR: %sms start.", fFTS(pr_probe_time, 0));
    // Начало работы с АЦП
    adc_power_acquire();  // Без проверки на SUCCESS, т.к. функция ничего не
                          // возвращает
    SAR_ADC1_LOCK_ACQUIRE();
    delay(1);  // Подождите 1 мс для стабилизации

    // Измерения
    for (long cont = 0; cont < PR_MiddleCount && !OtaStart; cont++)
    {
        if (__wega_adcStart(PR_AnalogPort) == false)
        {
            syslogf("PR: Ошибка при запуске АЦП %sms", fFTS(cont, 0));
            SAR_ADC1_LOCK_RELEASE();
            adc_power_release();
            return;
        }
        PR0 += __wega_adcEnd(PR_AnalogPort);
    }
    // Освобождаем АЦП
    SAR_ADC1_LOCK_RELEASE();
    adc_power_release();
    // Завершаем измерения
    pr_probe_time = micros() - pr_probe_time;
    syslogf("PR: %sms", fFTS(PR, 3));

    PR_time = millis() - PR_time;
    float Mid_PR = float(PR0) / PR_MiddleCount;

    PR = Mid_PR;
    wPR = fpr(PR);

    // Частота измерений
    float PR_Freq = PR_MiddleCount * 1000 / float(pr_probe_time);

    // Время работы

    // Логирование
    syslogf("PR: %sms wPR %sms probe time micros:%sms probe count:%s Frequency kHz:%sms %sms end.", fFTS(PR, 3),
            fFTS(wPR, 3), fFTS(pr_probe_time, 0), String(PR_MiddleCount), fFTS(PR_Freq, 3), fFTS(PR_time, 0));

    // Публикация данных

    publish_parameter("wPR", wPR, 3, 1);
}

TaskParams PRParams;