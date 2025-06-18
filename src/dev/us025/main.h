#include <HCSR04.h>

// Флаг смены пинов (0 или 1) задаётся до setup()
int change_pins = 0;

// Максимальная дистанция (в сантиметрах) для защиты от ложных эхо
static constexpr uint16_t MAX_DISTANCE_CM = 400;

// Пины триггера и эхо
uint8_t US_TRIG_PIN;
uint8_t US_ECHO_PIN;

// Параметры фильтра Калмана
float dist_mea_e = 1.0f;
float dist_est_e = 1.0f;
float dist_q = 0.3f;
GKalman KalmanDist(dist_mea_e, dist_est_e, dist_q);
bool first_Dist = true;

// Указатель на экземпляр датчика HCSR04
HCSR04 *distanceSensor = nullptr;

// Результирующее расстояние (см)
float measuredDistance = NAN;