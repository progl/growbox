#include "GyverFilters.h"
#include "RunningMedian.h"
#include <map>
#include <Preferences.h>

extern Preferences preferences;
extern String fFTS(float x, byte precision);
struct Param
{
    const char *name;
    const char *label;
    enum Type
    {
        INT,
        FLOAT,
        STRING
    } type;
    union
    {
        int defaultInt;
        float defaultFloat;
        const char *defaultString;
    };
};
String vpdstage = "";
String stages[] = {"Start", "Vega", "Fruit"};
struct Group
{
    const char *caption;
    int numParams;  // Количество действительных параметров
    Param params[40];
};
int sensorCount;
struct PreferenceSetting
{
    const char *key;
    int defaultValue;
};
int SelectedRP, RootPomp;
int PompNightPomp, PompNightEnable;
int MinLightLevel = 10;
String detected_sensors = "";
String not_detected_sensors = "";
int NTC_KAL_E, EC_KAL_E, DIST_KAL_E = 0;
int DEFAULT_DELAY = 35000;
int LOOP_DEFAULT_DELAY = 10;
String preferences_prefix = "set/";
String data_prefix = "data-calibrate/";
String timescale_prefix = "data-timescale/";
String main_prefix = "main/";
uint8_t qos = 0;
uint8_t qos_publish = 0;
int mqtt_not_connected_counter = 0;
int con_c;

int is_setup = 1;
float t_us;
float ec_notermo;
String pR_type;
float pR_raw_p1, pR_raw_p2, pR_raw_p3;
float pR_val_p1, pR_val_p2, pR_val_p3;
float pR_Rx, pR_T, pR_x, pR_DAC, pR1;
float wPR;
bool first_time = true;

int RDDelayOn = 60;    // Задержка в секундах для включения
int RDDelayOff = 300;  // Задержка в секундах для выключения
int RDEnable = 0;
int RDPWD = 256;
int RDWorkNight = 1;
int RDSelectedRP = 0;

int KickOnce = 0;       // Пинок для насоса
int KickUpMax = 255;    // максимум мощности пинка
int KickUpStrart = 10;  // начальная можность пинка
int KickUpTime = 300;   // Время пинка в миллисекундах
const char *pref_reset_reason = "ResetReason";
boolean log_debug = false;
float EC_Kalman, ec_notermo_kalman;
float ec_mea_e = 3;
float ec_est_e = 3;
float ec_q = 0.1;
GKalman KalmanEC(ec_mea_e, ec_est_e, ec_q);
GKalman KalmanEcUsual(ec_mea_e, ec_est_e, ec_q);
boolean first_EC = true;
const int DRV_COUNT = 4;
const int BITS_PER_DRV = 4;
String DRV_Keys[DRV_COUNT][BITS_PER_DRV] = {{"DRV1_A_State", "DRV1_B_State", "DRV1_C_State", "DRV1_D_State"},
                                            {"DRV2_A_State", "DRV2_B_State", "DRV2_C_State", "DRV2_D_State"},
                                            {"DRV3_A_State", "DRV3_B_State", "DRV3_C_State", "DRV3_D_State"},
                                            {"DRV4_A_State", "DRV4_B_State", "DRV4_C_State", "DRV4_D_State"}};

String DRV_PK_On_Keys[DRV_COUNT][BITS_PER_DRV] = {{"DRV1_A_PK_On", "DRV1_B_PK_On", "DRV1_C_PK_On", "DRV1_D_PK_On"},
                                                  {"DRV2_A_PK_On", "DRV2_B_PK_On", "DRV2_C_PK_On", "DRV2_D_PK_On"},
                                                  {"DRV3_A_PK_On", "DRV3_B_PK_On", "DRV3_C_PK_On", "DRV3_D_PK_On"},
                                                  {"DRV4_A_PK_On", "DRV4_B_PK_On", "DRV4_C_PK_On", "DRV4_D_PK_On"}};

int DRV1_A_PK_On = 0;
int DRV1_B_PK_On = 0;
int DRV1_C_PK_On = 0;
int DRV1_D_PK_On = 0;
int DRV2_A_PK_On = 0;
int DRV2_B_PK_On = 0;
int DRV2_C_PK_On = 0;
int DRV2_D_PK_On = 0;
int DRV3_A_PK_On = 0;
int DRV3_B_PK_On = 0;
int DRV3_C_PK_On = 0;
int DRV3_D_PK_On = 0;
int DRV4_A_PK_On = 0;
int DRV4_B_PK_On = 0;
int DRV4_C_PK_On = 0;
int DRV4_D_PK_On = 0;

unsigned long NextRootDrivePwdOn;   // Сохраняем текущее время для включения
unsigned long NextRootDrivePwdOff;  // Время для выключения

int mqttHAConnected = 0;

int failedCount = 0;
static _lock_t sar_adc1_lock;
#define SAR_ADC1_LOCK_ACQUIRE() _lock_acquire(&sar_adc1_lock)
#define SAR_ADC1_LOCK_RELEASE() _lock_release(&sar_adc1_lock)

portMUX_TYPE adc_reg_lock = portMUX_INITIALIZER_UNLOCKED;
#define ADC_REG_LOCK_ENTER() portENTER_CRITICAL(&adc_reg_lock)
#define ADC_REG_LOCK_EXIT() portEXIT_CRITICAL(&adc_reg_lock)

RunningMedian PhRM = RunningMedian(4);
RunningMedian AirTempRM = RunningMedian(4);
RunningMedian AirHumRM = RunningMedian(4);
RunningMedian AirPressRM = RunningMedian(4);
RunningMedian PRRM = RunningMedian(4);

PreferenceSetting preferenceSettings[] = {{"PWDport1", 16}, {"PWD1", 0}, {"FREQ1", 5000},
                                          {"PWDport2", 17}, {"PWD2", 0}, {"FREQ2", 5000}};

String options[] = {"DRV1_A", "DRV1_B", "DRV1_C", "DRV1_D", "DRV2_A", "DRV2_B", "DRV2_C", "DRV2_D",
                    "DRV3_A", "DRV3_B", "DRV3_C", "DRV3_D", "DRV4_A", "DRV4_B", "DRV4_C", "DRV4_D"};

String escapeHTML(String input)
{
    String output = "";

    for (int i = 0; i < input.length(); i++)
    {
        char c = input[i];
        switch (c)
        {
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            case '&':
                output += "&amp;";
                break;
            case '"':
                output += "&quot;";
                break;
            case '\'':
                output += "&#39;";
                break;
            default:
                output += c;
                break;
        }
    }

    return output;
}
String generateTableRow(const Param &p)
{
    String value;
    switch (p.type)
    {
        case Param::INT:
            value = String(preferences.getInt(p.name, p.defaultInt));
            break;
        case Param::FLOAT:
            value = fFTS(preferences.getFloat(p.name, p.defaultFloat), 3);
            break;
        case Param::STRING:
            value = preferences.getString(p.name, p.defaultString);
            break;
    }

    String row = "";
    if (p.name == "ECStabPomp" || p.name == "SelectedRP" || p.name == "PompNightPomp" || p.name == "RDSelectedRP" ||
        p.name == "StPumpA_A" || p.name == "StPumpA_B" || p.name == "StPumpA_C" || p.name == "StPumpA_D" ||
        p.name == "StPumpB_A" || p.name == "StPumpB_B" || p.name == "StPumpB_C" || p.name == "StPumpB_D")
    {
        row += "<tr><td>" + escapeHTML(String(p.label)) + "\n<td><select name='" + escapeHTML(p.name) + "'>";
        for (int i = 0; i < sizeof(options) / sizeof(options[0]); i++)
        {
            row += "<option value='" + String(i) + "'";
            if (p.type == Param::INT && value.toInt() == i)
            {
                row += " selected";
            }
            else if (p.type == Param::FLOAT && value.toFloat() == i)
            {
                row += " selected";
            }
            else if (p.type == Param::STRING && value == options[i])
            {
                row += " selected";
            }
            row += ">" + escapeHTML(options[i]) + "</option>";
        }
        row += "</select form='set'> ";
        row += "</tr>\n";
    }
    else
    {
        if (p.name == "vpdstage")
        {
            row += "<tr><td>" + escapeHTML(String(p.label)) + "\n<td><select name='" + escapeHTML(p.name) + "'>";

            for (int j = 0; j < sizeof(stages) / sizeof(stages[0]); ++j)
            {
                row += "<option value='" + escapeHTML(stages[j]) + "'";
                if (value == vpdstage)
                {
                    row += " selected";
                }
                row += ">" + escapeHTML(stages[j]) + "</option>";
            }

            row += "</select></td></tr>\n";
        }
        else
        {
            row = "<tr><td>" + escapeHTML(String(p.label)) + "\n<td><input type='text' name='" +
                  escapeHTML(String(p.name)) + "' value='" + escapeHTML(value) + "' form='set'></tr>\n";
        }
    }
    return row;
}

// Переменные
float AirTemp, AirHum, AirPress, RootTemp, hall, pHmV, pHraw, NTC_RAW, Ap, An, Dist, DstRAW, CPUTemp, CO2, tVOC, eRAW,
    Vcc, wNTC, wEC_ususal, wR2, wEC, wpH, PR, EC_R1, EC_R2_p1, EC_R2_p2, Kornevoe, wSalt, AirVPD, vZapas;
bool OtaStart, ECwork, USwork = false;
bool RootTempFound;
uint16_t readGPIO;
int PWD1, PWD2, totalLength;
long ECStabOn;
int currentLength = 0;  // current size of written firmware
int ECStabEnable, ECStabPomp, ECStabTime, ECStabInterval;
float ECStabValue, ECStabMinDist, ECStabMaxDist;
float tR_DAC, restart, tR_B, tR_val_korr, A1, A2, R1, Rx1, Rx2, Dr, R2p, R2n, py1, py2, py3, px1, px2, px3, pH_lkorr,
    ec1, ec2, ex1, ex2, eckorr, kt;
float max_l_level, max_l_raw, min_l_level, min_l_raw, wLevel, twLevel;

String tR_type = "direct";

String wegareply, err_wegaapi_json, dt, Reset_reason0, Reset_reason1;
struct Sensor
{
    char name[50];     // Название датчика
    uint8_t detected;  // Статус детектирования: 0 - не детектирован, 1 - детектирован
};

Sensor sensors[] = {
    {"VL53L0X", 0}, {"MCP23017", 0}, {"HX710B", 0}, {"Dallas", 0},  {"US025", 0},   {"AHT10", 0},
    {"AM2320", 0},  {"EC", 0},       {"PR", 0},     {"ADS1115", 0}, {"MCP3421", 0}, {"VL6180X", 0},
    {"SDC30", 0},   {"NTC", 0},      {"LCD", 0},    {"BMx280", 0},  {"CCS811", 0},
};
int sensorsCount = sizeof(sensors) / sizeof(sensors[0]);

// Функция для изменения статуса детектирования датчика по имени
void setSensorDetected(const char *name, uint8_t detected)
{
    for (int i = 0; i < sensorsCount; ++i)
    {
        if (strcmp(sensors[i].name, name) == 0)
        {
            sensors[i].detected = detected;
            break;
        }
    }
}

bool making_update = false;

long ECDoserTimer;
boolean make_doser = false;
#define SYSLOG_PORT 514  // Порт SYSLOG сервера
String SYSLOG_SERVER = "";
// ponics.online

IPAddress MQTT_HOST(127, 0, 0, 1);
int MQTT_PORT = 1883;
int enable_ponics_online = 1;
int enable_ponics_online_logs = 1;
String mqtt_mqtt_user;
String mqtt_mqtt_password;
uint16_t mqtt_mqtt_port = 1883;

// Переменные для параметров "calE"
int calE = 0;

// Переменные для "Doser Pump A"
int StPumpA_Del = 700;
int StPumpA_Ret = 700;
int StPumpA_On = 0;
int StPumpA_A = 4;
int StPumpA_B = 5;
int StPumpA_C = 6;
int StPumpA_D = 7;

// Переменные для "Doser Pump B"
int StPumpB_Del = 700;
int StPumpB_Ret = 700;
int StPumpB_On = 0;
int StPumpB_A = 8;
int StPumpB_B = 9;
int StPumpB_C = 10;
int StPumpB_D = 11;

int DRV1_A_State = 0;
int DRV1_B_State = 0;
int DRV2_A_State = 0;
int DRV2_B_State = 0;
int DRV3_A_State = 0;
int DRV3_B_State = 0;
int DRV4_A_State = 0;
int DRV4_B_State = 0;

int PWDport2 = 17;
int FREQ1 = 0;
int PWDport1 = 16;
int FREQ2 = 0;
int DRV1_C_State = 0;
int DRV1_D_State = 0;
int DRV2_C_State = 0;
int DRV2_D_State = 0;
int DRV3_C_State = 0;
int DRV3_D_State = 0;
int DRV4_C_State = 0;
int DRV4_D_State = 0;

float VccRaw = 0;

std::map<std::string, int *> variablePointers;

// Инициализация словаря указателями на переменные
void initializeVariablePointers()
{
    variablePointers["DRV1_A_State"] = &DRV1_A_State;
    variablePointers["DRV1_B_State"] = &DRV1_B_State;
    variablePointers["DRV1_C_State"] = &DRV1_C_State;
    variablePointers["DRV1_D_State"] = &DRV1_D_State;

    variablePointers["DRV2_A_State"] = &DRV2_A_State;
    variablePointers["DRV2_B_State"] = &DRV2_B_State;
    variablePointers["DRV2_C_State"] = &DRV2_C_State;
    variablePointers["DRV2_D_State"] = &DRV2_D_State;

    variablePointers["DRV3_A_State"] = &DRV3_A_State;
    variablePointers["DRV3_B_State"] = &DRV3_B_State;
    variablePointers["DRV3_C_State"] = &DRV3_C_State;
    variablePointers["DRV3_D_State"] = &DRV3_D_State;

    variablePointers["DRV4_A_State"] = &DRV4_A_State;
    variablePointers["DRV4_B_State"] = &DRV4_B_State;
    variablePointers["DRV4_C_State"] = &DRV4_C_State;
    variablePointers["DRV4_D_State"] = &DRV4_D_State;
    variablePointers["PWD1"] = &PWD1;
    variablePointers["PWD2"] = &PWD2;
    variablePointers["FREQ1"] = &FREQ1;
    variablePointers["FREQ2"] = &FREQ2;
    variablePointers["KickOnce"] = &KickOnce;

    variablePointers["DRV1_A_PK_On"] = &DRV1_A_PK_On;
    variablePointers["DRV1_B_PK_On"] = &DRV1_B_PK_On;
    variablePointers["DRV1_C_PK_On"] = &DRV1_C_PK_On;
    variablePointers["DRV1_D_PK_On"] = &DRV1_D_PK_On;
    variablePointers["DRV2_A_PK_On"] = &DRV2_A_PK_On;
    variablePointers["DRV2_B_PK_On"] = &DRV2_B_PK_On;
    variablePointers["DRV2_C_PK_On"] = &DRV2_C_PK_On;
    variablePointers["DRV2_D_PK_On"] = &DRV2_D_PK_On;
    variablePointers["DRV3_A_PK_On"] = &DRV3_A_PK_On;
    variablePointers["DRV3_B_PK_On"] = &DRV3_B_PK_On;
    variablePointers["DRV3_C_PK_On"] = &DRV3_C_PK_On;
    variablePointers["DRV3_D_PK_On"] = &DRV3_D_PK_On;
    variablePointers["DRV4_A_PK_On"] = &DRV4_A_PK_On;
    variablePointers["DRV4_B_PK_On"] = &DRV4_B_PK_On;
    variablePointers["DRV4_C_PK_On"] = &DRV4_C_PK_On;
    variablePointers["DRV4_D_PK_On"] = &DRV4_D_PK_On;
}

// Telegram
String tg_bt = "";   // бот токен
String tg_cid = "";  // chat id

// wPh
bool tg_ph_en = true;
float tg_ph_min = 5.5f;
float tg_ph_max = 6.5f;

// wEc
bool tg_ec_en = true;
float tg_ec_min = 1.0f;
float tg_ec_max = 2.0f;

// wNTC
bool tg_ntc_en = true;
float tg_ntc_min = 15.0f;
float tg_ntc_max = 40.0f;

// RootTemp
bool tg_rt_en = true;
float tg_rt_min = 18.0f;
float tg_rt_max = 24.0f;

// AirTemp
bool tg_at_en = true;
float tg_at_min = 18.0f;
float tg_at_max = 30.0f;