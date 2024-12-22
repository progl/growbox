
void run_doser_now()
{
    syslog_ng("ECDoserEnable" + String(ECDoserEnable));
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
        syslog_ng("DOSER: PumpA remains ml=" + fFTS(SetPumpA_Ml, 30));
        syslog_ng("DOSER: PumpB remains ml=" + fFTS(SetPumpB_Ml, 30));
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
            syslog_ng("DOSER: PumpA Start");
            syslog_ng("DOSER: ADel" + String(ADel));
            syslog_ng("DOSER: ARet" + String(ARet));
            syslog_ng("DOSER: StPumpA_cStep" + String(StPumpA_cStep));
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
                    preferences.putFloat("SetPumpA_Ml_SUM", SetPumpA_Ml_SUM + (StPumpA_cMl / StPumpA_cStepMl * i));
                    preferences.putLong("PumpA_Step_SUM", PumpA_Step_SUM + i);
                    vTaskDelete(NULL);
                }
            }
            preferences.putFloat("SetPumpA_Ml", SetPumpA_Ml - (StPumpA_cMl / StPumpA_cStepMl * StPumpA_cStep));
            preferences.putFloat("SetPumpA_Ml_SUM", SetPumpA_Ml_SUM + (StPumpA_cMl / StPumpA_cStepMl * StPumpA_cStep));
            preferences.putLong("PumpA_Step_SUM", PumpA_Step_SUM + StPumpA_cStep);
            syslog_ng("DOSER: PumpA SetPumpA_Ml  " + String(StPumpA_cMl / StPumpA_cStepMl * StPumpA_cStep));
            syslog_ng("DOSER: PumpA StPumpA_cStep  " + String(StPumpA_cStep));
            syslog_ng("DOSER: PumpA Stop");
        }

        if (BLeftStep <= StPumpB_cStep) StPumpB_cStep = BLeftStep;  // Если до конца цикла осталось меньше

        if (BLeftStep < 0.0000000000000000000001)
        {
            SetPumpB_Ml = -0.000;
            preferences.putFloat("SetPumpB_Ml", SetPumpB_Ml);
        }

        if (SetPumpB_Ml > 0 and BOn != 0)
        {
            syslog_ng("DOSER: PumpB Start");
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
                    preferences.putFloat("SetPumpB_Ml_SUM", SetPumpB_Ml_SUM + (StPumpB_cMl / StPumpB_cStepMl * i));
                    preferences.putLong("PumpB_Step_SUM", PumpB_Step_SUM + i);
                    vTaskDelete(NULL);
                }
            }
            preferences.putFloat("SetPumpB_Ml", SetPumpB_Ml - (StPumpB_cMl / StPumpB_cStepMl * StPumpB_cStep));
            preferences.putFloat("SetPumpB_Ml_SUM", SetPumpB_Ml_SUM + (StPumpB_cMl / StPumpB_cStepMl * StPumpB_cStep));
            preferences.putLong("PumpB_Step_SUM", PumpB_Step_SUM + StPumpB_cStep);
            syslog_ng("DOSER: PumpB SetPumpB_Ml  " + String(StPumpB_cMl / StPumpB_cStepMl * StPumpB_cStep));
            syslog_ng("DOSER: PumpB StPumpB_cStep  " + String(StPumpB_cStep));
            syslog_ng("DOSER: PumpB Stop");
        }

        bitW4(AA, AB, AC, AD, 0, 0, 0, 0);
        bitW4(BA, BB, BC, BD, 0, 0, 0, 0);
        mcp.writeGPIOAB(bitw);

        syslog_ng("DOSER: PumpA SUM ml=" + fFTS(SetPumpA_Ml_SUM, 2));
        syslog_ng("DOSER: PumpB SUM ml=" + fFTS(SetPumpB_Ml_SUM, 2));
    }
    else
        syslog_ng("DOSER: Nothing to do");

    if (ECDoserEnable == 1 and SetPumpA_Ml <= 0 and SetPumpB_Ml <= 0 and wEC and wEC < ECDoserLimit)
    {
        syslog_ng("DOSER: Current EC=" + fFTS(wEC, 3) + " < " + "LimitEC=" + fFTS(ECDoserLimit, 3));
        if (millis() - ECDoserTimer > ECDoserInterval * 1000)
        {
            ECDoserTimer = millis();
            syslog_ng("DOSER: add new task for Doser A = " + fFTS(ECDoserValueA, 3) +
                      " ml and Doser B = " + fFTS(ECDoserValueB, 3) + " ml");
            preferences.putFloat("SetPumpA_Ml", ECDoserValueA);
            preferences.putFloat("SetPumpB_Ml", ECDoserValueB);
        }

        publish_parameter("PumpA_SUM", SetPumpA_Ml_SUM, 3, 1);
        publish_parameter("PumpB_SUM", SetPumpB_Ml_SUM, 3, 1);
        publish_parameter("StepA_SUM", PumpA_Step_SUM, 3, 1);
        publish_parameter("StepB_SUM", PumpB_Step_SUM, 3, 1);
    }
    if (run_correction)
    {
        run_correction = false;
        syslog_ng("DOSER: restore pwd");
    }
    make_doser = false;
}
TaskParams run_doser_nowParams;