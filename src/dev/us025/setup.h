// Проверка и инициализация пинов
if (change_pins == 1)
{
    US_TRIG_PIN = 13;
    US_ECHO_PIN = 14;
}
else
{
    US_TRIG_PIN = 14;
    US_ECHO_PIN = 13;
}

syslog_ng("US025: US_TRIG_PIN: " + String(US_TRIG_PIN));
syslog_ng("US025: US_ECHO_PIN: " + String(US_ECHO_PIN));

distanceSensor = new HCSR04(US_TRIG_PIN, US_ECHO_PIN);
float initDist = distanceSensor->dist();
if (isnan(initDist) || initDist < 1.0f || initDist > MAX_DISTANCE_CM)
{
    syslog_ng("US025: Initial measurement out of range or invalid: " + String(initDist));
    setSensorDetected("US025", 0);
    return;
}

// Успешная инициализация
setSensorDetected("US025", 1);
syslog_ng("US025: Initialized, initial distance = " + String(initDist, 2) + " cm");

// Запуск задачи измерения
TaskUSParams = {"US", TaskUS, 30000, xSemaphore_C};
int err = xTaskCreatePinnedToCore(TaskTemplate, "US", stack_size, (void *)&TaskUSParams, 1, NULL, 1);
if (err != pdPASS)
{
    syslog_ng("US025: Failed to create task (err=" + String(err) + ")");
    setSensorDetected("US025", 0);
    return;
}

KalmanDist = GKalman(dist_mea_e, dist_est_e, dist_q);
syslog_ng("US025: Task created successfully");