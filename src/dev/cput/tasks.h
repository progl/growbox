void TaskCPUTEMP()
{
    unsigned long temp_probe_time = micros();

    // Инициализация работы с АЦП
    adc_power_acquire();  // Активируем питание АЦП
    SAR_ADC1_LOCK_ACQUIRE();
    delay(1);  // Стабилизация

    unsigned long raw_adc_sum = 0;

    // Считываем данные
    const int sample_count = 10000;  // Количество выборок для усреднения
    for (int i = 0; i < sample_count; i++)
    {
        if (__wega_adcStart(GPIO_NUM_34) == false)  // Канал температурного сенсора
        {
            SAR_ADC1_LOCK_RELEASE();
            adc_power_release();
            syslog_ng("CPUTemp: Ошибка при запуске АЦП");
            return;
        }
        raw_adc_sum += __wega_adcEnd(GPIO_NUM_34);  // Чтение данных
                                                    // Небольшая задержка между выборками
    }

    // Освобождаем АЦП
    SAR_ADC1_LOCK_RELEASE();
    adc_power_release();

    // Среднее значение
    float raw_adc_avg = float(raw_adc_sum) / sample_count;

    // Преобразование в напряжение и температуру
    float voltage = (raw_adc_avg / 4095.0) * 3.3;  // Преобразование в напряжение
    float CPUTemp = (voltage - 0.5) * 100;         // Преобразование напряжения в температуру
    if (CPUTemp < 0)
    {
        CPUTemp = -CPUTemp;
    }

    temp_probe_time = micros() - temp_probe_time;

    // Логирование
    syslog_ng("CPUTemp Raw ADC Avg: " + fFTS(raw_adc_avg, 3) + " Voltage: " + fFTS(voltage, 3) +
              " CPUTemp: " + fFTS(CPUTemp, 3) + "Probe Time micros: " + String(temp_probe_time));

    // Публикация результата
    publish_parameter("CPUTemp", CPUTemp, 3, 1);
}

TaskParams TaskCPUTEMPParams;
