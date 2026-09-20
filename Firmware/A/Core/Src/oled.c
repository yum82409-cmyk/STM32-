/* oled.c - SSD1306 128x64 OLED 驱动 (软件 I2C + 全帧缓冲)
 *
 * 字库为行扫描横向取模 (字节 = 一行 8 个水平像素, 高位在左);
 * GRAM 页寻址下字节 = 一列 8 个垂直像素, 因此写入帧缓冲时做转置。
 * 字形固定 16 行高, 只允许放在偶数页 (page 与 page+1 正好两个整页)。
 */
#include "oled.h"
#include "oled_font.h"

/* ---------------- 软件 I2C (开漏, 外部上拉, 直接写 BSRR/BRR 提速) ---------------- */

#define SCL_HI()  (OLED_SCL_GPIO->BSRR = (uint32_t)OLED_SCL_PIN)
#define SCL_LO()  (OLED_SCL_GPIO->BRR  = (uint32_t)OLED_SCL_PIN)
#define SDA_HI()  (OLED_SDA_GPIO->BSRR = (uint32_t)OLED_SDA_PIN)
#define SDA_LO()  (OLED_SDA_GPIO->BRR  = (uint32_t)OLED_SDA_PIN)
#define SDA_READ() ((OLED_SDA_GPIO->IDR & OLED_SDA_PIN) != 0U)

static void i2c_bit_delay(void)
{
    volatile uint8_t n = 2U;      /* 8MHz 主频下 ~1us, 远低于 SSD1306 400kHz 上限 */
    while (n != 0U) { n--; }
}

static void i2c_start(void)
{
    SDA_HI(); SCL_HI(); i2c_bit_delay();
    SDA_LO(); i2c_bit_delay();
    SCL_LO();
}

static void i2c_stop(void)
{
    SDA_LO(); i2c_bit_delay();
    SCL_HI(); i2c_bit_delay();
    SDA_HI(); i2c_bit_delay();
}

/* 返回 0 = ACK, 1 = NACK */
static uint8_t i2c_write_byte(uint8_t b)
{
    uint8_t i;
    for (i = 0U; i < 8U; i++)
    {
        if ((b & 0x80U) != 0U) { SDA_HI(); } else { SDA_LO(); }
        b = (uint8_t)(b << 1);
        i2c_bit_delay();
        SCL_HI();
        i2c_bit_delay();
        SCL_LO();
    }
    SDA_HI();                     /* 释放 SDA 读 ACK */
    i2c_bit_delay();
    SCL_HI();
    i2c_bit_delay();
    {
        uint8_t ack = SDA_READ() ? 1U : 0U;
        SCL_LO();
        return ack;
    }
}

static void oled_write_cmd(uint8_t cmd)
{
    i2c_start();
    (void)i2c_write_byte(OLED_I2C_ADDR);
    (void)i2c_write_byte(0x00U);  /* 控制字节: 命令流 */
    (void)i2c_write_byte(cmd);
    i2c_stop();
}

static void oled_write_data_bytes(const uint8_t *p, uint8_t n)
{
    uint8_t i;
    i2c_start();
    (void)i2c_write_byte(OLED_I2C_ADDR);
    (void)i2c_write_byte(0x40U);  /* 控制字节: 数据流 */
    for (i = 0U; i < n; i++)
    {
        (void)i2c_write_byte(p[i]);
    }
    i2c_stop();
}

/* ---------------- 帧缓冲 ---------------- */

static uint8_t oled_buf[8][128];   /* [page][column] */

void OLED_Clear(void)
{
    uint8_t page;
    for (page = 0U; page < 8U; page++)
    {
        uint8_t col;
        for (col = 0U; col < 128U; col++) { oled_buf[page][col] = 0U; }
    }
}

void OLED_Update(void)
{
    uint8_t page;
    for (page = 0U; page < 8U; page++)
    {
        oled_write_cmd(0xB0U | page);          /* 页地址 */
        oled_write_cmd(0x00U);                 /* 列地址低 4 位 */
        oled_write_cmd(0x10U);                 /* 列地址高 4 位 */
        oled_write_data_bytes(oled_buf[page], 128U);
    }
}

void OLED_Init(void)
{
    HAL_Delay(100);
    oled_write_cmd(0xAE);   /* display off */
    oled_write_cmd(0xD5);   /* 时钟分频/振荡 */
    oled_write_cmd(0x80);
    oled_write_cmd(0xA8);   /* multiplex 1/64 */
    oled_write_cmd(0x3F);
    oled_write_cmd(0xD3);   /* display offset 0 */
    oled_write_cmd(0x00);
    oled_write_cmd(0x40);   /* start line 0 */
    oled_write_cmd(0x8D);   /* charge pump */
    oled_write_cmd(0x14);
    oled_write_cmd(0x20);   /* 寻址模式: 0x00=水平 0x02=页 */
    oled_write_cmd(0x02);   /* 保持页寻址 (Proteus 模型与实物一致) */
    oled_write_cmd(0xA1);   /* segment remap */
    oled_write_cmd(0xC8);   /* COM 扫描方向 */
    oled_write_cmd(0xDA);   /* COM 引脚配置 */
    oled_write_cmd(0x12);
    oled_write_cmd(0x81);   /* contrast */
    oled_write_cmd(0xCF);
    oled_write_cmd(0xD9);   /* pre-charge */
    oled_write_cmd(0xF1);
    oled_write_cmd(0xDB);   /* VCOM detect */
    oled_write_cmd(0x40);
    oled_write_cmd(0xA4);   /* 显示来自 RAM */
    oled_write_cmd(0xA6);   /* 正常显示 */
    OLED_Clear();
    OLED_Update();
    oled_write_cmd(0xAF);   /* display on */
}

/* ---------------- 绘图 (字形行扫描 -> GRAM 列扫描转置) ---------------- */

/* 取行扫描字模中 (col,row) 像素: 每 2 字节为一行, 高字节在左 */
#define GLYPH_PIX(g, w, col, row) \
    (((g)[(row) * ((w) >> 3) + ((col) >> 3)] >> (7U - ((col) & 7U))) & 1U)

static void blit8x16(const uint8_t *g, uint8_t x, uint8_t page)
{
    uint8_t col;
    uint8_t lo = 0U, hi = 0U;
    for (col = 0U; col < 8U; col++)
    {
        uint8_t r;
        for (r = 0U; r < 8U; r++)
        {
            if (GLYPH_PIX(g, 8, col, r) != 0U)          { lo |= (uint8_t)(1U << r); }
            if (GLYPH_PIX(g, 8, col, (uint8_t)(r + 8U)) != 0U) { hi |= (uint8_t)(1U << r); }
        }
        oled_buf[page][x + col] = lo;
        oled_buf[(uint8_t)(page + 1U)][x + col] = hi;
        lo = 0U; hi = 0U;
    }
}

static void blit16x16(const uint8_t *g, uint8_t x, uint8_t page)
{
    uint8_t col;
    uint8_t lo = 0U, hi = 0U;
    for (col = 0U; col < 16U; col++)
    {
        uint8_t r;
        for (r = 0U; r < 8U; r++)
        {
            if (GLYPH_PIX(g, 16, col, r) != 0U)          { lo |= (uint8_t)(1U << r); }
            if (GLYPH_PIX(g, 16, col, (uint8_t)(r + 8U)) != 0U) { hi |= (uint8_t)(1U << r); }
        }
        oled_buf[page][x + col] = lo;
        oled_buf[(uint8_t)(page + 1U)][x + col] = hi;
        lo = 0U; hi = 0U;
    }
}

void OLED_PutASCII(uint8_t x, uint8_t page, char c)
{
    if ((c < (char)OLED_ASCII_FIRST) || (c > (char)(OLED_ASCII_FIRST + OLED_ASCII_COUNT - 1)))
    {
        return;
    }
    blit8x16(OLED_ASCII_8x16[(uint8_t)c - OLED_ASCII_FIRST], x, page);
}

void OLED_Print(uint8_t x, uint8_t page, const char *s)
{
    while (*s != '\0')
    {
        OLED_PutASCII(x, page, *s);
        x = (uint8_t)(x + 8U);
        s++;
    }
}

void OLED_PrintU(uint8_t x, uint8_t page, uint32_t v)
{
    char buf[11];
    uint8_t i = 10U;
    buf[10] = '\0';
    do
    {
        i--;
        buf[i] = (char)('0' + (v % 10U));
        v /= 10U;
    } while ((v != 0U) && (i != 0U));
    OLED_Print(x, page, &buf[i]);
}

void OLED_PrintU2(uint8_t x, uint8_t page, uint32_t v)
{
    char buf[3];
    v %= 100U;
    buf[0] = (char)('0' + (v / 10U));
    buf[1] = (char)('0' + (v % 10U));
    buf[2] = '\0';
    OLED_Print(x, page, buf);
}

void OLED_PutCN(uint8_t idx, uint8_t x, uint8_t page)
{
    if (idx >= CN_NUM) { return; }
    blit16x16(OLED_CN_16x16[idx], x, page);
}
