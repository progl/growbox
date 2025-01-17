void TaskUS()
{
    static const int MAX_ERRORS = 5;
    static int errorCount = 0;
    static const float MIN_VALID_DISTANCE = 2.0f;    // минимальное валидное расстояние в см
    static const float MAX_VALID_DISTANCE = 400.0f;  // максимальное валидное расстояние в см
    static uint32_t last_successful_measurement = 0;  // время последнего успешного измерения
    
    // Обновляем температуру, если она доступна
    if (AirTemp)
    {
        distanceSensor.setTemperature(AirTemp);
        syslog_ng("US025: AirTemp " + fFTS(AirTemp, 3));
    }

    // Первое измерение
    distanceSensor.triggerPulse();
    if (!distanceSensor.waitForMeasurement())
    {
        errorCount++;
        syslog_ng("US025: Measurement timeout, error count: " + String(errorCount));
        if (errorCount >= MAX_ERRORS)
        {
            syslog_ng("US025: Too many errors, sensor might be disconnected");
            setSensorDetected("US025", 0);
            return;
        }
        return;
    }

    distanceSensor.measureDistanceCm(measurement);
    float newDist = measurement.distanceCm;

    // Проверка валидности измерения
    if (isnan(newDist) || newDist < MIN_VALID_DISTANCE || newDist > MAX_VALID_DISTANCE)
    {
        errorCount++;
        syslog_ng("US025: Invalid measurement: " + String(newDist) + ", error count: " + String(errorCount));
        if (errorCount >= MAX_ERRORS)
        {
            syslog_ng("US025: Too many invalid measurements");
            setSensorDetected("US025", 0);
            return;
        }
        return;
    }

    // Сброс счетчика ошибок при успешном измерении
    errorCount = 0;
    setSensorDetected("US025", 1);

    // Фильтрация измерений
    Dist_Kalman = KalmanDist.filtered(newDist);

    // Подробное логирование для отладки
    syslog_ng("US025: Raw=" + fFTS(newDist, 3) + 
              " Kalman=" + fFTS(Dist_Kalman, 3) + 
              " Temp=" + fFTS(AirTemp, 1) + "C" +
              " PulseTime=" + String(measurement.pulseEndUs - measurement.pulseStartUs) + "us");

    // Второе измерение для подтверждения
    vTaskDelay(pdMS_TO_TICKS(100)); // Задержка между измерениями

    distanceSensor.measureDistanceCm(measurement);
    float secondDist = measurement.distanceCm;

    // Проверка согласованности измерений
    if (abs(secondDist - newDist) > 5.0f) // Если разница больше 5см
    {
        syslog_ng("US025: Inconsistent measurements: " + fFTS(newDist, 3) + " vs " + fFTS(secondDist, 3));
        return;
    }

    // Обновляем фильтр Калмана
    Dist_Kalman = KalmanDist.filtered(secondDist);

    // Инициализация фильтра при первом запуске
    if (first_Dist)
    {
        first_Dist = false;
        for (int i = 0; i < 50; i++)
        {
            Dist_Kalman = KalmanDist.filtered(secondDist);
        }
    }

    // Выбор окончательного значения расстояния
    Dist = (DIST_KAL_E == 1) ? Dist_Kalman : secondDist;

    // Вычисление уровня воды с проверкой валидности параметров
    float wLevel = NAN;
    if (max_l_raw != min_l_raw) // Защита от деления на ноль
    {
        wLevel = (min_l_level * max_l_raw - min_l_raw * max_l_level + Dist * max_l_level - Dist * min_l_level) /
                 (max_l_raw - min_l_raw);
                 
        // Проверка валидности уровня воды
        if (wLevel < 0 || wLevel > 100)
        {
            syslog_ng("US025: Invalid water level calculated: " + fFTS(wLevel, 3));
            wLevel = NAN;
        }
    }
    else
    {
        syslog_ng("US025: Invalid calibration parameters");
    }

    // Публикация параметров с метками времени
    uint32_t timestamp = millis();
    publish_parameter("Dist", Dist, 3, 1);
    publish_parameter("DistRaw", secondDist, 3, 1);
    publish_parameter("Dist_Kalman", Dist_Kalman, 3, 1);
    if (!isnan(wLevel))
    {
        publish_parameter("wLevel", wLevel, 3, 1);
    }

    // Сохраняем последнее успешное измерение
    last_successful_measurement = timestamp;
}

TaskParams TaskUSParams;