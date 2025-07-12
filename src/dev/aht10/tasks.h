
void AHT10()
{
    sensors_event_t humidity, temp;
    myAHT10.getEvent(&humidity, &temp);
    float AirTemp0 = temp.temperature;
    float AirHum0 = humidity.relative_humidity;
    if (!isnan(AirTemp0) && !isnan(AirHum0))
    {
        if (AirTemp0 != 255 and AirHum0 != 255 and AirTemp0 != -50)
        {
            AirTempRM.add(AirTemp0);
            AirTemp = AirTempRM.getMedianAverage(3);
            AirHumRM.add(AirHum0);
            AirHum = AirHumRM.getMedianAverage(3);
            Kornevoe = AirTemp - RootTemp;
            AirVPD = calculateVPD(AirTemp, AirHum);

            publish_parameter("AirTemp", AirTemp, 3, 1);
            publish_parameter("AirHum", AirHum, 3, 1);
            publish_parameter("AirVPD", AirVPD, 3, 1);
        }
    }
    else
    {
        vTaskDelay(100);
        myAHT10.begin();
    }

    syslog_ng("AHT10 AirTemp:" + fFTS(AirTemp, 3));
    syslog_ng("AHT10 AirHum:" + fFTS(AirHum, 3));
}
TaskParams AHT10Params;