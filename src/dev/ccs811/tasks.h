
void CCS811()
{
    ccs811.set_envdata_Celsius_percRH(AirTemp, AirHum);
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2, &etvoc, &errstat, &raw);

    // Print measurement results based on status
    if (errstat == CCS811_ERRSTAT_OK)
    {
        CCS811_eCO2RM.add(eco2);
        CCS811_tVOCRM.add(etvoc);
        eRAW = raw;
        CO2 = CCS811_eCO2RM.getAverage();
        tVOC = CCS811_tVOCRM.getAverage();

        syslogf("CCS811 CO2:  %.2f", eco2);

        syslogf("CCS811 tVOC: %.2f", etvoc);

        syslogf("CCS811 raw: %.2f", raw);
    }

    else
    {
        syslog_err("CCS811: ERROR SENSOR. Restart");
        ccs811.begin();
    }
}

TaskParams CCS811Params;