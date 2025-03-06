

void MCP3421()
{
    syslog_ng("MCP3421 phvalue " + String(phvalue) + "; " + " status " + status);
    uint8_t err = adc2.convertAndRead(MCP342x::channel1, MCP342x::oneShot, MCP342x::resolution18, MCP342x::gain4,
                                      100000, phvalue, status);
    if (!err)
    {
        pHraw = phvalue;
        PhRM.add(phvalue);
        pHmV = (4096 / pow(2, 18) * PhRM.getAverage(2) / 4);
        get_ph();
        publish_parameter("wpH", wpH, 3, 1);
        syslog_ng("MCP3421 RAW:" + fFTS(pHraw, 3) + " pHmV:" + fFTS(pHmV, 3));
    }
    else
    {
        syslog_ng("MCP3421 ERROR");
    }
}
TaskParams MCP3421Params;
