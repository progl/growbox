void BMP280()
{
    static Adafruit_BME280 bme;  // Use static to avoid re-instantiation
    if (!bme.begin(0x76))
    {  // Use your actual I2C address
        syslog_err("BME280: Sensor initialization failed");
        return;
    }
    syslogf("BME280: Sensor initialized");

    BMP280_Press = bme.readPressure() / 133.322368;  // Pa to mmHg
    char buffer[32];
    syslogf("BME280 AirPress: %.2f", BMP280_Press);

    BMP280_Temp = bme.readTemperature();
    syslogf("BME280 BMP280_Temp: %.2f", BMP280_Temp);
    BMP280_AirTempRM.add(BMP280_Temp);
    AirTemp = BMP280_AirTempRM.getMedianAverage(3);

    AirVPD = calculateVPD(AirTemp, AirHum);
    syslogf("BME280 AirTemp: %.2f", AirTemp);

    if (bme.readHumidity() != NAN)
    {
        BMP280_AirHumRM.add(bme.readHumidity());
        AirHum = BMP280_AirHumRM.getMedianAverage(3);
        syslogf("BME280 AirHum: %.2f", AirHum);
    }

    BMP280_AirPressRM.add(BMP280_Press);
    AirPress = BMP280_AirPressRM.getMedianAverage(3);

    publish_parameter("AirPress", AirPress, 3, 1);
    publish_parameter("AirTemp", AirTemp, 3, 1);
    publish_parameter("AirHum", AirHum, 3, 1);
    publish_parameter("AirVPD", AirVPD, 3, 1);
}

TaskParams BMP280Params76;
TaskParams BMP280Params77;