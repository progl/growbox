
String rootStyle;
String airStyle;
#include <unordered_map>

enum DataType
{
    FLOAT,
    STRING,
    BOOLEAN,
    INTEGER
};

struct Data
{
    union Value
    {
        float fValue;
        char sValue[50];  // choose an appropriate size
        bool bValue;
        int iValue;
    } value;

    DataType type;

    Data(float f) : type(FLOAT) { value.fValue = f; }
    Data(const char *s) : type(STRING) { strncpy(value.sValue, s, sizeof(value.sValue)); }
    Data(bool b) : type(BOOLEAN) { value.bValue = b; }
    Data(int i) : type(INTEGER) { value.iValue = i; }
};

struct PreferenceItem
{
    const char *key;
    DataType type;
    Data defaultValue;
    void *variable;
    Preferences *preferences;
};

PreferenceItem preferencesArray[] = {
    {"UPDATE_URL", DataType::STRING, {"default_url"}, &UPDATE_URL, &preferences},
    {"update_token", DataType::STRING, {"default_token"}, &update_token, &preferences},
    {"wegadb", DataType::STRING, {"default_wegadb"}, &wegadb, &preferences},
    {"ssid", DataType::STRING, {"ESP32"}, &ssid, &preferences},
    {"password", DataType::STRING, {"ESP32"}, &password, &preferences},
    {"appName", DataType::STRING, {"appName"}, &appName, &preferences},
    {"HOSTNAME", DataType::STRING, {"growbox"}, &HOSTNAME, &preferences},
    {"ntc_mea_e", DataType::FLOAT, Data(0.0f), &ntc_mea_e, &preferences},
    {"ntc_est_e", DataType::FLOAT, Data(0.0f), &ntc_est_e, &preferences},
    {"", DataType::FLOAT, Data(0.0f), &VccRawUser, &preferences},

    {"ntc_q", DataType::FLOAT, Data(0.0f), &ntc_q, &preferences},
    {"NTC_KAL_E", DataType::INTEGER, {0}, &NTC_KAL_E, &preferences},
    {"dist_mea_e", DataType::FLOAT, Data(0.0f), &dist_mea_e, &preferences},
    {"dist_est_e", DataType::FLOAT, Data(0.0f), &dist_est_e, &preferences},
    {"dist_q", DataType::FLOAT, Data(0.0f), &dist_q, &preferences},
    {"DIST_KAL_E", DataType::INTEGER, {0}, &DIST_KAL_E, &preferences},
    {"ec_mea_e", DataType::FLOAT, Data(0.0f), &ec_mea_e, &preferences},
    {"ec_est_e", DataType::FLOAT, Data(0.0f), &ec_est_e, &preferences},
    {"ec_q", DataType::FLOAT, Data(0.0f), &ec_q, &preferences},
    {"EC_KAL_E", DataType::INTEGER, {0}, &EC_KAL_E, &preferences},
    {"RDEnable", DataType::INTEGER, {0}, &RDEnable, &preferences},
    {"RDDelayOn", DataType::INTEGER, {60}, &RDDelayOn, &preferences},
    {"RDDelayOff", DataType::INTEGER, {300}, &RDDelayOff, &preferences},
    {"RDSelectedRP", DataType::INTEGER, {0}, &RDSelectedRP, &preferences},
    {"disable_ntc", DataType::INTEGER, {0}, &disable_ntc, &preferences},
    {"ECDoserEnable", DataType::INTEGER, {0}, &ECDoserEnable, &preferences},
    {"ECDoserInterval", DataType::INTEGER, {0}, &ECDoserInterval, &preferences},
    {"ECDoserLimit", DataType::FLOAT, Data(0.0f), &ECDoserLimit, &preferences},
    {"ECDoserValueA", DataType::FLOAT, Data(0.0f), &ECDoserValueA, &preferences},
    {"ECDoserValueB", DataType::FLOAT, Data(0.0f), &ECDoserValueB, &preferences},
    {"RDPWD", DataType::INTEGER, {256}, &RDPWD, &preferences},
    {"RDWorkNight", DataType::INTEGER, {1}, &RDWorkNight, &preferences},
    {"tR_DAC", DataType::FLOAT, Data(0.0f), &tR_DAC, &preferences},
    {"tR_B", DataType::FLOAT, Data(0.0f), &tR_B, &preferences},
    {"tR_val_korr", DataType::FLOAT, Data(0.0f), &tR_val_korr, &preferences},
    {"vpdstage", DataType::STRING, {"Start"}, &vpdstage, &preferences},
    {"wNTC", DataType::FLOAT, Data(0.0f), &wNTC, &preferences},
    {"wEC", DataType::FLOAT, Data(0.0f), &wEC, &preferences},
    {"R2p", DataType::FLOAT, Data(0.0f), &R2p, &preferences},
    {"R2n", DataType::FLOAT, Data(0.0f), &R2n, &preferences},
    {"A1", DataType::FLOAT, Data(0.0f), &A1, &preferences},
    {"A2", DataType::FLOAT, Data(0.0f), &A2, &preferences},
    {"R1", DataType::FLOAT, Data(0.0f), &R1, &preferences},
    {"Rx1", DataType::FLOAT, Data(0.0f), &Rx1, &preferences},
    {"Rx2", DataType::FLOAT, Data(0.0f), &Rx2, &preferences},
    {"Dr", DataType::FLOAT, Data(0.0f), &Dr, &preferences},
    {"py1", DataType::FLOAT, Data(0.0f), &py1, &preferences},
    {"py2", DataType::FLOAT, Data(0.0f), &py2, &preferences},
    {"py3", DataType::FLOAT, Data(0.0f), &py3, &preferences},
    {"px1", DataType::FLOAT, Data(0.0f), &px1, &preferences},
    {"px2", DataType::FLOAT, Data(0.0f), &px2, &preferences},
    {"px3", DataType::FLOAT, Data(0.0f), &px3, &preferences},
    {"ec1", DataType::FLOAT, Data(0.0f), &ec1, &preferences},
    {"ec2", DataType::FLOAT, Data(0.0f), &ec2, &preferences},
    {"ex1", DataType::FLOAT, Data(0.0f), &ex1, &preferences},
    {"ex2", DataType::FLOAT, Data(0.0f), &ex2, &preferences},
    {"pR_raw_p1", DataType::FLOAT, Data(0.0f), &pR_raw_p1, &preferences},
    {"pR_raw_p2", DataType::FLOAT, Data(0.0f), &pR_raw_p2, &preferences},
    {"pR_raw_p3", DataType::FLOAT, Data(0.0f), &pR_raw_p3, &preferences},
    {"pR_val_p1", DataType::FLOAT, Data(0.0f), &pR_val_p1, &preferences},
    {"pR_val_p2", DataType::FLOAT, Data(0.0f), &pR_val_p2, &preferences},
    {"pR_val_p3", DataType::FLOAT, Data(0.0f), &pR_val_p3, &preferences},
    {"pR_Rx", DataType::FLOAT, Data(0.0f), &pR_Rx, &preferences},
    {"pR_T", DataType::FLOAT, Data(0.0f), &pR_T, &preferences},
    {"pR_x", DataType::FLOAT, Data(0.0f), &pR_x, &preferences},
    {"pR_DAC", DataType::FLOAT, Data(0.0f), &pR_DAC, &preferences},
    {"pR1", DataType::FLOAT, Data(0.0f), &pR1, &preferences},
    {"pR_type", DataType::STRING, {""}, &pR_type, &preferences},
    {"eckorr", DataType::FLOAT, Data(0.0f), &eckorr, &preferences},
    {"kt", DataType::FLOAT, Data(0.0f), &kt, &preferences},
    {"pH_lkorr", DataType::FLOAT, Data(0.0f), &pH_lkorr, &preferences},
    {"A1name", DataType::STRING, {"A1"}, &A1name, &preferences},
    {"A2name", DataType::STRING, {"A2"}, &A2name, &preferences},
    {"max_l_level", DataType::FLOAT, Data(0.0f), &max_l_level, &preferences},
    {"max_l_raw", DataType::FLOAT, Data(0.0f), &max_l_raw, &preferences},
    {"min_l_level", DataType::FLOAT, Data(0.0f), &min_l_level, &preferences},
    {"min_l_raw", DataType::FLOAT, Data(0.0f), &min_l_raw, &preferences},
    {"server_make_update", DataType::BOOLEAN, {false}, &server_make_update, &preferences},
    {"old_ec", DataType::INTEGER, {0}, &old_ec, &preferences},
    {"DRV1_A_PK_On", DataType::INTEGER, {0}, &DRV1_A_PK_On, &preferences},
    {"DRV1_B_PK_On", DataType::INTEGER, {0}, &DRV1_B_PK_On, &preferences},
    {"DRV1_C_PK_On", DataType::INTEGER, {0}, &DRV1_C_PK_On, &preferences},
    {"DRV1_D_PK_On", DataType::INTEGER, {0}, &DRV1_D_PK_On, &preferences},
    {"DRV2_A_PK_On", DataType::INTEGER, {0}, &DRV2_A_PK_On, &preferences},
    {"DRV2_B_PK_On", DataType::INTEGER, {0}, &DRV2_B_PK_On, &preferences},
    {"DRV2_C_PK_On", DataType::INTEGER, {0}, &DRV2_C_PK_On, &preferences},
    {"DRV2_D_PK_On", DataType::INTEGER, {0}, &DRV2_D_PK_On, &preferences},
    {"DRV3_A_PK_On", DataType::INTEGER, {0}, &DRV3_A_PK_On, &preferences},
    {"DRV3_B_PK_On", DataType::INTEGER, {0}, &DRV3_B_PK_On, &preferences},
    {"DRV3_C_PK_On", DataType::INTEGER, {0}, &DRV3_C_PK_On, &preferences},
    {"DRV3_D_PK_On", DataType::INTEGER, {0}, &DRV3_D_PK_On, &preferences},
    {"DRV4_A_PK_On", DataType::INTEGER, {0}, &DRV4_A_PK_On, &preferences},
    {"DRV4_B_PK_On", DataType::INTEGER, {0}, &DRV4_B_PK_On, &preferences},
    {"DRV4_C_PK_On", DataType::INTEGER, {0}, &DRV4_C_PK_On, &preferences},
    {"DRV4_D_PK_On", DataType::INTEGER, {0}, &DRV4_D_PK_On, &preferences},
    {"ECStabEnable", DataType::INTEGER, {0}, &ECStabEnable, &config_preferences},
    {"ECStabValue", DataType::FLOAT, {2.5f}, &ECStabValue, &config_preferences},
    {"ECStabPomp", DataType::INTEGER, {0}, &ECStabPomp, &config_preferences},
    {"ECStabTime", DataType::INTEGER, {20}, &ECStabTime, &config_preferences},
    {"ECStabInterval", DataType::INTEGER, {180}, &ECStabInterval, &config_preferences},
    {"ECStabMinDist", DataType::FLOAT, {5.0f}, &ECStabMinDist, &config_preferences},
    {"ECStabMaxDist", DataType::FLOAT, {50.0f}, &ECStabMaxDist, &config_preferences},
    {"SelectedRP", DataType::INTEGER, {-1}, &SelectedRP, &config_preferences},
    {"RootPomp", DataType::INTEGER, {-1}, &RootPomp, &config_preferences},
    {"RootPwdOn", DataType::INTEGER, {1}, &RootPwdOn, &config_preferences},
    {"RootPwdMax", DataType::INTEGER, {254}, &RootPwdMax, &config_preferences},
    {"RootPwdMin", DataType::INTEGER, {0}, &RootPwdMin, &config_preferences},
    {"RootDistMin", DataType::INTEGER, {0}, &RootDistMin, &config_preferences},
    {"PwdStepUp", DataType::INTEGER, {1}, &PwdStepUp, &config_preferences},
    {"PwdStepDown", DataType::INTEGER, {10}, &PwdStepDown, &config_preferences},
    {"RootPwdTemp", DataType::INTEGER, {0}, &RootPwdTemp, &config_preferences},
    {"PwdNtcRoot", DataType::FLOAT, {-1.0f}, &PwdNtcRoot, &config_preferences},
    {"PwdDifPR", DataType::INTEGER, {0}, &PwdDifPR, &config_preferences},
    {"MinKickPWD", DataType::INTEGER, {150}, &MinKickPWD, &config_preferences},
    {"PompNightEnable", DataType::INTEGER, {0}, &PompNightEnable, &config_preferences},
    {"PompNightPomp", DataType::INTEGER, {0}, &PompNightPomp, &config_preferences},
    {"MinLightLevel", DataType::INTEGER, {10}, &MinLightLevel, &config_preferences},
    {"e_ha", DataType::INTEGER, {0}, &e_ha, &config_preferences},
    {"port_ha", DataType::INTEGER, {1883}, &port_ha, &config_preferences},
    {"a_ha", DataType::STRING, {"192.168.1.157"}, &a_ha, &config_preferences},
    {"u_ha", DataType::STRING, {""}, &u_ha, &config_preferences},
    {"p_ha", DataType::STRING, {""}, &p_ha, &config_preferences},
    {"calE", DataType::INTEGER, {0}, &calE, &config_preferences},
    {"change_pins", DataType::INTEGER, {0}, &change_pins, &config_preferences},
    {"clear_pref", DataType::INTEGER, {0}, &clear_pref, &config_preferences},
    {"SetPumpA_Ml", DataType::FLOAT, {0.0f}, &SetPumpA_Ml, &config_preferences},
    {"StPumpA_Del", DataType::INTEGER, {700}, &StPumpA_Del, &config_preferences},
    {"StPumpA_Ret", DataType::INTEGER, {700}, &StPumpA_Ret, &config_preferences},
    {"StPumpA_On", DataType::INTEGER, {0}, &StPumpA_On, &config_preferences},
    {"StPumpA_cStepMl", DataType::FLOAT, {1000.0f}, &StPumpA_cStepMl, &config_preferences},
    {"StPumpA_cMl", DataType::FLOAT, {1.0f}, &StPumpA_cMl, &config_preferences},
    {"StPumpA_cStep", DataType::FLOAT, {1000.0f}, &StPumpA_cStep, &config_preferences},
    {"StPumpA_A", DataType::INTEGER, {4}, &StPumpA_A, &config_preferences},
    {"StPumpA_B", DataType::INTEGER, {5}, &StPumpA_B, &config_preferences},
    {"StPumpA_C", DataType::INTEGER, {6}, &StPumpA_C, &config_preferences},
    {"StPumpA_D", DataType::INTEGER, {7}, &StPumpA_D, &config_preferences},
    {"SetPumpB_Ml", DataType::FLOAT, {0.0f}, &SetPumpB_Ml, &config_preferences},
    {"StPumpB_Del", DataType::INTEGER, {700}, &StPumpB_Del, &config_preferences},
    {"StPumpB_Ret", DataType::INTEGER, {700}, &StPumpB_Ret, &config_preferences},
    {"StPumpB_On", DataType::INTEGER, {0}, &StPumpB_On, &config_preferences},
    {"StPumpB_cStepMl", DataType::FLOAT, {1000.0f}, &StPumpB_cStepMl, &config_preferences},
    {"StPumpB_cMl", DataType::FLOAT, {1.0f}, &StPumpB_cMl, &config_preferences},
    {"StPumpB_cStep", DataType::FLOAT, {1000.0f}, &StPumpB_cStep, &config_preferences},
    {"StPumpB_A", DataType::INTEGER, {8}, &StPumpB_A, &config_preferences},
    {"StPumpB_B", DataType::INTEGER, {9}, &StPumpB_B, &config_preferences},
    {"StPumpB_C", DataType::INTEGER, {10}, &StPumpB_C, &config_preferences},
    {"StPumpB_D", DataType::INTEGER, {11}, &StPumpB_D, &config_preferences},
    {"PWDport1", DataType::INTEGER, {16}, &PWDport1, &config_preferences},
    {"PWD1", DataType::INTEGER, {255}, &PWD1, &config_preferences},
    {"FREQ1", DataType::INTEGER, {5000}, &FREQ1, &config_preferences},
    {"DRV1_A_State", DataType::INTEGER, {0}, &DRV1_A_State, &config_preferences},
    {"DRV1_B_State", DataType::INTEGER, {0}, &DRV1_B_State, &config_preferences},
    {"DRV2_A_State", DataType::INTEGER, {0}, &DRV2_A_State, &config_preferences},
    {"DRV2_B_State", DataType::INTEGER, {0}, &DRV2_B_State, &config_preferences},
    {"DRV3_A_State", DataType::INTEGER, {0}, &DRV3_A_State, &config_preferences},
    {"DRV3_B_State", DataType::INTEGER, {0}, &DRV3_B_State, &config_preferences},
    {"DRV4_A_State", DataType::INTEGER, {0}, &DRV4_A_State, &config_preferences},
    {"DRV4_B_State", DataType::INTEGER, {0}, &DRV4_B_State, &config_preferences},
    {"PWDport2", DataType::INTEGER, {17}, &PWDport2, &config_preferences},
    {"PWD2", DataType::INTEGER, {255}, &PWD2, &config_preferences},
    {"FREQ2", DataType::INTEGER, {5000}, &FREQ2, &config_preferences},
    {"DRV1_C_State", DataType::INTEGER, {0}, &DRV1_C_State, &config_preferences},
    {"DRV1_D_State", DataType::INTEGER, {0}, &DRV1_D_State, &config_preferences},
    {"DRV2_C_State", DataType::INTEGER, {0}, &DRV2_C_State, &config_preferences},
    {"DRV2_D_State", DataType::INTEGER, {0}, &DRV2_D_State, &config_preferences},
    {"DRV3_C_State", DataType::INTEGER, {0}, &DRV3_C_State, &config_preferences},
    {"DRV3_D_State", DataType::INTEGER, {0}, &DRV3_D_State, &config_preferences},
    {"DRV4_C_State", DataType::INTEGER, {0}, &DRV4_C_State, &config_preferences},
    {"DRV4_D_State", DataType::INTEGER, {0}, &DRV4_D_State, &config_preferences},
    {"KickUpMax", DataType::INTEGER, {255}, &KickUpMax, &config_preferences},
    {"KickUpStrart", DataType::INTEGER, {10}, &KickUpStrart, &config_preferences},
    {"KickUpTime", DataType::INTEGER, {300}, &KickUpTime, &config_preferences},
    {"KickOnce", DataType::INTEGER, {0}, &KickOnce, &config_preferences},

    {"RootTempAddress", DataType::STRING, "28:FF:1C:30:63:17:03:B1", &rootTempAddressString, &config_preferences},
    {"WNTCAddress", DataType::STRING, "28:FF:2E:31:63:17:04:C2", &WNTCAddressString, &config_preferences},

    {"SetPumpA_Ml_SUM", DataType::FLOAT, {0.0f}, &SetPumpA_Ml_SUM, &config_preferences},
    {"SetPumpB_Ml_SUM", DataType::FLOAT, {0.0f}, &SetPumpB_Ml_SUM, &config_preferences},
    {"PumpA_Step_SUM", DataType::FLOAT, {0.0f}, &PumpA_Step_SUM, &config_preferences},
    {"PumpB_Step_SUM", DataType::FLOAT, {0.0f}, &PumpB_Step_SUM, &config_preferences},

};

const int preferencesCount = sizeof(preferencesArray) / sizeof(preferencesArray[0]);

PreferenceItem *findPreferenceByKey(const char *key)
{
    if (key == nullptr) return nullptr;
    for (int i = 0; i < preferencesCount; i++)
    {
        if (strcmp(preferencesArray[i].key, key) == 0)
        {
            return &preferencesArray[i];
        }
    }
    return nullptr;
}

void loadPreferences()
{
    for (int i = 0; i < sizeof(preferencesArray) / sizeof(PreferenceItem); ++i)
    {
        switch (preferencesArray[i].type)
        {
            case DataType::FLOAT:
                *(float *)preferencesArray[i].variable = preferencesArray[i].preferences->getFloat(
                    preferencesArray[i].key, preferencesArray[i].defaultValue.value.fValue);
                break;
            case DataType::STRING:
                *(String *)preferencesArray[i].variable = preferencesArray[i].preferences->getString(
                    preferencesArray[i].key, preferencesArray[i].defaultValue.value.sValue);
                break;
            case DataType::BOOLEAN:
                *(bool *)preferencesArray[i].variable = preferencesArray[i].preferences->getBool(
                    preferencesArray[i].key, preferencesArray[i].defaultValue.value.bValue);
                break;
            case DataType::INTEGER:
                *(int *)preferencesArray[i].variable = preferencesArray[i].preferences->getInt(
                    preferencesArray[i].key, preferencesArray[i].defaultValue.value.iValue);
                break;
        }
    }
}
