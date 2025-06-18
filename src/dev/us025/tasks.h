// TaskUS using HCSR04::dist() which returns distance in cm
static const uint8_t MAX_ERRORS = 5;
static uint8_t errorCount = 0;
static constexpr float MIN_VALID_DISTANCE = 2.0f;
static constexpr float MAX_VALID_DISTANCE = MAX_DISTANCE_CM;
static uint32_t last_successful_measurement = 0;

void TaskUS()
{
    // Читаем и логируем температуру
    float tempC = isnan(AirTemp) ? 20.0f : AirTemp;
    syslog_ng("US025: AirTemp " + fFTS(tempC, 2) + " °C");

    // Первое измерение (возвращает см или NAN при таймауте)
    float newDist = distanceSensor->dist();
    if (isnan(newDist))
    {
        errorCount++;
        syslog_ng("US025: Ping timeout, errors=" + String(errorCount));
        if (errorCount >= MAX_ERRORS)
        {
            syslog_ng("US025: Too many ping errors, sensor disconnected");
            setSensorDetected("US025", 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    // Проверка диапазона
    if (newDist < MIN_VALID_DISTANCE || newDist > MAX_VALID_DISTANCE)
    {
        errorCount++;
        syslog_ng("US025: Invalid measurement=" + fFTS(newDist, 2) + ", errors=" + String(errorCount));
        if (errorCount >= MAX_ERRORS)
        {
            syslog_ng("US025: Too many invalid readings, sensor disconnected");
            setSensorDetected("US025", 0);
        }
        return;
    }

    // Успешное измерение
    errorCount = 0;

    // Фильтрация Калманом
    float kalmanDist = KalmanDist.filtered(newDist);
    syslog_ng("US025: Raw=" + fFTS(newDist, 2) + "cm Kalman=" + fFTS(kalmanDist, 2) + "cm");
    float secondDist = distanceSensor->dist();
    if (isnan(secondDist))
    {
        syslog_ng("US025: Second ping timeout");
        return;
    }

    if (fabs(secondDist - newDist) > 5.0f)
    {
        syslog_ng("US025: Inconsistent: " + fFTS(newDist, 2) + " vs " + fFTS(secondDist, 2));
        return;
    }

    // Обновляем фильтр Калмана
    kalmanDist = KalmanDist.filtered(secondDist);
    if (first_Dist)
    {
        first_Dist = false;
        for (int i = 0; i < 50; ++i)
        {
            kalmanDist = KalmanDist.filtered(secondDist);
        }
    }

    // Окончательное расстояние
    float finalDist = (DIST_KAL_E == 1) ? kalmanDist : secondDist;

    // Вычисление уровня воды
    if (max_l_raw != min_l_raw)
    {
        wLevel = (min_l_level * max_l_raw - min_l_raw * max_l_level + finalDist * (max_l_level - min_l_level)) /
                 (max_l_raw - min_l_raw);
        if (wLevel >= 0)
        {
            publish_parameter("wLevel", wLevel, 3, 1);
        }
        else
        {
            syslog_ng("US025: Water level out of range=" + fFTS(wLevel, 2));
        }
    }
    else
    {
        syslog_ng("US025: Invalid calibration params");
    }

    // Метка времени успешного измерения
    last_successful_measurement = millis();
}

TaskParams TaskUSParams;
