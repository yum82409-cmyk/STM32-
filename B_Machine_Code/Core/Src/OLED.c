#include "OLED.h"
#include "OLED_Font.h" // 确保你的字库文件名是这个

// 简易延时
void OLED_Delay(void) {
    volatile uint8_t t = 500;
    while(t--);
}

// I2C 起始信号
void IIC_Start(void) {
    OLED_SCL_Set();
    OLED_SDA_Set();
    OLED_Delay();
    OLED_SDA_Clr();
    OLED_Delay();
    OLED_SCL_Clr();
    OLED_Delay();
}

// I2C 停止信号
void IIC_Stop(void) {
    OLED_SCL_Clr();
    OLED_SDA_Clr();
    OLED_Delay();
    OLED_SCL_Set();
    OLED_Delay();
    OLED_SDA_Set();
    OLED_Delay();
}

// I2C 写一个字节
void Write_IIC_Byte(unsigned char IIC_Byte) {
    unsigned char i, m, da;
    da = IIC_Byte;
    OLED_SCL_Clr();
    for(i = 0; i < 8; i++) {
        m = da & 0x80;
        if(m == 0x80) OLED_SDA_Set();
        else OLED_SDA_Clr();
        da = da << 1;
        OLED_Delay();
        OLED_SCL_Set();
        OLED_Delay();
        OLED_SCL_Clr();
        OLED_Delay();
    }
    // 简易跳过应答检测
    OLED_SCL_Set();
    OLED_Delay();
    OLED_SCL_Clr();
    OLED_Delay();
}

// 写命令
void OLED_WR_Cmd(unsigned char dat) {
    IIC_Start();
    Write_IIC_Byte(0x7A); // OLED 地址
    Write_IIC_Byte(0x00); // 写命令寄存器
    Write_IIC_Byte(dat);
    IIC_Stop();
}

// 写数据
void OLED_WR_Data(unsigned char dat) {
    IIC_Start();
    Write_IIC_Byte(0x7A); // OLED 地址
    Write_IIC_Byte(0x40); // 写数据寄存器
    Write_IIC_Byte(dat);
    IIC_Stop();
}

// 设置光标坐标
void OLED_Set_Pos(unsigned char x, unsigned char y) {
    OLED_WR_Cmd(0xb0 + y);
    OLED_WR_Cmd(((x & 0xf0) >> 4) | 0x10);
    OLED_WR_Cmd((x & 0x0f) | 0x01);
}

// 清屏
void OLED_Clear(void) {
    unsigned char i, n;
    for(i = 0; i < 8; i++) {
        OLED_WR_Cmd(0xb0 + i);
        OLED_WR_Cmd(0x00);
        OLED_WR_Cmd(0x10);
        for(n = 0; n < 128; n++) OLED_WR_Data(0);
    }
}

// 初始化（完全依赖 HAL 库引脚，不调用老版标准库）
void OLED_Init(void) {
    HAL_Delay(100); // 稍等上电稳定
    
    OLED_WR_Cmd(0xAE); // 关闭显示
    OLED_WR_Cmd(0x20); // 寻址模式
    OLED_WR_Cmd(0x10); // 页寻址模式
    OLED_WR_Cmd(0xb0); // 设置页地址
    OLED_WR_Cmd(0xc8); // COM 扫描方向
    OLED_WR_Cmd(0x00); // 低列地址
    OLED_WR_Cmd(0x10); // 高列地址
    OLED_WR_Cmd(0x40); // 起始行
    OLED_WR_Cmd(0x81); // 对比度
    OLED_WR_Cmd(0xff); 
    OLED_WR_Cmd(0xa1); // 段重映射
    OLED_WR_Cmd(0xa6); // 正常显示
    OLED_WR_Cmd(0xa8); // 复用率
    OLED_WR_Cmd(0x1F); 
    OLED_WR_Cmd(0xa4); // 全局显示开启
    OLED_WR_Cmd(0xd3); // 显示偏移
    OLED_WR_Cmd(0x00); 
    OLED_WR_Cmd(0xd5); // 振荡器频率
    OLED_WR_Cmd(0xf0); 
    OLED_WR_Cmd(0xd9); // 预充电周期
    OLED_WR_Cmd(0x22); 
    OLED_WR_Cmd(0xda); // COM 引脚配置
    OLED_WR_Cmd(0x02);
    OLED_WR_Cmd(0xdb); // VCOMH
    OLED_WR_Cmd(0x20); 
    OLED_WR_Cmd(0x8d); // 电荷泵
    OLED_WR_Cmd(0x14); 
    OLED_WR_Cmd(0xaf); // 开启显示
    OLED_Clear();
}


// 显示单个字符 (完美适配二维数组 OLED_F8x16)
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr) {
    unsigned char c = 0, i = 0;
    c = chr - ' '; 
    if(x > 120) { x = 0; y = y + 2; }
    
    OLED_Set_Pos(x, y);
    // 画上半部分 (前 8 个字节)
    for(i = 0; i < 8; i++) OLED_WR_Data(OLED_F8x16[c][i]);
    
    OLED_Set_Pos(x, y + 1);
    // 画下半部分 (后 8 个字节)
    for(i = 0; i < 8; i++) OLED_WR_Data(OLED_F8x16[c][i + 8]);
}

// 显示字符串
void OLED_ShowString(uint8_t x, uint8_t y, char *chr) {
    unsigned char j = 0;
    while(chr[j] != '\0') {
        OLED_ShowChar(x, y, chr[j]);
        x += 8;
        if(x > 120) { x = 0; y += 2; }
        j++;
    }
}

