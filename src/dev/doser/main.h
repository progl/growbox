
long lastOffDoser;

int AOn = 1;
int ADel = 100;
int ARet = 100;

int AA = 4;
int AB = 5;
int AC = 6;
int AD = 7;

int BOn = 1;
int BDel = 700;
int BRet = 700;

int BA = 8;
int BB = 9;
int BC = 10;
int BD = 11;

long del = 100;
long ret = 100;
int stap = 1;

float StPumpA_cStepMl = 1000;  // Число шагов на объем для калибровки
float StPumpA_cMl = 1;         // Объем в мл для калибровки
float StPumpA_cStep = 1000;    // Число шагов за 1 цикл
float StPumpB_cStepMl = 1000;
float StPumpB_cMl = 1;
float StPumpB_cStep = 1000;

float SetPumpA_Ml = 0;  // Сколько налить мл
float SetPumpB_Ml = 0;  // Сколько налить мл

#include <dev/doser/func.h>
