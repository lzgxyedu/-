// CountSensor.h
#ifndef __COUNT_SENSOR_H
#define __COUNT_SENSOR_H

#include <stdint.h>

void CountSensor_Init(void);
uint16_t CountSensor_Get(void);
uint32_t CountSensor_GetLastPulseTime(void); 
void CountSensor_Reset(void);   

#endif
