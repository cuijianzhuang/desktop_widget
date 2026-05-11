/**
 * LVGL 配置文件 - 针对 ESP32-S3-Touch-AMOLED-1.8 优化
 * 适用 LVGL v9.x
 *
 * 注意：
 *   · LV_COLOR_16_SWAP 在 v9 中已移除，字节序交换改为运行时设置：
 *       lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAP);
 *   · LV_USE_SCALE / LV_USE_LINE 在 v9 中默认已启用，无需在此声明
 *   · LV_USE_METER 已从 v9 中移除（用 lv_scale 替代）
 */
#if 1  /* 激活此文件 */

#define LV_COLOR_DEPTH 16

/* LVGL 内部堆放入 PSRAM，避免 DRAM 溢出。
 * ps_malloc / ps_realloc 由 ESP32 Arduino 框架提供，
 * 优先从 SPIRAM 分配，PSRAM 不足时回退到内部 SRAM。
 * LV_MEM_SIZE 在 LV_MEM_CUSTOM=1 时无效，可删去，此处保留仅供参考。 */
#define LV_MEM_CUSTOM          1
#define LV_MEM_CUSTOM_INCLUDE  <Arduino.h>
#define LV_MEM_CUSTOM_ALLOC    ps_malloc
#define LV_MEM_CUSTOM_FREE     free
#define LV_MEM_CUSTOM_REALLOC  ps_realloc

/* v9 中刷新周期的正确宏名（v8 的 LV_DISP_DEF_REFR_PERIOD 已改名）*/
#define LV_DEF_REFR_PERIOD 16       /* ~60fps */
#define LV_INDEV_DEF_READ_PERIOD 16

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_LOG 0

/* Flush 内需 lv_draw_sw_rotate，须启用软件绘制核心 */
#define LV_USE_DRAW_SW 1

/* 字体 */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_36 1   /* 时钟屏表盘外数字时间 */
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* 中文字体支持 */
#define LV_USE_FONT_SUBPX 1
#define LV_FONT_SUBPX_BGR 0

/* 控件（v9 中 LV_USE_SCALE / LV_USE_LINE 默认为 1，不需要重复声明）*/
#define LV_USE_ARC      1
#define LV_USE_BAR      1
#define LV_USE_BTN      1
#define LV_USE_CHART    1
#define LV_USE_LABEL    1
#define LV_USE_OBJ      1
#define LV_USE_TABVIEW  1
#define LV_USE_SPINNER  1

/* 标签内联颜色重绘（传感器屏图例使用 lv_label_set_recolor）*/
#define LV_LABEL_TEXT_SELECTION 1

/* 文件系统（SD卡示例用，可选）*/
#define LV_USE_FS_STDIO 0

#endif /* 激活此文件 */
