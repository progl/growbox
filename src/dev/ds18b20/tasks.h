bool first_dallas = true;

void DS18B20()
{
    sens18b20.requestTemperatures();
    for (uint8_t i = 0; i < sensorCount && i < MAX_DS18B20_SENSORS; i++)
    {
        float tempC = sens18b20.getTempC(sensorArray[i].address);

        syslogf("DS18B20 Sensor %d adr: %s Temp: %.2f", i, deviceAddressToString(sensorArray[i].address).c_str(),
                tempC);

        if (tempC != DEVICE_DISCONNECTED_C)
        {
            if (E_dallas_kalman)
            {
                if (first_dallas)
                {
                    for (int j = 0; j < 100; j++)  // Use a different variable for the inner loop
                    {
                        sensorArray[i].KalmanDallasTmp.filtered(tempC);
                    }
                    tempC = sensorArray[i].KalmanDallasTmp.filtered(tempC);
                }
                tempC = sensorArray[i].KalmanDallasTmp.filtered(tempC);
            }

            publish_parameter("dallas__" + deviceAddressToString(sensorArray[i].address), tempC, 3, 1);

            if (sensorArray[i].key == "RootTemp")
            {
                syslogf("DS18B20 found by address RootTemp Sensor: %.2f", tempC);

                RootTemp = tempC;
                Kornevoe = AirTemp - RootTemp;

                AirVPD = calculateVPD(AirTemp, AirHum);
                publish_parameter("RootTemp", RootTemp, 3, 1);
            }
            if (sensorArray[i].key == "wNTC")
            {
                wNTC = tempC;
                syslogf("DS18B20 found by address wNTC Sensor: %.2f", wNTC);
                publish_parameter("wNTC", wNTC, 3, 1);
            }
        }
    }
    first_dallas = false;  // Move this outside the loop to ensure it only runs once
}

TaskParams DS18B20Params;