#include <Arduino.h>

// Структура для хранения данных измерения
struct UltrasonicMeasurement
{
    float distanceCm;
    float distanceCm_25;
    uint32_t pulseStartUs;
    uint32_t pulseEndUs;
    float temperatureC;  // Температура в градусах Цельсия
};

// Объявление внешней функции обработчика прерывания
void IRAM_ATTR ultrasonicISR(void *arg);

// Класс ультразвукового датчика
class UltrasonicSensor
{
   public:
    UltrasonicSensor(uint8_t triggerPin, uint8_t echoPin, uint32_t timeoutUs)
        : triggerPin_(triggerPin),
          echoPin_(echoPin),
          timeoutUs_(timeoutUs),
          pulseStartUs_(0),
          pulseEndUs_(0),
          newMeasurementAvailable_(false),
          temperatureC_(20.0)
    {
    }  // По умолчанию 20°C

    void begin()
    {
        pinMode(triggerPin_, OUTPUT);
        digitalWrite(triggerPin_, LOW);  // Установить триггер пин в LOW

        pinMode(echoPin_, INPUT);
        attachInterruptArg(digitalPinToInterrupt(echoPin_), ultrasonicISR, this,
                           CHANGE);  // Прерывание на изменение состояния
    }

    void measureDistanceCm(UltrasonicMeasurement &outputMeasurement)
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
            // Ждем завершения прерывания
        }

        if (!newMeasurementAvailable_)
        {
            outputMeasurement.distanceCm = NAN;     // Таймау
            outputMeasurement.distanceCm_25 = NAN;  // Таймау

            return;
        }

        // Рассчитываем расстояние
        uint32_t durationUs = getPulseEndUs() - getPulseStartUs();
        if (durationUs >= timeoutUs_)
        {
            outputMeasurement.distanceCm = NAN;     // Таймаут
            outputMeasurement.distanceCm_25 = NAN;  // Таймау
        }
        else
        {
            // Скорость звука с учетом температуры: V = 331.3 + 0.606 * T (м/с)
            float speedOfSound = 331.3 + 0.606 * temperatureC_;
            float speedOfSound_25 = 331.3 + 0.606 * 25;
            outputMeasurement.distanceCm = (durationUs * (speedOfSound / 10000.0)) / 2.0;
            outputMeasurement.distanceCm_25 = (durationUs * (speedOfSound_25 / 10000.0)) / 2.0;
        }

        outputMeasurement.pulseStartUs = getPulseStartUs();
        outputMeasurement.pulseEndUs = getPulseEndUs();
        outputMeasurement.temperatureC = temperatureC_;
    }

    void setTemperature(float temperatureC) { temperatureC_ = temperatureC; }

    float getTemperature() const { return temperatureC_; }

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
            // Ждем завершения прерывания
        }
        return newMeasurementAvailable_;
    }

    bool isMeasurementAvailable() const { return newMeasurementAvailable_; }

    // Getter and Setter for pulseStartUs_
    uint32_t getPulseStartUs() const { return pulseStartUs_; }

    void setPulseStartUs(uint32_t value) { pulseStartUs_ = value; }

    // Getter and Setter for pulseEndUs_
    uint32_t getPulseEndUs() const { return pulseEndUs_; }

    void setPulseEndUs(uint32_t value) { pulseEndUs_ = value; }

    // Setter for newMeasurementAvailable_
    void setMeasurementAvailable(bool value) { newMeasurementAvailable_ = value; }

    // Getter for echoPin_
    uint8_t getEchoPin() const { return echoPin_; }

   private:
    uint8_t triggerPin_;
    uint8_t echoPin_;
    uint32_t timeoutUs_;
    volatile uint32_t pulseStartUs_;         // Keep as volatile but no need for IRAM_ATTR
    volatile uint32_t pulseEndUs_;           // Keep as volatile but no need for IRAM_ATTR
    volatile bool newMeasurementAvailable_;  // Keep as volatile but no need for IRAM_ATTR
    float temperatureC_;                     // Температура в градусах Цельсия
};

// Реализация функции обработчика прерывания (ISR) за пределами класса
void IRAM_ATTR ultrasonicISR(void *arg)
{
    UltrasonicSensor *sensor = static_cast<UltrasonicSensor *>(arg);

    uint32_t currentMicros = (uint32_t)(esp_timer_get_time());  // Используем безопасный таймер ESP32
    if (digitalRead(sensor->getEchoPin()) == HIGH)
    {
        sensor->setPulseStartUs(currentMicros);
    }
    else
    {
        sensor->setPulseEndUs(currentMicros);
        sensor->setMeasurementAvailable(true);
    }
}
