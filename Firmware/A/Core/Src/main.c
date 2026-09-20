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
#include "tim.h"
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
#define LED_PERIOD_MS     1000U   /* 流水灯每个 LED 点亮时长: 1s */
#define KEY_DEB_SAMPLES   2U      /* 按键消抖: 连续 2 次 10ms 采样一致才生效 (20ms) */
#define PWM1_UP_MS        4000U   /* PWM1: 4s 匀速 0%->100% */
#define PWM1_DOWN_MS      2000U   /* PWM1: 2s 匀速 100%->0% */
#define PWM1_DUTY_MAX     1000U   /* PWM1 分辨率 0.1% (ARR=999) */
#define PWM2_STEP         20U     /* 按键2: 占空比步进 20% */
#define OLED_REFRESH_MS   100U    /* OLED 刷新周期 (时间显示精度仍为 0.01s) */
#define TX_PERIOD_MS      100U    /* A->B 状态帧发送周期 */
#define RX_BUF_SIZE       64U     /* 串口接收环形缓冲 */

/* ============================================================================
 * 显示用中文姓名 (评分点: OLED 第一行)
 * 修改步骤:
 *   1. 编辑 tools/gen_font.py, 把 NAME 改成需要公开显示的别名 (或运行:
 *      py gen_font.py 显示别名 )
 *   2. 重新生成字库 (该脚本会同时更新 A/B 机的 oled_font.h)
 *   3. 按脚本输出, 把下面两个绘制调用的枚举名改成别名对应的 CN_xxx
 * 公开版使用隐私别名 "张三" (对应 CN_ZHANG + CN_SAN)
 * ==========================================================================*/
/* 调试显示开关: 1 = 第4行附加发送计数器 (T##); 0 = 正常显示 */
#define DEBUG_DISPLAY     0

/* ============================================================================
 * 验收自动演示开关 (仅供自动取证的临时固件)
 *   0 = 【正式提交】完全由真实按键 + B机控制帧驱动, 无任何自动序列
 *   1 = 【仅取证】按时间轴自动切换 PWM2 档位/方向/暂停, 便于无人值守截图
 * 正式提交必须为 0
 * ==========================================================================*/
#define ACCEPTANCE_DEMO   0

#define USER_NAME_GLYPH1   CN_ZHANG   /* 隐私别名：张 */
#define USER_NAME_GLYPH2   CN_SAN     /* 隐私别名：三 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ---- 系统节拍 (TIM2, 10ms: 8MHz/(7999+1)=1kHz, ARR=9) ---- */
static volatile uint32_t g_ms = 0U;        /* 总运行时间, 暂停时冻结 (0.01s 显示 = g_ms/10) */
static volatile uint8_t  g_paused = 0U;    /* B 机暂停指令置位 */

/* ---- 流水灯 (PB12..PB15 = LED1..LED4, 从左到右) ---- */
static uint16_t led_ms = 0U;
static uint8_t  led_idx = 0U;              /* 0..3 */

/* ---- PWM1 渐变状态机 ---- */
static uint8_t  pwm1_state = 0U;           /* 0=空闲 1=上升 2=下降 */
static uint16_t pwm1_ms = 0U;
static uint16_t pwm1_duty = 0U;            /* 0.1% 单位 0..1000 (中断写, 主循环读, 16 位原子) */

/* ---- PWM2 电机 (PA2, L298 ENA; DIR1/DIR2 = PA4/PA5) ---- */
static uint8_t  pwm2_duty = 0U;            /* % 单位 0..80, 到 100 即停止清零 */
static uint8_t  pwm2_saved = 0U;           /* 暂停前占空比, 恢复时还原 */
static uint8_t  motor_dir = 0U;            /* 0=正转 1=反转 */

/* ---- 按键 (低电平按下, 20ms 消抖) ---- */
typedef struct
{
  uint8_t last_raw;      /* 上一次原始电平 (1=按下) */
  uint8_t cnt;           /* 连续一致采样计数 */
  uint8_t stable;        /* 消抖后状态 (1=按下) */
  uint8_t event;         /* 按下沿事件, 主循环消费 */
} key_t;
static key_t keys[3];

/* ---- 串口接收环形缓冲: 中断写 head, 主循环读 tail (单生产/单消费, 无竞争) ---- */
static volatile uint8_t rx_head = 0U;
static uint8_t rx_tail = 0U;
static uint8_t rx_buf[RX_BUF_SIZE];
static uint8_t rx_byte;                    /* HAL 单字节接收载体 */

/* ---- 显示缓存 (脏检查, 避免全屏无谓刷新) ---- */
static uint8_t disp_paused_z = 0xFFU;
static uint8_t disp_duty = 0xFFU;
static uint8_t disp_state = 0xFFU;         /* 0=停止 1=正转 2=反转 */
static uint32_t disp_time_cs = 0xFFFFFFFFU;

/* ---- 通信诊断: A机发送帧计数与发送结果 ---- */
volatile uint32_t dbg_tx_ok = 0U;   /* HAL_UART_Transmit 返回 HAL_OK 次数 */
volatile uint32_t dbg_tx_err = 0U;  /* 发送错误次数 */
volatile uint32_t dbg_tx_call = 0U; /* 调用发送次数 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void motor_apply(void);
static void keys_scan(void);
static void oled_render(void);
static uint8_t motor_state(void);
static uint8_t u32_digits(uint32_t v);
static void send_state_frame(void);
static void proto_feed(uint8_t b);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* 电机输出: CCR3 = 占空比% × 10 (0.1% 单位, ARR=999); 方向脚 PA4/PA5 接 L298 IN1/IN2 */
static void motor_apply(void)
{
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, (uint32_t)pwm2_duty * 10U);
  if (pwm2_duty == 0U)
  {
    HAL_GPIO_WritePin(DIR1_GPIO_Port, DIR1_Pin | DIR2_Pin, GPIO_PIN_RESET);
  }
  else if (motor_dir == 0U)                       /* 正转 */
  {
    HAL_GPIO_WritePin(DIR1_GPIO_Port, DIR1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(DIR2_GPIO_Port, DIR2_Pin, GPIO_PIN_RESET);
  }
  else                                            /* 反转 */
  {
    HAL_GPIO_WritePin(DIR1_GPIO_Port, DIR1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DIR2_GPIO_Port, DIR2_Pin, GPIO_PIN_SET);
  }
}

/* 按键扫描: 每 10ms 由 TIM2 中断调用一次, 连续 KEY_DEB_SAMPLES 次一致才翻转, 按下沿置事件 */
static void keys_scan(void)
{
  static const uint16_t pins[3] = {KEY1_Pin, KEY2_Pin, KEY3_Pin};
  uint8_t i;
  for (i = 0U; i < 3U; i++)
  {
    uint8_t raw = (HAL_GPIO_ReadPin(KEY1_GPIO_Port, pins[i]) == GPIO_PIN_RESET) ? 1U : 0U;
    if (raw == keys[i].last_raw)
    {
      if ((keys[i].cnt < 255U) && (keys[i].cnt < KEY_DEB_SAMPLES)) { keys[i].cnt++; }
      if ((keys[i].cnt >= KEY_DEB_SAMPLES) && (keys[i].stable != raw))
      {
        keys[i].stable = raw;
        if (raw != 0U) { keys[i].event = 1U; }    /* 按下沿 */
      }
    }
    else
    {
      keys[i].last_raw = raw;
      keys[i].cnt = 1U;
    }
  }
}

/* 界面渲染: 脏检查, 只重画内容变化的行 (行高 16px, 分别在偶数页 0/2/4/6) */
static void oled_render(void)
{
  uint32_t t = g_ms;
  uint32_t t_cs = t / 10U;                        /* 0.01s 单位 */

  if (disp_paused_z != g_paused)                  /* 电机状态行 / 占空比行随暂停变化 */
  {
    disp_paused_z = g_paused;
    disp_duty = 0xFFU;                            /* 强制重画下面两行 */
    disp_state = 0xFFU;
  }

  if (disp_duty != pwm2_duty)
  {
    disp_duty = pwm2_duty;
    OLED_PutCN(CN_ZHAN, 0U, 2U);                  /* 占 */
    OLED_PutCN(CN_KONG, 16U, 2U);                 /* 空 */
    OLED_PutCN(CN_BI, 32U, 2U);                   /* 比 */
    OLED_Print(48U, 2U, ":");
    OLED_Print(56U, 2U, "   ");                   /* 清 3 位数字区 */
    OLED_PrintU(56U, 2U, (uint32_t)pwm2_duty);
    OLED_Print(56U + (uint8_t)(8U * ((pwm2_duty > 99U) ? 3U : ((pwm2_duty > 9U) ? 2U : 1U))), 2U, "%");
  }

  if (disp_state != motor_state())
  {
    disp_state = motor_state();
    OLED_PutCN(CN_ZHUANG, 0U, 4U);                /* 状 */
    OLED_PutCN(CN_TAI, 16U, 4U);                  /* 态 */
    OLED_Print(32U, 4U, ":");
    if (pwm2_duty == 0U)
    {
      OLED_PutCN(CN_TING, 48U, 4U);               /* 停止 */
      OLED_PutCN(CN_ZHI, 64U, 4U);
    }
    else if (motor_dir == 0U)
    {
      OLED_PutCN(CN_ZHENG, 48U, 4U);              /* 正转 */
      OLED_PutCN(CN_ZUAN, 64U, 4U);
    }
    else
    {
      OLED_PutCN(CN_FAN, 48U, 4U);                /* 反转 */
      OLED_PutCN(CN_ZUAN, 64U, 4U);
    }
  }

  if (disp_time_cs != t_cs)
  {
    disp_time_cs = t_cs;
    OLED_PutCN(CN_SHI, 0U, 6U);                   /* 时 */
    OLED_PutCN(CN_JIAN, 16U, 6U);                 /* 间 */
    OLED_Print(32U, 6U, ":");
    OLED_Print(40U, 6U, "        ");   /* 清 8 字符, 防止位数变化残留 */
    OLED_PrintU(40U, 6U, t_cs / 100U);
    OLED_Print(40U + (uint8_t)(8U * u32_digits(t_cs / 100U)), 6U, ".");
    OLED_PrintU2(40U + (uint8_t)(8U * (u32_digits(t_cs / 100U) + 1U)), 6U, t_cs % 100U);
#if DEBUG_DISPLAY
    /* 调试: 发送计数 (仅 DEBUG_DISPLAY=1 时显示) */
    OLED_Print(96U, 6U, "T");
    OLED_PrintU(104U, 6U, dbg_tx_ok % 100U);
#endif
  }

  OLED_Update();
}

static uint8_t motor_state(void)
{
  if (pwm2_duty == 0U) { return 0U; }
  return (motor_dir == 0U) ? 1U : 2U;
}

static uint8_t u32_digits(uint32_t v)
{
  uint8_t n = 1U;
  while (v >= 10U) { v /= 10U; n++; }
  return n;
}

/* A->B 状态帧: 0xAA 0x55 0x01 0x05 [P1H P1L P2H P2L FLAGS] CKSUM */
static void send_state_frame(void)
{
  uint8_t f[10];
  uint8_t ck;
  uint16_t p1 = pwm1_duty;
  uint16_t p2 = (uint16_t)((uint16_t)pwm2_duty * 10U);
  f[0] = PROT_HEAD1;
  f[1] = PROT_HEAD2;
  f[2] = PROT_TYPE_STATE;
  f[3] = PROT_STATE_LEN;
  f[4] = (uint8_t)(p1 >> 8);
  f[5] = (uint8_t)(p1 & 0xFFU);
  f[6] = (uint8_t)(p2 >> 8);
  f[7] = (uint8_t)(p2 & 0xFFU);
  f[8] = (g_paused != 0U) ? PROT_FLAG_PAUSE : 0U;
  ck = (uint8_t)(f[2] ^ f[3]);
  {
    uint8_t i;
    for (i = 4U; i <= 8U; i++) { ck = (uint8_t)(ck ^ f[i]); }
  }
  f[9] = ck;
  dbg_tx_call++;
  if (HAL_UART_Transmit(&huart1, f, 10U, 20U) == HAL_OK) { dbg_tx_ok++; }
  else { dbg_tx_err++; }
}

/* B->A 控制帧解析 (状态机, 主循环逐字节喂入; 校验失败整帧丢弃) */
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
      st = (type == PROT_TYPE_CTRL) ? 3U : 0U;
      break;
    case 3U:
      len = b;
      ck = (uint8_t)(ck ^ len);
      if ((len == PROT_CTRL_LEN) && (len <= PROT_MAX_PAYLOAD)) { idx = 0U; st = 4U; }
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
        if (pay[0] == PROT_CTRL_PAUSE)          /* 暂停: 电机停 + 时间停 + 流水灯停 */
        {
          g_paused = 1U;
          pwm2_saved = pwm2_duty;
          pwm2_duty = 0U;
          motor_apply();
        }
        else if (pay[0] == PROT_CTRL_RESUME)    /* 恢复: 还原暂停前占空比 */
        {
          g_paused = 0U;
          pwm2_duty = pwm2_saved;
          motor_apply();
        }
      }
      break;
    default:
      st = 0U;
      break;
  }
}

/* TIM2 10ms 系统节拍 (8MHz/(7999+1)=1kHz 计数, ARR=9 -> 每 10 个计数溢出 = 10ms) */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    keys_scan();               /* 按键每 10ms 采样一次, 连续 2 次一致即消抖 20ms */

    if (g_paused == 0U)                           /* 暂停: 时间与流水灯冻结, 当前 LED 保持点亮 */
    {
      g_ms += 10U;                                /* 每次 10ms 中断推进 10ms */
      led_ms += 10U;
      if (led_ms >= LED_PERIOD_MS)
      {
        led_ms = 0U;
        led_idx = (uint8_t)((led_idx + 1U) & 3U);
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin | LED2_Pin | LED3_Pin | LED4_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED1_GPIO_Port, (uint16_t)(LED1_Pin << led_idx), GPIO_PIN_SET);  /* LED1..4 = PA1..PA4 */
      }
    }

    /* PWM1 渐变: 暂停时输出归零且冻结进度, 恢复后从冻结位置继续 */
    if (g_paused != 0U)
    {
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0U);   /* 暂停: PWM1 输出归零(进度保留) */
    }
    else
    {
      if (pwm1_state == 1U)
      {
        pwm1_ms += 10U;   /* 每次 10ms 中断推进 10ms, 与 _MS 阈值同量纲 */
        pwm1_duty = (uint16_t)(((uint32_t)pwm1_ms * PWM1_DUTY_MAX) / PWM1_UP_MS);
        if (pwm1_ms >= PWM1_UP_MS) { pwm1_duty = PWM1_DUTY_MAX; pwm1_state = 2U; pwm1_ms = 0U; }
      }
      else if (pwm1_state == 2U)
      {
        pwm1_ms += 10U;   /* 每次 10ms 中断推进 10ms, 与 _MS 阈值同量纲 */
        pwm1_duty = (uint16_t)(PWM1_DUTY_MAX - (((uint32_t)pwm1_ms * PWM1_DUTY_MAX) / PWM1_DOWN_MS));
        if (pwm1_ms >= PWM1_DOWN_MS) { pwm1_duty = 0U; pwm1_state = 0U; pwm1_ms = 0U; }
      }
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm1_duty);
    }
  }
}

/* USART1 单字节接收完成: 写环形缓冲 (中断内只写 head, 不与主循环共享其它状态) */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uint8_t next = (uint8_t)((rx_head + 1U) % RX_BUF_SIZE);
    if (next != rx_tail)                          /* 缓冲满则丢弃最新字节 */
    {
      rx_buf[rx_head] = rx_byte;
      rx_head = next;
    }
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);   /* 重挂接收; 失败则下次中断不再触发 */
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
  MX_TIM2_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  motor_apply();                       /* 电机初始: 占空比 0, 方向脚低 */
  OLED_Init();
  OLED_PutCN(USER_NAME_GLYPH1, 0U, 0U);   /* 第一行: 姓名 (中文点阵) */
  OLED_PutCN(USER_NAME_GLYPH2, 16U, 0U);
  oled_render();
  if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) { Error_Handler(); }
  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) { Error_Handler(); }          /* 10ms 系统节拍 */
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) { Error_Handler(); }/* PWM1: PA8 */
  if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK) { Error_Handler(); }/* PWM2: PB0 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t last_oled = 0U;
    static uint32_t last_tx = 0U;
    uint32_t now = HAL_GetTick();

    /* 消费串口接收环形缓冲 (主循环唯一消费者, 与中断无共享写) */
    while (rx_tail != rx_head)
    {
      uint8_t b = rx_buf[rx_tail];
      rx_tail = (uint8_t)((rx_tail + 1U) % RX_BUF_SIZE);
      proto_feed(b);
    }

#if ACCEPTANCE_DEMO
    /* ===== 验收自动演示序列 (仅 ACCEPTANCE_DEMO=1 时编译;
     *       状态值来自真实控制变量, 非硬编码显示) ===== */
    {
      uint32_t el = HAL_GetTick() % 150000U;   /* 150 秒循环, 每档 20 秒 (留足 A/B 收敛时间) */
      if      (el <  20000U) { if (pwm2_duty != 0U)  { pwm2_duty = 0U;  motor_dir = 0U; motor_apply(); } }
      else if (el <  40000U) { if (pwm2_duty != 20U) { pwm2_duty = 20U; motor_dir = 0U; motor_apply(); } }
      else if (el <  60000U) { if (pwm2_duty != 40U) { pwm2_duty = 40U; motor_dir = 0U; motor_apply(); } }
      else if (el <  80000U) { if (pwm2_duty != 60U) { pwm2_duty = 60U; motor_dir = 0U; motor_apply(); } }
      else if (el < 100000U) { if (pwm2_duty != 80U) { pwm2_duty = 80U; motor_dir = 0U; motor_apply(); } }
      else if (el < 120000U) { if (motor_dir != 1U) { motor_dir = 1U; motor_apply(); } }   /* 反转 20s */
      else if (el < 135000U) { g_paused = 1U; pwm2_saved = pwm2_duty; pwm2_duty = 0U; motor_apply(); } /* 暂停 15s */
      else                   { g_paused = 0U; pwm2_duty = pwm2_saved; motor_dir = 0U; motor_apply(); } /* 恢复 15s */
      /* PWM1 触发: 每 10 秒一轮 4s升+2s降 (留 4s 静默) */
      {
        static uint32_t last_trig = 0xFFFFFFFFU;
        uint32_t slot = el / 10000U;
        if (slot != last_trig) { last_trig = slot; pwm1_state = 1U; pwm1_ms = 0U; }
      }
    }
#endif /* ACCEPTANCE_DEMO */

    /* 按键 1: 触发 PWM1 一次 4s 升 + 2s 降 (渐变中再按无效) */
    if (keys[0].event != 0U)
    {
      keys[0].event = 0U;
      if (pwm1_state == 0U) { pwm1_state = 1U; pwm1_ms = 0U; }
    }

    /* 按键 2: PWM2 占空比 +20%, 达到 >=100% 则电机停止并清零 (暂停时忽略) */
    if (keys[1].event != 0U)
    {
      keys[1].event = 0U;
      if (g_paused == 0U)
      {
        uint8_t nd = (uint8_t)(pwm2_duty + PWM2_STEP);
        if (nd >= 100U) { pwm2_duty = 0U; }       /* 电机停止, 占空比清零 */
        else { pwm2_duty = nd; }
        motor_apply();
      }
    }

    /* 按键 3: 电机正反转切换 (暂停时忽略) */
    if (keys[2].event != 0U)
    {
      keys[2].event = 0U;
      if (g_paused == 0U)
      {
        motor_dir = (uint8_t)(motor_dir ^ 1U);
        motor_apply();
      }
    }

    if ((now - last_oled) >= OLED_REFRESH_MS)     /* 界面刷新 (脏检查在 oled_render 内) */
    {
      last_oled = now;
      oled_render();
    }

    if ((now - last_tx) >= TX_PERIOD_MS)          /* 实时发送 PWM1/PWM2 占空比给 B 机 */
    {
      last_tx = now;
      send_state_frame();
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
