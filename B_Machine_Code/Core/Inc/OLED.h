#ifndef __OLED_H
#define __OLED_H

#include "main.h"

//-----------------OLED端口定义----------------  
#define OLED_SCL_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define OLED_SCL_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)

#define OLED_SDA_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define OLED_SDA_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)

// OLED控制用函数
void OLED_WR_Byte(uint8_t dat, uint8_t cmd);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr);
void OLED_ShowString(uint8_t x, uint8_t y, char* p);

#endif

