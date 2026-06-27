/* USER CODE BEGIN Header */
// ==========================================
//    单片机 A 机（基础+发挥）竞赛优秀级终极代码
// ==========================================
/* USER CODE END Header */
#include "main.h"
#include <stdio.h>  
#include <string.h> 

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3; 
UART_HandleTypeDef huart1;

uint32_t sys_time_10ms = 0; 
uint16_t led_timer = 0;     
uint8_t  led_state = 0;     
char time_str[20];          

uint8_t pwm_state = 0; 
uint16_t pwm_tick = 0; 
uint8_t pwm1_duty = 0;  
uint8_t pwm2_duty = 0;  
uint8_t motor_dir = 1;  
char duty_str[20];       

// 竞赛级：非阻塞按键状态机标志
uint8_t key5_state = 0; 
uint8_t key6_state = 0; 
uint8_t key7_state = 0; 

// 双机通讯状态
volatile uint8_t system_pause = 0; 
uint8_t a_rx_byte = 0;             

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void); 
static void MX_USART1_UART_Init(void);

// ===================================================================
// ========= OLED 底层驱动 (软件 I2C) =========
// ===================================================================
#define OLED_ADDR 0x78 
void I2C_Delay(void) { for(volatile int i=0; i<10; i++); }
void I2C_Start(void) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); I2C_Delay(); }
void I2C_Stop(void) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); I2C_Delay(); }
void I2C_SendByte(uint8_t byte) {
    for(int i=0; i<8; i++) {
        if(byte & 0x80) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); else HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); I2C_Delay(); byte <<= 1;
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); I2C_Delay(); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); I2C_Delay();
}
void OLED_WriteCmd(uint8_t cmd) { I2C_Start(); I2C_SendByte(OLED_ADDR); I2C_SendByte(0x00); I2C_SendByte(cmd); I2C_Stop(); }
void OLED_WriteData(uint8_t data) { I2C_Start(); I2C_SendByte(OLED_ADDR); I2C_SendByte(0x40); I2C_SendByte(data); I2C_Stop(); }
void OLED_Init(void) {
    HAL_Delay(100);
    OLED_WriteCmd(0xAE); OLED_WriteCmd(0x20); OLED_WriteCmd(0x02); OLED_WriteCmd(0x00); OLED_WriteCmd(0x10); OLED_WriteCmd(0x40); 
    OLED_WriteCmd(0xB0); OLED_WriteCmd(0x81); OLED_WriteCmd(0xFF); OLED_WriteCmd(0xA1); OLED_WriteCmd(0xA6); OLED_WriteCmd(0xA8); 
    OLED_WriteCmd(0x3F); OLED_WriteCmd(0xC8); OLED_WriteCmd(0xD3); OLED_WriteCmd(0x00); OLED_WriteCmd(0xDA); OLED_WriteCmd(0x12); 
    OLED_WriteCmd(0x8D); OLED_WriteCmd(0x14); OLED_WriteCmd(0xAF); 
}
void OLED_Clear(void) { for(uint8_t i=0; i<8; i++) { OLED_WriteCmd(0xB0 + i); OLED_WriteCmd(0x00); OLED_WriteCmd(0x10); for(uint8_t n=0; n<128; n++) OLED_WriteData(0x00); } }

// ===================================================================
// ========= 标准全尺寸 ASCII 字库 =========
// ===================================================================
const uint8_t F6x8[][6] = {
    {0x00,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x2f,0x00,0x00},{0x00,0x00,0x07,0x00,0x07,0x00},{0x00,0x14,0x7f,0x14,0x7f,0x14},
    {0x00,0x24,0x2a,0x7f,0x2a,0x12},{0x00,0x23,0x13,0x08,0x64,0x62},{0x00,0x36,0x49,0x55,0x22,0x50},{0x00,0x00,0x05,0x03,0x00,0x00},
    {0x00,0x00,0x1c,0x22,0x41,0x00},{0x00,0x00,0x41,0x22,0x1c,0x00},{0x00,0x14,0x08,0x3e,0x08,0x14},{0x00,0x08,0x08,0x3e,0x08,0x08},
    {0x00,0x00,0x40,0x20,0x00,0x00},{0x00,0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00,0x00},{0x00,0x20,0x10,0x08,0x04,0x02},
    {0x00,0x3e,0x51,0x49,0x45,0x3e},{0x00,0x00,0x42,0x7f,0x40,0x00},{0x00,0x42,0x61,0x51,0x49,0x46},{0x00,0x21,0x41,0x45,0x4b,0x31},
    {0x00,0x18,0x14,0x12,0x7f,0x10},{0x00,0x27,0x45,0x45,0x45,0x39},{0x00,0x3c,0x4a,0x49,0x49,0x30},{0x00,0x01,0x71,0x09,0x05,0x03},
    {0x00,0x36,0x49,0x49,0x49,0x36},{0x00,0x06,0x49,0x49,0x29,0x1e},{0x00,0x00,0x36,0x36,0x00,0x00},{0x00,0x00,0x56,0x36,0x00,0x00},
    {0x00,0x00,0x08,0x14,0x22,0x41},{0x00,0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08,0x00},{0x00,0x02,0x01,0x51,0x09,0x06},
    {0x00,0x32,0x49,0x79,0x41,0x3e},{0x00,0x7e,0x11,0x11,0x11,0x7e},{0x00,0x7f,0x49,0x49,0x49,0x36},{0x00,0x3e,0x41,0x41,0x41,0x22},
    {0x00,0x7f,0x41,0x41,0x22,0x1c},{0x00,0x7f,0x49,0x49,0x49,0x41},{0x00,0x7f,0x09,0x09,0x01,0x01},{0x00,0x3e,0x41,0x41,0x51,0x32},
    {0x00,0x7f,0x08,0x08,0x08,0x7f},{0x00,0x00,0x41,0x7f,0x41,0x00},{0x00,0x20,0x40,0x41,0x3f,0x01},{0x00,0x7f,0x08,0x14,0x22,0x41},
    {0x00,0x7f,0x40,0x40,0x40,0x40},{0x00,0x7f,0x02,0x04,0x02,0x7f},{0x00,0x7f,0x04,0x08,0x10,0x7f},{0x00,0x3e,0x41,0x41,0x41,0x3e},
    {0x00,0x7f,0x09,0x09,0x09,0x06},{0x00,0x3e,0x41,0x51,0x21,0x5e},{0x00,0x7f,0x09,0x19,0x29,0x46},{0x00,0x46,0x49,0x49,0x49,0x31},
    {0x00,0x01,0x01,0x7f,0x01,0x01},{0x00,0x3f,0x40,0x40,0x40,0x3f},{0x00,0x1f,0x20,0x40,0x20,0x1f},{0x00,0x3f,0x40,0x38,0x40,0x3f},
    {0x00,0x63,0x14,0x08,0x14,0x63},{0x00,0x03,0x04,0x78,0x04,0x03},{0x00,0x61,0x51,0x49,0x45,0x43},{0x00,0x00,0x7f,0x41,0x41,0x00},
    {0x00,0x02,0x04,0x08,0x10,0x20},{0x00,0x00,0x41,0x41,0x7f,0x00},{0x00,0x04,0x02,0x01,0x02,0x04},{0x00,0x40,0x40,0x40,0x40,0x40},
    {0x00,0x00,0x01,0x02,0x04,0x00},{0x00,0x20,0x54,0x54,0x54,0x78},{0x00,0x7f,0x48,0x44,0x44,0x38},{0x00,0x38,0x44,0x44,0x44,0x20},
    {0x00,0x38,0x44,0x44,0x48,0x7f},{0x00,0x38,0x54,0x54,0x54,0x18},{0x00,0x08,0x7e,0x09,0x01,0x02},{0x00,0x08,0x14,0x54,0x54,0x3c},
    {0x00,0x7f,0x08,0x04,0x04,0x78},{0x00,0x00,0x44,0x7d,0x40,0x00},{0x00,0x20,0x40,0x44,0x3d,0x00},{0x00,0x7f,0x10,0x28,0x44,0x00},
    {0x00,0x00,0x41,0x7f,0x40,0x00},{0x00,0x7c,0x04,0x18,0x04,0x78},{0x00,0x7c,0x08,0x04,0x04,0x78},{0x00,0x38,0x44,0x44,0x44,0x38},
    {0x00,0x7c,0x14,0x14,0x14,0x08},{0x00,0x08,0x14,0x14,0x18,0x7c},{0x00,0x7c,0x08,0x04,0x04,0x08},{0x00,0x48,0x54,0x54,0x54,0x20},
    {0x00,0x04,0x3f,0x44,0x40,0x20},{0x00,0x3c,0x40,0x40,0x20,0x7c},{0x00,0x1c,0x20,0x40,0x20,0x1c},{0x00,0x3c,0x40,0x30,0x40,0x3c},
    {0x00,0x44,0x28,0x10,0x28,0x44},{0x00,0x0c,0x50,0x50,0x50,0x3c},{0x00,0x44,0x64,0x54,0x4c,0x44},{0x00,0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x00,0x7f,0x00,0x00},{0x00,0x00,0x41,0x36,0x08,0x00},{0x00,0x08,0x04,0x08,0x10,0x08}
};
void OLED_ShowChar(uint8_t x, uint8_t y, char chr) {
    if (chr < ' ' || chr > '~') return;
    uint8_t c = chr - ' '; 
    OLED_WriteCmd(0xB0 + y); OLED_WriteCmd(((x & 0xF0) >> 4) | 0x10); OLED_WriteCmd(x & 0x0F);
    for(uint8_t i=0; i<6; i++) OLED_WriteData(F6x8[c][i]);
}
void OLED_ShowString(uint8_t x, uint8_t y, char *str) { while (*str) { OLED_ShowChar(x, y, *str); x += 6; str++; } }

// ===================================================================
// ===== A机中文字库：完美横向解码引擎 (占空比状态正反转停止) ========
// ===================================================================
static const unsigned char bitmap_bytes_A[] = {
    0x01, 0x00, 0x02, 0x00, 0x00, 0x80, 0x08, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x20, 0x20, 0x10, 0x80, 0x01, 0x00, 
    0x01, 0x00, 0x01, 0x00, 0x20, 0x80, 0x08, 0x48, 0x01, 0x00, 0x7f, 0xfc, 0x00, 0xf8, 0x20, 0x20, 0x10, 0x40, 0x01, 0x00, 
    0x01, 0x00, 0x7f, 0xfe, 0x20, 0x80, 0x08, 0x44, 0x7f, 0xfc, 0x01, 0x00, 0x3f, 0x00, 0x20, 0x20, 0x17, 0xfc, 0x01, 0x00, 
    0x01, 0xfe, 0x40, 0x02, 0x20, 0x84, 0x48, 0x44, 0x01, 0x00, 0x01, 0x00, 0x20, 0x00, 0xfd, 0xfc, 0x20, 0x00, 0x01, 0x00, 
    0x01, 0x00, 0x88, 0x24, 0x20, 0x88, 0x28, 0x40, 0x02, 0x80, 0x01, 0x00, 0x20, 0x00, 0x40, 0x20, 0x23, 0xf8, 0x11, 0x00, 
    0x01, 0x00, 0x10, 0x10, 0x20, 0x90, 0x2f, 0xfe, 0x04, 0x40, 0x01, 0x00, 0x3f, 0xf8, 0x50, 0x40, 0x62, 0x08, 0x11, 0x00, 0x01, 0x00, 0x20, 0x08, 0x3e, 0xa0, 0x08, 0x40, 0x0a, 0x20, 0x11, 0x00, 0x24, 0x08, 0x93, 0xfe, 0x63, 0xf8, 0x11, 0xf8, 
    0x01, 0x00, 0x00, 0x00, 0x20, 0xc0, 0x08, 0x40, 0x31, 0x18, 0x11, 0xf8, 0x24, 0x10, 0xfc, 0x40, 0xa0, 0x00, 0x11, 0x00, 
    0x3f, 0xf8, 0x1f, 0xf0, 0x20, 0x80, 0x18, 0x40, 0xc0, 0x06, 0x11, 0x00, 0x22, 0x10, 0x10, 0x80, 0x2f, 0xfe, 0x11, 0x00, 
    0x20, 0x08, 0x01, 0x00, 0x20, 0x80, 0x28, 0xa0, 0x01, 0x00, 0x11, 0x00, 0x22, 0x20, 0x11, 0xfc, 0x28, 0x02, 0x11, 0x00, 
    0x20, 0x08, 0x01, 0x00, 0x20, 0x80, 0xc8, 0xa0, 0x08, 0x88, 0x11, 0x00, 0x21, 0x40, 0x1c, 0x04, 0x23, 0xf8, 0x11, 0x00, 
    0x20, 0x08, 0x01, 0x00, 0x20, 0x82, 0x08, 0x90, 0x48, 0x84, 0x11, 0x00, 0x20, 0x80, 0xf0, 0x88, 0x20, 0x40, 0x11, 0x00, 
    0x20, 0x08, 0x01, 0x00, 0x26, 0x82, 0x09, 0x10, 0x48, 0x12, 0x11, 0x00, 0x41, 0x40, 0x50, 0x50, 0x20, 0x40, 0x11, 0x00, 
    0x20, 0x08, 0x01, 0x00, 0x38, 0x82, 0x09, 0x08, 0x48, 0x12, 0x11, 0x00, 0x42, 0x20, 0x10, 0x20, 0x20, 0x40, 0x11, 0x00, 
    0x3f, 0xf8, 0x7f, 0xfc, 0x20, 0x7e, 0x0a, 0x04, 0x87, 0xf0, 0xff, 0xfe, 0x8c, 0x18, 0x10, 0x10, 0x21, 0x40, 0xff, 0xfe, 
    0x20, 0x08, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x02, 0x00, 0x00, 0x00, 0x00, 0x30, 0x06, 0x10, 0x10, 0x20, 0x80, 0x00, 0x00
};

void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index) {
    uint8_t start_col = index * 16;
    uint8_t bytes_per_row = 10 * 2; 
    for (uint8_t page = 0; page < 2; page++) {
        OLED_WriteCmd(0xB0 + y + page); OLED_WriteCmd(((x & 0xF0) >> 4) | 0x10); OLED_WriteCmd(x & 0x0F);
        for (uint8_t col = 0; col < 16; col++) {
            uint8_t actual_col = start_col + col; uint8_t vData = 0;
            for (uint8_t r = 0; r < 8; r++) {
                uint8_t row = page * 8 + r; uint8_t byte_idx = row * bytes_per_row + (actual_col / 8); uint8_t bit_idx = 7 - (actual_col % 8);
                if (bitmap_bytes_A[byte_idx] & (1 << bit_idx)) vData |= (1 << r);
            }
            OLED_WriteData(vData);
        }
    }
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  __HAL_RCC_AFIO_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_TIM1_CLK_ENABLE(); __HAL_RCC_TIM2_CLK_ENABLE(); __HAL_RCC_TIM3_CLK_ENABLE(); __HAL_RCC_USART1_CLK_ENABLE(); 

  MX_GPIO_Init(); MX_TIM2_Init(); MX_TIM1_Init(); MX_TIM3_Init(); MX_USART1_UART_Init();
  
  HAL_Delay(200);
  OLED_Init(); OLED_Clear(); 

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET); 
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0); 

  HAL_TIM_Base_Start_IT(&htim2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); __HAL_TIM_MOE_ENABLE(&htim1); 
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); 

  HAL_UART_Receive_IT(&huart1, &a_rx_byte, 1);

  uint8_t tx_counter = 0;
  char uart_buf[30];

  while (1)
  {
      // 👉 串口保活引擎：若HAL因溢出死锁，自动复位重启中断接收
      if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) {
          __HAL_UART_CLEAR_OREFLAG(&huart1);
          HAL_UART_Receive_IT(&huart1, &a_rx_byte, 1);
      }

      if (!system_pause) {
          OLED_ShowString(0, 0, "LiuGuangZ"); 
          sprintf(duty_str, "PWM1:%d%%  ", pwm1_duty); OLED_ShowString(72, 0, duty_str); 

          OLED_ShowChinese(0, 2, 0); OLED_ShowChinese(16, 2, 1); OLED_ShowChinese(32, 2, 2); OLED_ShowString(48, 2, ":");
          sprintf(duty_str, "%d%%  ", pwm2_duty); OLED_ShowString(56, 2, duty_str); 
          
          OLED_ShowChinese(0, 4, 3); OLED_ShowChinese(16, 4, 4); OLED_ShowString(32, 4, ":");
          if(pwm2_duty == 0) {
              OLED_ShowChinese(40, 4, 8); OLED_ShowChinese(56, 4, 9); OLED_ShowString(72, 4, "    ");
          } else {
              if(motor_dir == 1) { OLED_ShowChinese(40, 4, 5); OLED_ShowChinese(56, 4, 7); OLED_ShowString(72, 4, "    "); }
              else { OLED_ShowChinese(40, 4, 6); OLED_ShowChinese(56, 4, 7); OLED_ShowString(72, 4, "    "); }
          }

          uint32_t sec = sys_time_10ms / 100; uint32_t ms10 = sys_time_10ms % 100;
          sprintf(time_str, "Time: %d.%02ds  ", sec, ms10); OLED_ShowString(0, 6, time_str);    
          
          if(pwm2_duty == 0) {
              HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);
          } else {
              if(motor_dir == 1) { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET); } 
              else { HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET); }
          }

          tx_counter++;
          if(tx_counter >= 5) {
              sprintf(uart_buf, "PWM1:%d,PWM2:%d\r\n", pwm1_duty, pwm2_duty);
              HAL_UART_Transmit(&huart1, (uint8_t *)uart_buf, strlen(uart_buf), 100);
              tx_counter = 0;
          }
      } 
      else {
          OLED_ShowString(0, 4, "SYSTEM PAUSED ");
      }
      HAL_Delay(50);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if(htim->Instance == TIM2) {
        if (system_pause) { 
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0); 
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET); 
            return; 
        }

        sys_time_10ms++; led_timer++;
        if(led_timer >= 100) { 
            led_timer = 0;
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);
            if(led_state == 0) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
            else if(led_state == 1) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
            else if(led_state == 2) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
            else if(led_state == 3) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
            led_state++; if(led_state > 3) led_state = 0; 
        }

        if (pwm_state == 1) { 
            pwm_tick++;
            if (pwm_tick % 4 == 0) { 
                pwm1_duty++; __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm1_duty); 
                if (pwm1_duty >= 100) { pwm_state = 2; pwm_tick = 0; }
            }
        } else if (pwm_state == 2) { 
            pwm_tick++;
            if (pwm_tick % 2 == 0) { 
                if(pwm1_duty > 0) pwm1_duty--;
                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm1_duty); 
                if (pwm1_duty == 0) { pwm_state = 0; pwm_tick = 0; }
            }
        }

        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) {
            if (key5_state == 0) { key5_state = 1; if(pwm_state == 0) { pwm_state = 1; pwm1_duty = 0; pwm_tick = 0; } }
        } else key5_state = 0;

        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET) {
            if (key6_state == 0) { key6_state = 1; pwm2_duty += 20; if(pwm2_duty >= 100) pwm2_duty = 0; __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwm2_duty); }
        } else key6_state = 0;

        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_RESET) {
            if (key7_state == 0) { key7_state = 1; motor_dir = !motor_dir; }
        } else key7_state = 0;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart->Instance == USART1) {
        if(a_rx_byte == 'S') { system_pause = 1; }
        HAL_UART_Receive_IT(&huart1, &a_rx_byte, 1); 
    }
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0}; RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE; RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON; RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE; RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL2;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2; 
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2; RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

static void MX_USART1_UART_Init(void) {
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600; // 👉 全面降到超稳的 9600 抵抗仿真时间失真
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  HAL_UART_Init(&huart1);
  HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void MX_TIM1_Init(void) { TIM_ClockConfigTypeDef sClockSourceConfig = {0}; TIM_MasterConfigTypeDef sMasterConfig = {0}; TIM_OC_InitTypeDef sConfigOC = {0}; TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0}; htim1.Instance = TIM1; htim1.Init.Prescaler = 80-1; htim1.Init.CounterMode = TIM_COUNTERMODE_UP; htim1.Init.Period = 100-1; htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; htim1.Init.RepetitionCounter = 0; htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; HAL_TIM_Base_Init(&htim1); sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL; HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig); HAL_TIM_PWM_Init(&htim1); sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET; sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE; HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig); sConfigOC.OCMode = TIM_OCMODE_PWM1; sConfigOC.Pulse = 0; sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH; sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH; sConfigOC.OCFastMode = TIM_OCFAST_DISABLE; sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET; sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET; HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1); sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE; sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE; sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF; sBreakDeadTimeConfig.DeadTime = 0; sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE; sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH; sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE; HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig); HAL_TIM_MspPostInit(&htim1); }
static void MX_TIM2_Init(void) { TIM_ClockConfigTypeDef sClockSourceConfig = {0}; TIM_MasterConfigTypeDef sMasterConfig = {0}; htim2.Instance = TIM2; htim2.Init.Prescaler = 8000-1; htim2.Init.CounterMode = TIM_COUNTERMODE_UP; htim2.Init.Period = 10-1; htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; HAL_TIM_Base_Init(&htim2); sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL; HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig); sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET; sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE; HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig); }
static void MX_TIM3_Init(void) { TIM_ClockConfigTypeDef sClockSourceConfig = {0}; TIM_MasterConfigTypeDef sMasterConfig = {0}; TIM_OC_InitTypeDef sConfigOC = {0}; htim3.Instance = TIM3; htim3.Init.Prescaler = 80-1; htim3.Init.CounterMode = TIM_COUNTERMODE_UP; htim3.Init.Period = 100-1; htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; HAL_TIM_Base_Init(&htim3); sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL; HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig); HAL_TIM_PWM_Init(&htim3); sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET; sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE; HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig); sConfigOC.OCMode = TIM_OCMODE_PWM1; sConfigOC.Pulse = 0; sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH; sConfigOC.OCFastMode = TIM_OCFAST_DISABLE; HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3); }
static void MX_GPIO_Init(void) { GPIO_InitTypeDef GPIO_InitStruct = {0}; HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET); GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4; GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7; GPIO_InitStruct.Mode = GPIO_MODE_INPUT; GPIO_InitStruct.Pull = GPIO_PULLUP; HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_9; GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_10; GPIO_InitStruct.Mode = GPIO_MODE_INPUT; GPIO_InitStruct.Pull = GPIO_NOPULL; HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_8; GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_0; GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2; GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW; HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7; GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Pull = GPIO_NOPULL; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); }