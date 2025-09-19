/*==============================================================================
  ??:KeyEXTI.h
  ??:?????????????????
==============================================================================*/
#ifndef __KEYEXTI_H
#define __KEYEXTI_H

#include "stm32f10x.h"

// ???????(???Key.h)
#define KEY_NONE    0
#define KEY1_PRES   1
#define KEY2_PRES   2
#define KEY3_PRES   3
#define KEY4_PRES   4

// ????
void KeyEXTI_Init(void);                    // ???
void KeyEXTI_TimerInit(void);               // ??????(????)

// ??????
uint8_t KeyEXTI_GetKey(void);               // ??????(??)
uint8_t KeyEXTI_GetKeyState(uint8_t key_num); // ????????
uint8_t KeyEXTI_GetLongPress(uint8_t key_num); // ??????
uint8_t KeyEXTI_GetRepeat(uint8_t key_num);    // ????????
uint16_t KeyEXTI_GetPressTime(uint8_t key_num); // ??????
void KeyEXTI_ResetKey(uint8_t key_num);         // ??????

// ????
uint32_t GetTick(void);                     // ??????(ms)

// ?????
uint8_t KeyEXTI_GetKey1LongPressFlag(void);
uint8_t KeyEXTI_GetKey4LongPressFlag(void);
void KeyEXTI_ResetKey1LongPressFlag(void);
void KeyEXTI_ResetKey4LongPressFlag(void);

#endif /* __KEYEXTI_H */
