
uint8_t VL6180Xaddr = 0x29;

VL6180X s_vl6180X;

RunningMedian VL6180X_RangeRM = RunningMedian(4);
RunningMedian VL6180X_RangeAVG = RunningMedian(4);

int vl6180XScalling = 1;
