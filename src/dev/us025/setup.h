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

syslogf("US025: US_TRIG_PIN: %d", US_TRIG_PIN);
syslogf("US025: US_ECHO_PIN: %d", US_ECHO_PIN);

distanceSensor = new HCSR04(US_TRIG_PIN, US_ECHO_PIN);
float initDist = distanceSensor->dist();
if (isnan(initDist) || initDist < 1.0f || initDist > MAX_DISTANCE_CM)
{
    syslogf("US025: Initial measurement out of range or invalid: %.3fms", initDist);
    setSensorDetected("US025", 0);
    return;
}

// Успешная инициализация
setSensorDetected("US025", 1);
syslogf("US025: Initialized, initial distance = %.3fms", initDist);

// Запуск задачи измерения
TaskUSParams = {"US", TaskUS, 30000, xSemaphore_C};
addTask(&TaskUSParams);

KalmanDist = GKalman(dist_mea_e, dist_est_e, dist_q);
syslogf("US025: Task created successfully");