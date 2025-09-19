/*==============================================================================
  ??:Key.h
  ??:????????(PB5/PB6/PB7/PB8)
==============================================================================*/
#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

#define KEY_NONE    0
#define KEY1_PRES   1   // ??? / ??+ - PB5
#define KEY2_PRES   2   // ??? / ??- - PB6
#define KEY3_PRES   3   // ??   / ???? - PB7
#define KEY4_PRES   4   // ???? - PB8

void Key_Init(void);
uint8_t Key_GetNum(void);
uint8_t Key_GetState(uint8_t key_num);

#endif // __KEY_H
