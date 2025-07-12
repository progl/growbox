#define NTC_port ADC1_CHANNEL_4  // gpio32
unsigned long NTC_old = millis();
unsigned long NTC_Repeat = 1000;
int ntc_daly_ms = 50;
int NTC_MiddleCount = 5000;
int ntc_avg_number = 100;
int disable_ntc = 0;
int first_time_ntc = 1;
RunningMedian NTCRM = RunningMedian((NTC_MiddleCount / ntc_avg_number) * 1.5);
RunningMedian NTCRM_AVG = RunningMedian(ntc_avg_number / 10);
float NTC_Kalman;
float ntc_mea_e = 20;
float ntc_est_e = 20;
float ntc_q = 0.1;
GKalman KalmanNTC(ntc_mea_e, ntc_est_e, ntc_q);

boolean first_ntc = true;
unsigned long NTC_LastTime = millis() - NTC_old;
