#pragma once
#include "pin_config.h"   // LCD_WIDTH / LCD_HEIGHT 在此定义，避免重复

/**
 * 运行时可配置参数的编译期默认值
 * 正式配置通过 Web 后台（http://设备IP）修改，保存在 NVS 中
 *
 * Apple Watch 风格配色（iOS System Colors on AMOLED）
 */

// ── iOS System Colors ─────────────────────────────────────────────────────────
#define COLOR_BG         0x000000   // AMOLED 纯黑
#define COLOR_SURFACE    0x1C1C1E   // iOS elevated surface（卡片/胶囊背景）
#define COLOR_SURFACE2   0x2C2C2E   // iOS grouped fill（深层分组背景）
#define COLOR_SEPARATOR  0x38383A   // 分割线

#define COLOR_PRIMARY    0x0A84FF   // iOS Blue（主强调色，标题/进度）
#define COLOR_ACCENT     0xFF375F   // iOS Pink/Red（秒针、危险提示）
#define COLOR_GREEN      0x30D158   // iOS Green（电量/活动环·Move）
#define COLOR_YELLOW     0xFFD60A   // iOS Yellow（警告/UV）
#define COLOR_ORANGE     0xFF9F0A   // iOS Orange（天气·高温）
#define COLOR_TEAL       0x5AC8FA   // iOS Light Blue（次强调）

#define COLOR_TEXT       0xFFFFFF   // 主文字（白）
#define COLOR_SUBTEXT    0x8E8E93   // 次级文字（iOS Gray）
#define COLOR_DIM        0x48484A   // 最弱提示文字
