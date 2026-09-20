/* oled.h - SSD1306 128x64 OLED driver, software I2C on PB8/PB9 (open-drain) */
#ifndef OLED_H
#define OLED_H

#include "stm32f1xx_hal.h"
#include "oled_font.h"      /* 提供 CN_* 中文点阵索引枚举 */

/* 引脚定义: 与 .ioc 的 OLED_SCL(PB8) / OLED_SDA(PB9) 一致, 外部 4.7K 上拉到 3.3V */
#define OLED_SCL_GPIO   GPIOB
#define OLED_SCL_PIN    GPIO_PIN_6
#define OLED_SDA_GPIO   GPIOB
#define OLED_SDA_PIN    GPIO_PIN_7

#define OLED_I2C_ADDR   0x78U   /* SSD1306 SA0=0 的写地址 */

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);                       /* 把整个帧缓冲刷到屏幕 */

void OLED_PutASCII(uint8_t x, uint8_t page, char c);   /* 8x16, page=0..6 */
void OLED_Print(uint8_t x, uint8_t page, const char *s);
void OLED_PutCN(uint8_t idx, uint8_t x, uint8_t page); /* 16x16 中文点阵 */
void OLED_PrintU(uint8_t x, uint8_t page, uint32_t v); /* 无符号十进制 */
void OLED_PrintU2(uint8_t x, uint8_t page, uint32_t v);/* 2 位十进制, 前导零 */

#endif /* OLED_H */
