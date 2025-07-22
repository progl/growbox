
void run_doser_now()
{
    syslogf("ECDoserEnable %d", ECDoserEnable);
    if (ECDoserEnable == 0)
    {
        return;
    }
    SetPumpA_Ml = SetPumpA_Ml;
    SetPumpB_Ml = SetPumpB_Ml;
    pwd_port2 = PWDport2;
    pwd_port = PWDport1;
    bool run_correction = false;
    if ((SetPumpA_Ml > 0 and AOn != 0) or (SetPumpB_Ml > 0 and BOn != 0))
    {
        syslogf("DOSER: PumpA remains ml=%.2f", SetPumpA_Ml);
        syslogf("DOSER: PumpB remains ml=%.2f", SetPumpB_Ml);
        make_doser = true;
        AA = StPumpA_A;
        AB = StPumpA_B;
        AC = StPumpA_C;
        AD = StPumpA_D;
        ADel = StPumpA_Del;
        ARet = StPumpA_Ret;
        mcp.pinMode(AA, OUTPUT);
        mcp.pinMode(AB, OUTPUT);
        mcp.pinMode(AC, OUTPUT);
        mcp.pinMode(AD, OUTPUT);
        BA = StPumpB_A;
        BB = StPumpB_B;
        BC = StPumpB_C;
        BD = StPumpB_D;
        BDel = StPumpB_Del;
        BRet = StPumpB_Ret;
        mcp.pinMode(BA, OUTPUT);
        mcp.pinMode(BB, OUTPUT);
        mcp.pinMode(BC, OUTPUT);
        mcp.pinMode(BD, OUTPUT);
        StPumpA_cStepMl = StPumpA_cStepMl;
        StPumpA_cMl = StPumpA_cMl;
        AOn = StPumpA_On;
        StPumpA_cStep = StPumpA_cStep;
        float ALeftStep = (SetPumpA_Ml / StPumpA_cMl) * StPumpA_cStepMl;

        StPumpB_cStepMl = StPumpB_cStepMl;
        StPumpB_cMl = StPumpB_cMl;
        BOn = StPumpB_On;
        StPumpB_cStep = StPumpB_cStep;
        float BLeftStep = (SetPumpB_Ml / StPumpB_cMl) * StPumpB_cStepMl;

        if (ALeftStep <= StPumpA_cStep) StPumpA_cStep = ALeftStep;  // Если до конца цикла осталось меньше
        if (ALeftStep < 0.0000000000000000000001)
        {
            SetPumpA_Ml = -0.000;
            preferences.putFloat("SetPumpA_Ml", SetPumpA_Ml);
        }

        if (SetPumpA_Ml > 0 and AOn != 0)
        {
            syslogf("DOSER: PumpA Start");
            syslogf("DOSER: ADel %d", ADel);
            syslogf("DOSER: ARet %d", ARet);
            syslogf("DOSER: StPumpA_cStep %.2f", StPumpA_cStep);
            // ledcWrite(PwdChannel1, 255);
            // ledcWrite(PwdChannel2, 255);
            run_correction = true;
            for (long i = 0; i < StPumpA_cStep; i++)
            {
                StepAF(true, true, true);
                if (OtaStart == true)
                {
                    bitW4(AA, AB, AC, AD, 0, 0, 0, 0);
                    bitW4(BA, BB, BC, BD, 0, 0, 0, 0);
                    mcp.writeGPIOAB(bitw);
                    preferences.putFloat("SetPumpA_Ml", SetPumpA_Ml - (StPumpA_cMl / StPumpA_cStepMl * i));
                    preferences.putFloat("PumpA_SUM", PumpA_SUM + (StPumpA_cMl / StPumpA_cStepMl * i));
                    preferences.putFloat("StepA_SUM", StepA_SUM + i);
                    while (OtaStart == true) vTaskDelay(1000);
                }
            }
            preferences.putFloat("SetPumpA_Ml", SetPumpA_Ml - (StPumpA_cMl / StPumpA_cStepMl * StPumpA_cStep));
            preferences.putFloat("PumpA_SUM", PumpA_SUM + (StPumpA_cMl / StPumpA_cStepMl * StPumpA_cStep));
            preferences.putFloat("StepA_SUM", StepA_SUM + StPumpA_cStep);
            syslogf("DOSER: PumpA SetPumpA_Ml  %.2f", StPumpA_cMl / StPumpA_cStepMl * StPumpA_cStep);
            syslogf("DOSER: PumpA StPumpA_cStep  %.2f", StPumpA_cStep);
            syslogf("DOSER: PumpA Stop");
        }

        if (BLeftStep <= StPumpB_cStep) StPumpB_cStep = BLeftStep;  // Если до конца цикла осталось меньше

        if (BLeftStep < 0.0000000000000000000001)
        {
            SetPumpB_Ml = -0.000;
            preferences.putFloat("SetPumpB_Ml", SetPumpB_Ml);
        }

        if (SetPumpB_Ml > 0 and BOn != 0)
        {
            syslogf("DOSER: PumpB Start");
            // ledcWrite(PwdChannel1, 255);
            // ledcWrite(PwdChannel2, 255);

            run_correction = true;
            for (long i = 0; i < StPumpB_cStep; i++)
            {
                StepBF(true, true, true);
                if (OtaStart == true)
                {
                    bitW4(AA, AB, AC, AD, 0, 0, 0, 0);
                    bitW4(BA, BB, BC, BD, 0, 0, 0, 0);
                    mcp.writeGPIOAB(bitw);
                    preferences.putFloat("SetPumpB_Ml", SetPumpB_Ml - (StPumpB_cMl / StPumpB_cStepMl * i));
                    preferences.putFloat("PumpB_SUM", PumpB_SUM + (StPumpB_cMl / StPumpB_cStepMl * i));
                    preferences.putLong("StepB_SUM", StepB_SUM + i);
                    while (OtaStart == true) vTaskDelay(1000);
                }
            }
            preferences.putFloat("SetPumpB_Ml", SetPumpB_Ml - (StPumpB_cMl / StPumpB_cStepMl * StPumpB_cStep));
            preferences.putFloat("PumpB_SUM", PumpB_SUM + (StPumpB_cMl / StPumpB_cStepMl * StPumpB_cStep));
            preferences.putLong("StepB_SUM", StepB_SUM + StPumpB_cStep);
            syslogf("DOSER: PumpB SetPumpB_Ml  %.2f", StPumpB_cMl / StPumpB_cStepMl * StPumpB_cStep);
            syslogf("DOSER: PumpB StPumpB_cStep  %.2f", StPumpB_cStep);
            syslogf("DOSER: PumpB Stop");
        }

        bitW4(AA, AB, AC, AD, 0, 0, 0, 0);
        bitW4(BA, BB, BC, BD, 0, 0, 0, 0);
        mcp.writeGPIOAB(bitw);

        syslogf("DOSER: PumpA SUM ml=%.2f", PumpA_SUM);
        syslogf("DOSER: PumpB SUM ml=%.2f", PumpB_SUM);
    }
    else
        syslogf("DOSER: Nothing to do");

    if (ECDoserEnable == 1 and SetPumpA_Ml <= 0 and SetPumpB_Ml <= 0 and wEC and wEC < ECDoserLimit)
    {
        syslogf("DOSER: Current EC=%.2f < LimitEC=%.2f", wEC, ECDoserLimit);
        if (millis() - ECDoserTimer > ECDoserInterval * 1000)
        {
            ECDoserTimer = millis();
            syslogf("DOSER: add new task for Doser A = %.2f ml and Doser B = %.2f ml", ECDoserValueA, ECDoserValueB);
            preferences.putFloat("SetPumpA_Ml", ECDoserValueA);
            preferences.putFloat("SetPumpB_Ml", ECDoserValueB);
        }

        publish_parameter("PumpA_SUM", PumpA_SUM, 3, 1);
        publish_parameter("PumpB_SUM", PumpB_SUM, 3, 1);
        publish_parameter("StepA_SUM", StepA_SUM, 3, 1);
        publish_parameter("StepB_SUM", StepB_SUM, 3, 1);
    }
    if (run_correction)
    {
        run_correction = false;
        syslogf("DOSER: restore pwd");
    }
    make_doser = false;
}
TaskParams run_doser_nowParams;