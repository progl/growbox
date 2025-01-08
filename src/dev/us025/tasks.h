void TaskUS()
{
    // Измеряем время в микросекундах

    distanceSensor.triggerPulse();
    int maxAttempts = 5;  // Максимальное количество попыток
    int attempts = 0;     // Текущий счётчик попыток

    while (!distanceSensor.waitForMeasurement() && attempts < maxAttempts)
    {
        attempts++;
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Ожидание 100 мс
    }

    if (attempts >= maxAttempts)
    {
        // Обработка ошибки: сенсор не ответил за 5 попыток
        Serial.println("Error: Distance sensor measurement failed after 5 attempts.");
    }
    else
    {
        // Успешное завершение
        Serial.println("Distance measurement successful!");
    }

    if (AirTemp)
    {
        distanceSensor.setTemperature(AirTemp);
        syslog_ng("US025: AirTemp " + fFTS(AirTemp, 3));
    }

    distanceSensor.measureDistanceCm(measurement);
    float newDist = measurement.distanceCm;
    Dist_Kalman = KalmanDist.filtered(newDist);
    syslog_ng("US025: newDist " + fFTS(newDist, 3) + " newDist_25 " + fFTS(measurement.distanceCm_25, 3) +
              " DIST_KAL_E " + fFTS(DIST_KAL_E, 0) + " Dist_Kalman: " + fFTS(Dist_Kalman, 3) + " pulseStartUs: " +
              fFTS(measurement.pulseStartUs, 8) + " pulseEndUs: " + fFTS(measurement.pulseEndUs, 8) +
              " pulse: " + fFTS(measurement.pulseEndUs - measurement.pulseStartUs, 8));
    vTaskDelay(100);

    distanceSensor.measureDistanceCm(measurement);
    newDist = measurement.distanceCm;
    Dist_Kalman = KalmanDist.filtered(newDist);

    // Логирование
    syslog_ng("US025: newDist " + fFTS(newDist, 3) + " newDist_25 " + fFTS(measurement.distanceCm_25, 3) +
              " DIST_KAL_E " + fFTS(DIST_KAL_E, 0) + " Dist_Kalman: " + fFTS(Dist_Kalman, 3) + " pulseStartUs: " +
              fFTS(measurement.pulseStartUs, 8) + " pulseEndUs: " + fFTS(measurement.pulseEndUs, 8) +
              " pulse: " + fFTS(measurement.pulseEndUs - measurement.pulseStartUs, 8));

    // Инициализация фильтра, если первый раз
    if (first_Dist)
    {
        first_Dist = false;
        for (int i = 0; i < 50; i++)
        {
            Dist_Kalman = KalmanDist.filtered(newDist);
        }
    }

    // Выбор расстояния
    if (DIST_KAL_E == 1)
    {
        Dist = Dist_Kalman;
    }
    else
    {
        Dist = newDist;
    }

    // Вычисляем уровень воды
    float wLevel = (min_l_level * max_l_raw - min_l_raw * max_l_level + Dist * max_l_level - Dist * min_l_level) /
                   (-min_l_raw + max_l_raw);

    syslog_ng("US025: wLevel " + fFTS(wLevel, 3));

    // Публикация параметров
    publish_parameter("Dist", Dist, 3, 1);
    publish_parameter("DistRaw", newDist, 3, 1);  // Сырой результат
    publish_parameter("Dist_Kalman", Dist_Kalman, 3, 1);
    publish_parameter("wLevel", wLevel, 3, 1);
}

TaskParams TaskUSParams;