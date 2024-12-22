void bitW4(int p1, int p2, int p3, int p4, int b1, int b2, int b3, int b4)
{
    bitWrite(bitw, p1, b1);
    bitWrite(bitw, p2, b2);
    bitWrite(bitw, p3, b3);
    bitWrite(bitw, p4, b4);
}

void StepMotor(int pins[4], bool phase1, bool phase2, bool cool, int retTime, int delTime)
{
    // enn
    if (phase1)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 0, 0, 1);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }
    if (cool)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 0, 0, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(delTime);
    }
    if (phase2)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 1, 0, 1);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }

    // twee
    if (phase1)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 1, 0, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }
    if (cool)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 0, 0, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(delTime);
    }
    if (phase2)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 1, 1, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }

    // drie
    if (phase1)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 0, 1, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }
    if (cool)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 0, 0, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(delTime);
    }
    if (phase2)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 1, 0, 1, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }

    // vier
    if (phase1)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 1, 0, 0, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }
    if (cool)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 0, 0, 0, 0);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(delTime);
    }
    if (phase2)
    {
        bitW4(pins[0], pins[1], pins[2], pins[3], 1, 0, 0, 1);
        mcp.writeGPIOAB(bitw);
        delayMicroseconds(retTime);
    }
}

void StepAF(bool phase1, bool phase2, bool cool)
{
    int pins[4] = {AA, AB, AC, AD};
    StepMotor(pins, phase1, phase2, cool, ARet, ADel);
}

void StepBF(bool phase1, bool phase2, bool cool)
{
    int pins[4] = {BA, BB, BC, BD};
    StepMotor(pins, phase1, phase2, cool, BRet, BDel);
}
