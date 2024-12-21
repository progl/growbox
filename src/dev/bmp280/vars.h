uint8_t BMP280addr;
bool AHTx = false;

RunningMedian BMP280_AirTempRM = RunningMedian(4);
RunningMedian BMP280_AirHumRM = RunningMedian(4);
RunningMedian BMP280_AirPressRM = RunningMedian(4);
float BMP280_Temp, BMP280_Hum, BMP280_Press;