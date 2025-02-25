void BMP280()
{
    BMx280I2C bmx280(BMP280addr);
    syslog_ng("BMx280: Initializing sensor");
    if (!bmx280.begin())
    {
        syslog_ng("BMx280: Resetting to defaults");
        bmx280.resetToDefaults();
        vTaskDelay(100);
        if (!bmx280.begin())
        {
            syslog_err("BMx280: Sensor initialization failed");
            return;
        }
    }

    bool isBME280 = bmx280.isBME280();
    syslog_ng(isBME280 ? "BMx280: Found sensor is a BME280" : "BMx280: Found sensor is a BMP280");

    syslog_ng("BMx280: Resetting to defaults");
    bmx280.resetToDefaults();

    syslog_ng("BMx280: Setting oversampling");
    bmx280.writeOversamplingPressure(BMx280MI::OSRS_P_x16);
    bmx280.writeOversamplingTemperature(BMx280MI::OSRS_T_x16);

    if (isBME280) bmx280.writeOversamplingHumidity(BMx280MI::OSRS_H_x16);

    syslog_ng("BMx280: Starting measurement");
    if (!bmx280.measure())
    {
        syslog_err("BMx280: Measurement start failed, possibly already running");
        return;
    }

    for (int i = 0; !bmx280.hasValue() && i < 10; i++)
    {
        vTaskDelay(100);
    }

    if (!bmx280.hasValue())
    {
        syslog_err("BMx280: Timed out waiting for measurement");
        return;
    }

    BMP280_Press = bmx280.getPressure64() * 0.00750063755419211;
    syslog_ng("BMx280 AirPress: " + fFTS(BMP280_Press, 3));

    if (!AHTx)
    {
        BMP280_Temp = bmx280.getTemperature();
        syslog_ng("BMx280 BMP280_Temp: " + fFTS(BMP280_Temp, 3));
        BMP280_AirTempRM.add(BMP280_Temp);
        AirTemp = BMP280_AirTempRM.getMedianAverage(3);
        RootVPD = calculateVPD(RootTemp, AirHum);
        AirVPD = calculateVPD(AirTemp, AirHum);
        syslog_ng("BMx280 AirTemp: " + fFTS(AirTemp, 3));

        if (isBME280)
        {
            BMP280_AirHumRM.add(bmx280.getHumidity());
            AirHum = BMP280_AirHumRM.getMedianAverage(3);
            syslog_ng("BMx280 AirHum: " + fFTS(AirHum, 3));
        }
    }

    BMP280_AirPressRM.add(BMP280_Press);
    AirPress = BMP280_AirPressRM.getMedianAverage(3);

    publish_parameter("AirPress", AirPress, 3, 1);
    publish_parameter("AirTemp", AirTemp, 3, 1);
    publish_parameter("AirHum", AirHum, 3, 1);
}

TaskParams BMP280Params76;
TaskParams BMP280Params77;