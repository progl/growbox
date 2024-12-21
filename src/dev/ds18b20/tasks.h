void DS18B20()
{

  sens18b20.requestTemperatures();

  for (uint8_t i = 0; i < sensorCount && i < MAX_DS18B20_SENSORS; i++)
  {
    DeviceAddress currentAddress;
    sens18b20.getAddress(currentAddress, i);

    float tempC = sens18b20.getTempC(currentAddress);

    if (tempC != DEVICE_DISCONNECTED_C)
    {
      // Сохраняем адрес и температуру в массив
      memcpy(sensorArray[i].address, currentAddress, sizeof(DeviceAddress));
      sensorArray[i].temperature = tempC;
      publish_parameter("dallas__" + deviceAddressToString(currentAddress), tempC, 3, 1);

      // Логируем данные
      syslog_ng("DS18B20 Sensor " + String(i) + " Temp: " + fFTS(tempC, 3));

      DeviceAddress deviceAdrCompare;
      DeviceAddress deviceAdrCompare2;

      // Преобразуем строки в адреса устройств
      stringToDeviceAddress(rootTempAddressString, deviceAdrCompare);
      stringToDeviceAddress(WNTCAddressString, deviceAdrCompare2);

      // Создаем массив и копируем в него адреса после их инициализации
      DeviceAddress addressesToCompare[2];
      memcpy(addressesToCompare[0], deviceAdrCompare, sizeof(DeviceAddress));
      memcpy(addressesToCompare[1], deviceAdrCompare2, sizeof(DeviceAddress));
      syslog_ng("DS18B20 rootTempAddressString: " + rootTempAddressString);
      syslog_ng("DS18B20 WNTCAddressString: " + WNTCAddressString);
      for (int j = 0; j < 2; j++)
      {
        syslog_ng("DS18B20 cmp: " + deviceAddressToString(addressesToCompare[j]) + ":" + deviceAddressToString(currentAddress));
        if (memcmp(currentAddress, addressesToCompare[j], sizeof(DeviceAddress)) == 0)
        {
          syslog_ng("DS18B20 cmp true: " + deviceAddressToString(addressesToCompare[j]) + ":" + deviceAddressToString(currentAddress));

          if (j == 0)
          {
            
            syslog_ng("DS18B20 found by adress RootTemp Sensor: " + fFTS(RootTemp, 3));
            RootTempRM.add(tempC);
            RootTemp = RootTempRM.getAverage(3);

            publish_parameter("RootTemp", RootTemp, 3, 1);
            Kornevoe = AirTemp - RootTemp;
            rootVPD = calculateVPD(RootTemp, AirHum);
            airVPD = calculateVPD(AirTemp, AirHum);
          }
          if (j == 1)
          {
            wNTC = tempC;
            syslog_ng("DS18B20 found by adress wNTC Sensor: " + fFTS(wNTC, 3));
            publish_parameter("wNTC", wNTC, 3, 1);
            preferences.putFloat("wNTC", wNTC);
          }
        }
      }
    }
  }
}
  TaskParams DS18B20Params  ;