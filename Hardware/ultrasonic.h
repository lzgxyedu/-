#ifndef __ULTRASONIC_H
#define	__ULTRASONIC_H
#include "stm32f10x.h"
#include "adcx.h"
#include "Delay.h"
#include "math.h"

/*****************???????******************
											STM32
 * ??			:	HC-SR04??????h??                   
 * ??			: V1.0
 * ??			: 2024.8.27
 * MCU			:	STM32F103C8T6
 * ??			:	???							
 * BILIBILI	:	???????
 * CSDN			:	???????
 * ??			:	??

**********************BEGIN***********************/


/***************????????****************/
// ULTRASONIC GPIO???

#define		ULTRASONIC_GPIO_CLK								RCC_APB2Periph_GPIOA
#define 	ULTRASONIC_GPIO_PORT							GPIOA
#define 	ULTRASONIC_TRIG_GPIO_PIN					GPIO_Pin_8	
#define 	ULTRASONIC_ECHO_GPIO_PIN					GPIO_Pin_9	

#define 	TRIG_Send  PAout(8)
#define 	ECHO_Reci  PAin(9)

/*********************END**********************/

void Ultrasonic_Init(void);
float UltrasonicGetLength(void);

void OpenTimerForHc(void);
void CloseTimerForHc(void); 
u32 GetEchoTimer(void);

#endif /* __ULTRASONIC_H */
