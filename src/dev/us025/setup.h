// Проверка и инициализация пинов
if (change_pins == 1)
{
    US_ECHO = 14;
    US_TRIG = 13;
}
else
{
    US_ECHO = 13;
    US_TRIG = 14;
}

// Создание и инициализация датчика
UltrasonicSensor distanceSensor(US_ECHO, US_TRIG, 200000);

// Инициализация датчика
if (!distanceSensor.begin())
{
    syslog_ng("US025: Failed to initialize");
    setSensorDetected("US025", 0);
    return;
}

// Тестовое измерение
distanceSensor.triggerPulse();
unsigned long start = millis();
bool measurementSuccess = false;

// Попытка получить измерение с таймаутом
while (millis() - start < 1000)
{
    if (distanceSensor.waitForMeasurement())
    {
        measurementSuccess = true;
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}

if (!measurementSuccess)
{
    syslog_ng("US025: Initial measurement timeout");
    setSensorDetected("US025", 0);
    return;
}

// Получение результата измерения
UltrasonicMeasurement measurement;
distanceSensor.measureDistanceCm(measurement);

// Проверка валидности измерения
if (isnan(measurement.distanceCm))
{
    syslog_ng("US025: Invalid initial measurement");
    setSensorDetected("US025", 0);
    return;
}

// Проверка диапазона измерения
if (measurement.distanceCm < 2.0 || measurement.distanceCm > 400.0)
{
    syslog_ng("US025: Initial measurement out of range: " + String(measurement.distanceCm));
    setSensorDetected("US025", 0);
    return;
}

// Успешная инициализация
setSensorDetected("US025", 1);
syslog_ng("US025: Successfully initialized. Initial distance: " + String(measurement.distanceCm) + " cm");

// Создание задачи измерения
TaskUSParams = {"US", TaskUS, 50000, xSemaphore_C};
int US025_TaskErr = xTaskCreatePinnedToCore(TaskTemplate, "US", stack_size, (void *)&TaskUSParams, 1, NULL, 1);

if (US025_TaskErr != pdPASS)
{
    syslog_ng("US025: Failed to create task: " + String(US025_TaskErr));
    setSensorDetected("US025", 0);
    return;
}

// Инициализация фильтра Калмана
KalmanDist = GKalman(dist_mea_e, dist_est_e, dist_q);

syslog_ng("US025: Task created successfully");
