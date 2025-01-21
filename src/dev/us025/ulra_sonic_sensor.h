#include <Arduino.h>

// Структура для хранения данных измерения
struct UltrasonicMeasurement
{
    float distanceCm;
    float distanceCm_25;
    uint32_t pulseStartUs;
    uint32_t pulseEndUs;
    float temperatureC;  // Температура в градусах Цельсия
    bool isValid;        // Флаг валидности измерения
    uint32_t timestamp;  // Временная метка измерения
};

// Объявление внешней функции обработчика прерывания
void IRAM_ATTR ultrasonicISR(void *arg);

// Класс ультразвукового датчика
class UltrasonicSensor
{
   public:
    // Константы класса
    static constexpr uint32_t DEFAULT_TIMEOUT_US = 200000;  // 200ms
    static constexpr float DEFAULT_TEMP_C = 20.0f;          // 20°C
    static constexpr uint8_t MAX_RETRIES = 3;               // Максимальное количество попыток измерения

    UltrasonicSensor(uint8_t triggerPin, uint8_t echoPin, uint32_t timeoutUs = DEFAULT_TIMEOUT_US)
        : triggerPin_(triggerPin),
          echoPin_(echoPin),
          timeoutUs_(timeoutUs),
          pulseStartUs_(0),
          pulseEndUs_(0),
          newMeasurementAvailable_(false),
          temperatureC_(DEFAULT_TEMP_C),
          consecutiveErrors_(0)
    {
        memset(&lastValidMeasurement_, 0, sizeof(lastValidMeasurement_));
    }

    bool begin()
    {
        if (!isPinValid(triggerPin_) || !isPinValid(echoPin_))
        {
            return false;
        }

        pinMode(triggerPin_, OUTPUT);
        digitalWrite(triggerPin_, LOW);

        pinMode(echoPin_, INPUT);
        attachInterruptArg(digitalPinToInterrupt(echoPin_), ultrasonicISR, this, CHANGE);
        return true;
    }

    void setTemperature(float temperatureC) { temperatureC_ = temperatureC; }

    void triggerPulse()
    {
        digitalWrite(triggerPin_, HIGH);
        delayMicroseconds(10);
        digitalWrite(triggerPin_, LOW);
    }

    bool waitForMeasurement()
    {
        newMeasurementAvailable_ = false;
        uint32_t startWait = millis();
        while (!newMeasurementAvailable_ && (millis() - startWait) < timeoutUs_ / 1000)
        {
            vTaskDelay(1);
        }
        return newMeasurementAvailable_;
    }

    bool measureDistanceCm(UltrasonicMeasurement &outputMeasurement)
    {
        outputMeasurement.isValid = false;
        outputMeasurement.timestamp = millis();

        for (uint8_t attempt = 0; attempt < MAX_RETRIES; ++attempt)
        {
            if (performSingleMeasurement(outputMeasurement))
            {
                consecutiveErrors_ = 0;
                lastValidMeasurement_ = outputMeasurement;
                outputMeasurement.isValid = true;
                return true;
            }
            // Экспоненциальная задержка между попытками
            vTaskDelay(pdMS_TO_TICKS(50 * (1 << attempt)));
        }

        consecutiveErrors_++;
        if (consecutiveErrors_ > 5)
        {
            // Возвращаем последнее валидное измерение с пометкой
            outputMeasurement = lastValidMeasurement_;
            outputMeasurement.isValid = false;
        }
        return false;
    }

   private:
    bool performSingleMeasurement(UltrasonicMeasurement &measurement)
    {
        // Генерируем триггерный импульс
        digitalWrite(triggerPin_, HIGH);
        delayMicroseconds(10);
        digitalWrite(triggerPin_, LOW);

        // Ждем завершения измерения
        newMeasurementAvailable_ = false;
        uint32_t startWait = millis();
        while (!newMeasurementAvailable_ && (millis() - startWait) < timeoutUs_ / 1000)
        {
            vTaskDelay(1);  // Освобождаем процессор
        }

        if (!newMeasurementAvailable_)
        {
            measurement.distanceCm = NAN;
            measurement.distanceCm_25 = NAN;
            return false;
        }

        // Рассчитываем расстояние
        uint32_t durationUs = pulseEndUs_ - pulseStartUs_;
        if (durationUs >= timeoutUs_ || durationUs < 50)
        {  // Проверка на минимальную длительность
            measurement.distanceCm = NAN;
            measurement.distanceCm_25 = NAN;
            return false;
        }

        // Скорость звука с учетом температуры
        float speedOfSound = calculateSpeedOfSound(temperatureC_);
        float speedOfSound_25 = calculateSpeedOfSound(25.0f);

        measurement.distanceCm = calculateDistance(durationUs, speedOfSound);
        measurement.distanceCm_25 = calculateDistance(durationUs, speedOfSound_25);

        // Проверка на валидность результата
        if (!isValidDistance(measurement.distanceCm))
        {
            return false;
        }

        measurement.pulseStartUs = pulseStartUs_;
        measurement.pulseEndUs = pulseEndUs_;
        measurement.temperatureC = temperatureC_;
        return true;
    }

    static float calculateSpeedOfSound(float tempC) { return 331.3f + 0.606f * tempC; }

    static float calculateDistance(uint32_t durationUs, float speedOfSound)
    {
        return (durationUs * (speedOfSound / 10000.0f)) / 2.0f;
    }

    static bool isValidDistance(float distance) { return !isnan(distance) && distance >= 2.0f && distance <= 400.0f; }

    static bool isPinValid(uint8_t pin)
    {
        return pin < 40;  // Проверка на валидность пина для ESP32
    }

    uint8_t triggerPin_;
    uint8_t echoPin_;
    uint32_t timeoutUs_;
    volatile uint32_t pulseStartUs_;
    volatile uint32_t pulseEndUs_;
    volatile bool newMeasurementAvailable_;
    float temperatureC_;
    uint8_t consecutiveErrors_;
    UltrasonicMeasurement lastValidMeasurement_;

    friend void IRAM_ATTR ultrasonicISR(void *arg);
};

// Реализация функции обработчика прерывания
void IRAM_ATTR ultrasonicISR(void *arg)
{
    UltrasonicSensor *sensor = static_cast<UltrasonicSensor *>(arg);
    if (!sensor) return;

    uint32_t now = micros();
    if (digitalRead(sensor->echoPin_))
    {
        sensor->pulseStartUs_ = now;
    }
    else
    {
        sensor->pulseEndUs_ = now;
        sensor->newMeasurementAvailable_ = true;
    }
}
