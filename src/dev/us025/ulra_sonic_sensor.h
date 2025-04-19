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
    static constexpr uint32_t DEFAULT_TIMEOUT_US = 200000;  // 100ms
    static constexpr float DEFAULT_TEMP_C = 20.0f;
    static constexpr uint8_t MAX_RETRIES = 3;

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
            vTaskDelay(pdMS_TO_TICKS(50 * (1 << attempt)));
        }

        consecutiveErrors_++;
        if (consecutiveErrors_ > 5)
        {
            outputMeasurement = lastValidMeasurement_;
            outputMeasurement.isValid = false;
        }
        return false;
    }

   private:
    uint8_t triggerPin_;
    uint8_t echoPin_;
    uint32_t timeoutUs_;
    volatile uint32_t pulseStartUs_;
    volatile uint32_t pulseEndUs_;
    volatile bool newMeasurementAvailable_;
    float temperatureC_;
    uint8_t consecutiveErrors_;
    UltrasonicMeasurement lastValidMeasurement_;

    bool performSingleMeasurement(UltrasonicMeasurement &measurement)
    {
        triggerPulse();
        newMeasurementAvailable_ = false;

        uint32_t startWait = millis();
        while (!newMeasurementAvailable_ && (millis() - startWait) < timeoutUs_ / 1000)
        {
            vTaskDelay(1);
        }

        if (!newMeasurementAvailable_)
        {
            measurement.distanceCm = NAN;
            measurement.distanceCm_25 = NAN;
            return false;
        }

        uint32_t durationUs = pulseEndUs_ - pulseStartUs_;
        if (durationUs >= timeoutUs_ || durationUs < 50)
        {
            measurement.distanceCm = NAN;
            measurement.distanceCm_25 = NAN;
            return false;
        }

        float speed = calculateSpeedOfSound(temperatureC_);
        float speed25 = calculateSpeedOfSound(25.0f);

        measurement.distanceCm = (speed * durationUs) / 2.0f / 10000.0f;
        measurement.distanceCm_25 = (speed25 * durationUs) / 2.0f / 10000.0f;
        measurement.pulseStartUs = pulseStartUs_;
        measurement.pulseEndUs = pulseEndUs_;
        measurement.temperatureC = temperatureC_;
        measurement.isValid = true;
        return true;
    }

    static float calculateSpeedOfSound(float tempC)
    {
        // Формула: скорость = 331.3 + 0.606 * температура
        return 331.3f + 0.606f * tempC;
    }

    bool isPinValid(uint8_t pin) { return pin < NUM_DIGITAL_PINS; }

    friend void IRAM_ATTR ultrasonicISR(void *arg);
};

// Обработчик прерывания echo пина
void IRAM_ATTR ultrasonicISR(void *arg)
{
    auto *sensor = static_cast<UltrasonicSensor *>(arg);
    if (digitalRead(sensor->echoPin_) == HIGH)
    {
        sensor->pulseStartUs_ = micros();
    }
    else
    {
        sensor->pulseEndUs_ = micros();
        sensor->newMeasurementAvailable_ = true;
    }
}
