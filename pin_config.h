#pragma once

// ── 显示屏 QSPI ───────────────────────────────────────────────────────────────
#define LCD_SDIO0   4
#define LCD_SDIO1   5
#define LCD_SDIO2   6
#define LCD_SDIO3   7
#define LCD_SCLK    11
#define LCD_CS      12
#define LCD_WIDTH   368
#define LCD_HEIGHT  448

// ── I2C 总线（触摸/RTC/IMU共用）──────────────────────────────────────────────
#define IIC_SDA     15
#define IIC_SCL     14

// ── 触摸 FT3168 ───────────────────────────────────────────────────────────────
#define TP_INT      21
#define FT3168_DEVICE_ADDRESS  0x38

// ── 音频 ES8311 ───────────────────────────────────────────────────────────────
#define I2S_MCK_IO  16
#define I2S_BCK_IO  9
#define I2S_WS_IO   45
#define I2S_DI_IO   10
#define I2S_DO_IO   8
#define PA_EN       46

// ── SD 卡 SDMMC ───────────────────────────────────────────────────────────────
#define SD_CLK      2
#define SD_CMD      1
#define SD_DATA     3
