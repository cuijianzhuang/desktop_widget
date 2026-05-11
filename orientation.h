#pragma once
#include <math.h>

/**
 * 屏幕自动旋转 — 基于 QMI8658 加速度计重力向量判断设备姿态
 *
 * 不依赖 LVGL 判断本身。旋转切换由调用方完成：
 *   lv_display_set_rotation(disp, current())   → 交换逻辑宽高
 *   lv_flush 内 lv_draw_sw_rotate              → LVGL v9 实际旋转像素（勿再叠加 gfx->setRotation）
 *
 * 轴映射：先按重力在芯片 X/Y 上的象限得到“自然”0–3，再在 fromAccel 末尾做 180°
 * 折转与 AMOLED 扫描方向一致，对应 lv_display_rotation_t 0–3。
 *
 * 防抖：连续 STABLE_TICKS × 50ms ≈ 750ms 稳定后才切换，避免抖动触发重建。
 *
 * ESP32-S3-Touch-AMOLED-1.8：
 *   · 加速度计芯片 X/Y 与面板横竖常用约定相差 90° → updateMapped(chip_x, chip_y)
 *   · 面板/驱动与重力“上”相差 180° → fromAccel 内横竖象限对调（0↔2，1↔3）
 */
namespace Orientation {

static constexpr int STABLE_TICKS = 15;

// 0-3 对应 lv_display_rotation_t / GFX 角度约定（本项目 flush 软件旋转，不再写 gfx->setRotation）
static int s_cur  = 0;
static int s_cand = 0;
static int s_cnt  = 0;

inline void init() {
    s_cur = 0; s_cand = 0; s_cnt = 0;
}

/** 当前旋转索引（0-3），传给 lv_display_set_rotation */
inline int current() { return s_cur; }

/** 是否处于横屏（1 或 3）*/
inline bool isLandscape() { return s_cur == 1 || s_cur == 3; }

/** 从加速度分量推断期望旋转，平放/模糊时维持当前方向 */
static int fromAccel(float ax, float ay) {
    constexpr float T = 0.5f;
    const float absx = fabsf(ax), absy = fabsf(ay);
    if (absx < T && absy < T) return s_cur;   // 平放，保持
    if (absx > absy)
        return (ax > 0) ? 3 : 1;              // 横屏（相对 IMU 象限整体转 180°）
    return (ay > 0) ? 0 : 2;                  // 竖屏正立 / 倒置
}

/**
 * 每 50ms 调用一次（imu_ok 为 true 时）。
 * 返回 true：旋转已稳定切换，调用方需 lv_display_set_rotation 并重建 UI。
 */
inline bool update(float ax, float ay) {
    const int cand = fromAccel(ax, ay);
    if (cand == s_cur) { s_cnt = 0; return false; }
    if (cand != s_cand) { s_cand = cand; s_cnt = 0; }
    if (++s_cnt < STABLE_TICKS) return false;
    s_cnt = 0;
    s_cur = cand;
    return true;
}

/** QMI8658 芯片轴 → fromAccel 假定的屏幕轴（交换 X/Y）*/
inline bool updateMapped(float imx, float imy) { return update(imy, imx); }

} // namespace Orientation
