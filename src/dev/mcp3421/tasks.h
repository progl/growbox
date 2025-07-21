

void MCP3421()
{
    syslogf("MCP3421 phvalue %s; status %s", String(phvalue), String(status));
    uint8_t err = adc2.convertAndRead(MCP342x::channel1, MCP342x::oneShot, MCP342x::resolution18, MCP342x::gain4,
                                      100000, phvalue, status);
    if (!err)
    {
        pHraw = phvalue;
        PhRM.add(phvalue);
        pHmV = (4096 / pow(2, 18) * PhRM.getAverage(2) / 4);
        get_ph();
        publish_parameter("wpH", wpH, 3, 1);
        syslogf("MCP3421 RAW:%s pHmV:%s", fFTS(pHraw, 3), fFTS(pHmV, 3));
    }
    else
    {
        syslogf("MCP3421 ERROR");
    }
}
TaskParams MCP3421Params;
