
void AHT10()
{
    readStatus = myAHT10.readRawData();
    float AirTemp0, AirHum0;
    if (readStatus != AHT10_ERROR)
    {
        AirTemp0 = myAHT10.readTemperature();
        AirHum0 = myAHT10.readHumidity();
        if (AirTemp0 != 255 and AirHum0 != 255 and AirTemp0 != -50)
        {
            AirTempRM.add(AirTemp0);
            AirTemp = AirTempRM.getMedianAverage(3);
            AirHumRM.add(AirHum0);
            AirHum = AirHumRM.getMedianAverage(3);

            publish_parameter("AirTemp", AirTemp, 3, 1);
            publish_parameter("AirHum", AirHum, 3, 1);
            Kornevoe = AirTemp - RootTemp;
            RootVPD = calculateVPD(RootTemp, AirHum);
            AirVPD = calculateVPD(AirTemp, AirHum);
        }
    }
    else
    {
        myAHT10.softReset();
        delay(100);
        myAHT10.begin();
        myAHT10.setNormalMode();
        myAHT10.setCycleMode();
    }

    syslog_ng("AHT10 AirTemp:" + fFTS(AirTemp, 3));
    syslog_ng("AHT10 AirHum:" + fFTS(AirHum, 3));
}
TaskParams AHT10Params;