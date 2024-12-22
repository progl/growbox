// Определяем ошибку для АЦП

void EC_void()
{
    unsigned long EC_time = millis();
    unsigned long ec_probe_time = micros();

    // Начало работы с АЦП
    adc_power_acquire();  // Без проверки на SUCCESS, т.к. функция ничего не возвращает
    SAR_ADC1_LOCK_ACQUIRE();
    delay(1);  // Подождите 1 мс для стабилизации

    pinMode(EC_DigitalPort1, OUTPUT);
    pinMode(EC_DigitalPort2, OUTPUT);

    // Инициализация для измерений
    digitalWrite(EC_DigitalPort1, LOW);
    digitalWrite(EC_DigitalPort2, LOW);

    unsigned long An0 = 0;
    unsigned long Ap0 = 0;

    // Измерения
    for (long i = 0; i < EC_MiddleCount && !OtaStart; i++)
    {
        digitalWrite(EC_DigitalPort1, HIGH);
        delayMicroseconds(10);
        // Периодический запуск и завершение ADC
        if (__wega_adcStart(EC_AnalogPort) == false)
        {
            SAR_ADC1_LOCK_RELEASE();
            adc_power_release();
            syslog_ng("Ошибка при запуске АЦП");
            return;
        }
        Ap0 += __wega_adcEnd(EC_AnalogPort);

        digitalWrite(EC_DigitalPort1, LOW);
        digitalWrite(EC_DigitalPort2, HIGH);
        delayMicroseconds(10);

        if (__wega_adcStart(EC_AnalogPort) == false)
        {
            syslog_ng("Ошибка при запуске АЦП");
            SAR_ADC1_LOCK_RELEASE();
            adc_power_release();
            return;
        }
        An0 += __wega_adcEnd(EC_AnalogPort);
        digitalWrite(EC_DigitalPort2, LOW);
    }

    // Завершаем измерения
    ec_probe_time = micros() - ec_probe_time;
    pinMode(EC_DigitalPort1, INPUT);
    pinMode(EC_DigitalPort2, INPUT);

    // Освобождаем АЦП
    SAR_ADC1_LOCK_RELEASE();
    adc_power_release();

    // Средние значения
    float Mid_Ap0 = float(Ap0) / EC_MiddleCount;
    float Mid_An0 = float(An0) / EC_MiddleCount;

    // Применение фильтра
    if (EC_KAL_E == 1)
    {
        ApRM_AVG.add(Mid_Ap0);
        AnRM_AVG.add(Mid_An0);
        Ap = ApRM_AVG.getAverage();
        An = AnRM_AVG.getAverage();
    }
    else
    {
        An = Mid_An0;
        Ap = Mid_Ap0;
    }

    // Частота измерений
    float EC_Freq = EC_MiddleCount * 1000 / float(ec_probe_time);

    // Время работы
    EC_time = millis() - EC_time;
    EC_old = millis();

    // Получаем EC
    get_ec();

    // Логирование
    syslog_ng("EC Ap:" + fFTS(Ap, 3) + " An:" + fFTS(An, 3) + " probe time micros:" + String(ec_probe_time) +
              " probe count:" + String(EC_MiddleCount) + " Frequency kHz:" + fFTS(EC_Freq, 3) + " wR2:" + fFTS(wR2, 3) +
              " wNTC:" + fFTS(wNTC, 3) + " wEC:" + fFTS(wEC, 3) + " " + fFTS(EC_time, 0) + "ms end.");

    // Публикация данных
    publish_parameter("EC_Kalman", EC_Kalman, 3, 1);
    publish_parameter("wECnt", ec_notermo, 3, 1);
    publish_parameter("wR2", wR2, 3, 1);
    publish_parameter("wEC", wEC, 3, 1);
    publish_parameter("Ap", Ap, 3, 1);
    publish_parameter("An", An, 3, 1);
}

TaskParams EC_voidParams;