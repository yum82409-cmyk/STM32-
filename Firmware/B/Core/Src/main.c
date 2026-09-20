/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KEY_DEB_SAMPLES   2U      /* 按键消抖: 连续 2 次 10ms 采样一致 (20ms) */
#define OLED_REFRESH_MS   20U     /* OLED 最小刷新间隔 (收到新帧即刷新, 保证数值与状态同帧更新) */
#define RX_BUF_SIZE       64U     /* 串口接收环形缓冲 */

/* ============================================================================
 * 调试显示开关: 1 = OLED 第4行显示通信计数器 (I中断数/F帧数/E校验错)
 *               0 = 正常显示 (仅保留计数器变量, 不占用屏幕)
 * 正式验收请保持 0
 * ==========================================================================*/
#define DEBUG_DISPLAY     0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ---- A 机发来的状态 (解析帧后更新, 仅主循环访问) ---- */
static uint16_t pwm1_duty = 0U;            /* 0.1% 单位 0..1000 */
static uint16_t pwm2_duty = 0U;            /* 0.1% 单位 0..1000 */
static uint8_t  a_paused = 0U;
static uint8_t  frame_new = 0U;

/* ---- 通信诊断计数器 (临时诊断用, 定位数据停在哪一步) ---- */
volatile uint32_t dbg_isr  = 0U;   /* UART 接收中断进入次数 */
volatile uint32_t dbg_ring = 0U;   /* 环形缓冲写入成功次数 */
volatile uint32_t dbg_full = 0U;   /* 环形缓冲满(丢弃)次数 */
volatile uint32_t dbg_frame= 0U;   /* 收到完整帧(校验通过)次数 */
volatile uint32_t dbg_ckbad= 0U;   /* 校验失败次数 */
volatile uint16_t dbg_last_p1 = 0U;
volatile uint16_t dbg_last_p2 = 0U;

/* ---- 暂停键状态 (PB0, 按下切换 暂停/恢复) ---- */
static uint8_t  pause_cmd = 0U;            /* 当前输出状态: 0=运行 1=暂停 */

/* ---- 按键消抖 (主循环 10ms 扫描) ---- */
typedef struct
{
  uint8_t last_raw;
  uint8_t cnt;
  uint8_t stable;
  uint8_t event;
} key_t;
static key_t key_pause;

/* ---- 串口接收环形缓冲: 中断写 head, 主循环读 tail (单生产/单消费, 无竞争) ---- */
static volatile uint8_t rx_head = 0U;
static uint8_t rx_tail = 0U;
static uint8_t rx_buf[RX_BUF_SIZE];
static uint8_t rx_byte;

/* ---- 显示缓存 ---- */
static uint16_t disp_p1 = 0xFFFFU;
static uint16_t disp_p2 = 0xFFFFU;
static uint8_t  disp_state = 0xFFU;        /* 0=电机停止 1=正常运作 2=速度异常 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void proto_feed(uint8_t b);
static void oled_render(void);
static void send_ctrl_frame(uint8_t cmd);
static uint8_t u16_digits(uint16_t v);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* A->B 状态帧解析: 0xAA 0x55 0x01 0x05 [P1H P1L P2H P2L FLAGS] CKSUM
 * 状态机逐字节喂入, XOR 校验失败整帧丢弃 (旧方案 sscanf 行协议无校验, 已废除) */
static void proto_feed(uint8_t b)
{
  static uint8_t st = 0U;         /* 0 找头1 1 找头2 2 TYPE 3 LEN 4 载荷 5 校验 */
  static uint8_t type = 0U;
  static uint8_t len = 0U;
  static uint8_t idx = 0U;
  static uint8_t pay[PROT_MAX_PAYLOAD];
  static uint8_t ck = 0U;

  switch (st)
  {
    case 0U:
      if (b == PROT_HEAD1) { st = 1U; }
      break;
    case 1U:
      st = (b == PROT_HEAD2) ? 2U : 0U;
      break;
    case 2U:
      type = b;
      ck = b;
      st = (type == PROT_TYPE_STATE) ? 3U : 0U;
      break;
    case 3U:
      len = b;
      ck = (uint8_t)(ck ^ len);
      if ((len == PROT_STATE_LEN) && (len <= PROT_MAX_PAYLOAD)) { idx = 0U; st = 4U; }
      else { st = 0U; }
      break;
    case 4U:
      pay[idx++] = b;
      ck = (uint8_t)(ck ^ b);
      st = (idx >= len) ? 5U : 4U;
      break;
    case 5U:
      st = 0U;
      if (b == ck)
      {
        dbg_frame++;
        pwm1_duty = (uint16_t)(((uint16_t)pay[0] << 8) | pay[1]);
        pwm2_duty = (uint16_t)(((uint16_t)pay[2] << 8) | pay[3]);
        dbg_last_p1 = pwm1_duty;
        dbg_last_p2 = pwm2_duty;
        a_paused  = (uint8_t)(pay[4] & PROT_FLAG_PAUSE);
        frame_new = 1U;
      }
      else
      {
        dbg_ckbad++;
      }
      break;
    default:
      st = 0U;
      break;
  }
}

/* 第三行状态判断: =0 电机停止; 0<duty<80% 正常运作; >=80% 速度异常
 * A 机暂停时占空比已清零, 此处再按暂停标志兜底强制显示"电机停止" */
static uint8_t motor_status(void)
{
  if ((pwm2_duty == 0U) || (a_paused != 0U)) { return 0U; }
  if (pwm2_duty < 800U) { return 1U; }
  return 2U;
}

static void oled_render(void)
{
  uint8_t st = motor_status();

  if (disp_p1 != pwm1_duty)
  {
    disp_p1 = pwm1_duty;
    OLED_Print(0U, 0U, "PWM1:");
    OLED_Print(40U, 0U, "    ");               /* 清数字区 */
    OLED_PrintU(40U, 0U, (uint32_t)(pwm1_duty / 10U));  /* 0.1% -> % */
    OLED_Print(40U + (uint8_t)(8U * u16_digits((uint16_t)(pwm1_duty / 10U))), 0U, "%");
  }

  if (disp_p2 != pwm2_duty)
  {
    disp_p2 = pwm2_duty;
    OLED_Print(0U, 2U, "PWM2:");
    OLED_Print(40U, 2U, "    ");
    OLED_PrintU(40U, 2U, (uint32_t)(pwm2_duty / 10U));
    OLED_Print(40U + (uint8_t)(8U * u16_digits((uint16_t)(pwm2_duty / 10U))), 2U, "%");
  }

  if (disp_state != st)
  {
    disp_state = st;
    if (st == 0U)
    {
      OLED_PutCN(CN_DIAN, 0U, 4U);             /* 电机停止 */
      OLED_PutCN(CN_JI, 16U, 4U);
      OLED_PutCN(CN_TING, 32U, 4U);
      OLED_PutCN(CN_ZHI, 48U, 4U);
    }
    else if (st == 1U)
    {
      OLED_PutCN(CN_ZHENG, 0U, 4U);            /* 正常运作 */
      OLED_PutCN(CN_CHANG, 16U, 4U);
      OLED_PutCN(CN_YUN, 32U, 4U);
      OLED_PutCN(CN_ZUO, 48U, 4U);
    }
    else
    {
      OLED_PutCN(CN_SU, 0U, 4U);               /* 速度异常 */
      OLED_PutCN(CN_DU, 16U, 4U);
      OLED_PutCN(CN_YI, 32U, 4U);
      OLED_PutCN(CN_CHANG, 48U, 4U);
    }
  }

#if DEBUG_DISPLAY
  /* 调试行: 通信计数器 (仅在 DEBUG_DISPLAY=1 时显示) */
  OLED_Print(0U, 6U, "I");
  OLED_PrintU(8U, 6U, dbg_isr);
  OLED_Print(64U, 6U, "F");
  OLED_PrintU(72U, 6U, dbg_frame);
  OLED_Print(96U, 6U, "E");
  OLED_PrintU(104U, 6U, dbg_ckbad % 1000U);
#endif

  OLED_Update();
}

static uint8_t u16_digits(uint16_t v)
{
  uint8_t n = 1U;
  while (v >= 10U) { v /= 10U; n++; }
  return n;
}

/* B->A 控制帧: 0xAA 0x55 0x02 0x01 [CMD] CKSUM */
static void send_ctrl_frame(uint8_t cmd)
{
  uint8_t f[6];
  f[0] = PROT_HEAD1;
  f[1] = PROT_HEAD2;
  f[2] = PROT_TYPE_CTRL;
  f[3] = PROT_CTRL_LEN;
  f[4] = cmd;
  f[5] = (uint8_t)(PROT_TYPE_CTRL ^ PROT_CTRL_LEN ^ cmd);
  (void)HAL_UART_Transmit(&huart1, f, 6U, 20U);
}

/* USART1 单字节接收完成: 写环形缓冲 (中断内只写 head) */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uint8_t next = (uint8_t)((rx_head + 1U) % RX_BUF_SIZE);
    dbg_isr++;
    if (next != rx_tail)
    {
      rx_buf[rx_head] = rx_byte;
      rx_head = next;
      dbg_ring++;
    }
    else
    {
      dbg_full++;
    }
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  OLED_Init();
  oled_render();                       /* 初始画面: PWM1:0% / PWM2:0% / 电机停止 */
  (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t last_scan = 0U;
    static uint32_t last_oled = 0U;
    uint32_t now = HAL_GetTick();

    /* 消费串口接收环形缓冲 */
    while (rx_tail != rx_head)
    {
      uint8_t b = rx_buf[rx_tail];
      rx_tail = (uint8_t)((rx_tail + 1U) % RX_BUF_SIZE);
      proto_feed(b);
    }

    /* 暂停键 (PB0): 每 10ms 扫描, 20ms 消抖, 按下沿切换 暂停/恢复 并发控制帧 */
    if ((now - last_scan) >= 10U)
    {
      uint8_t raw;
      last_scan = now;
      raw = (HAL_GPIO_ReadPin(KEY_PAUSE_GPIO_Port, KEY_PAUSE_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
      if (raw == key_pause.last_raw)
      {
        if ((key_pause.cnt < 255U) && (key_pause.cnt < KEY_DEB_SAMPLES)) { key_pause.cnt++; }
        if ((key_pause.cnt >= KEY_DEB_SAMPLES) && (key_pause.stable != raw))
        {
          key_pause.stable = raw;
          if (raw != 0U) { key_pause.event = 1U; }
        }
      }
      else
      {
        key_pause.last_raw = raw;
        key_pause.cnt = 1U;
      }
    }

    if (key_pause.event != 0U)
    {
      key_pause.event = 0U;
      pause_cmd = (uint8_t)(pause_cmd ^ 1U);    /* 一键暂停/恢复 */
      send_ctrl_frame((pause_cmd != 0U) ? PROT_CTRL_PAUSE : PROT_CTRL_RESUME);
    }

    /* 收到新帧即刷新 (限最小间隔 20ms 防止刷屏过载):
       保证「占空比数值」与「状态文字」在同一帧内同时更新, 消除状态滞后 */
    if ((frame_new != 0U) && ((now - last_oled) >= 20U))
    {
      frame_new = 0U;
      last_oled = now;
      oled_render();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
