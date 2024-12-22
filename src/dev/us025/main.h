

int timeout = 200000;
#include <dev/us025/ulra_sonic_sensor.h>
int US_ECHO, US_TRIG, change_pins;

float dist_mea_e = 4;
float dist_est_e = 4;
float dist_q = 0.3;

float Dist_Kalman;
GKalman KalmanDist(dist_mea_e, dist_est_e, dist_q);
boolean first_Dist = true;
boolean first_Level = true;

// Экземпляр и использование
UltrasonicSensor distanceSensor(13, 14, timeout);  // 200000 мс таймаут - мало ли в потолок смотрим
UltrasonicMeasurement measurement;
