/// ----------------- Константы и состояние -----------------
static constexpr uint8_t MAX_ERRORS = 5;
static constexpr float MIN_VALID_DISTANCE = 2.0f;
static constexpr float MAX_VALID_DISTANCE = MAX_DISTANCE_CM;

static uint8_t errorCount = 0;
static uint32_t last_successful_measurement = 0;

// ----------------- Утилита форматирования для логов -----------------
static inline const char* f2(float v, int prec = 2)
{
    static char buf[32];  // безопасный общий буфер
    if (isnan(v))
    {
        return "NaN";
    }
    snprintf(buf, sizeof(buf), "%.*f", prec, v);
    return buf;
}

// ----------------- Основная задача -----------------
void TaskUS()
{
    // Температура воздуха для компенсации
    float tempC = isnan(AirTemp) ? 20.0f : AirTemp;
    syslogf("US025: AirTemp %s°C", f2(tempC, 2));

    // Первое измерение
    float newDist = distanceSensor->dist();
    if (isnan(newDist))
    {
        if (++errorCount >= MAX_ERRORS)
        {
            syslogf("US025: Ping timeout x%d, sensor disconnected", errorCount);
            setSensorDetected("US025", 0);
        }
        else
        {
            syslogf("US025: Ping timeout, errors=%d", errorCount);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    // Проверка диапазона
    if (newDist < MIN_VALID_DISTANCE || newDist > MAX_VALID_DISTANCE)
    {
        if (++errorCount >= MAX_ERRORS)
        {
            syslogf("US025: Distance %.2fcm out of range, sensor disconnected", newDist);
            setSensorDetected("US025", 0);
        }
        else
        {
            syslogf("US025: Distance %.2fcm invalid, errors=%d", newDist, errorCount);
        }
        return;
    }

    // Успешное измерение
    errorCount = 0;

    // Фильтрация Калманом
    float kalmanDist = KalmanDist.filtered(newDist);
    syslogf("US025: Raw=%s Kalman=%s", f2(newDist), f2(kalmanDist));

    // Второе измерение для проверки
    float secondDist = distanceSensor->dist();
    if (isnan(secondDist))
    {
        syslogf("US025: Second ping timeout");
        return;
    }

    if (fabsf(secondDist - newDist) > 5.0f)
    {
        syslogf("US025: Inconsistent readings: %s vs %s", f2(newDist), f2(secondDist));
        return;
    }

    // Дополнительная стабилизация фильтра (первый раз прогнать много итераций)
    if (first_Dist)
    {
        first_Dist = false;
        for (int i = 0; i < 50; ++i)
        {
            kalmanDist = KalmanDist.filtered(secondDist);
        }
    }
    else
    {
        kalmanDist = KalmanDist.filtered(secondDist);
    }

    // Окончательное значение
    float finalDist = (DIST_KAL_E == 1) ? kalmanDist : secondDist;

    // Вычисляем уровень воды по калибровке
    if (max_l_raw != min_l_raw)
    {
        wLevel = (min_l_level * max_l_raw - min_l_raw * max_l_level + finalDist * (max_l_level - min_l_level)) /
                 (max_l_raw - min_l_raw);
        if (wLevel >= 0.0001)
        {
            publish_parameter("wLevel", wLevel, 3, 1);
        }
        else
        {
            syslogf("US025: Water level out of range=%s", f2(wLevel));
        }
    }
    else
    {
        syslogf("US025: Invalid calibration params");
    }

    last_successful_measurement = millis();
}

TaskParams TaskUSParams;
