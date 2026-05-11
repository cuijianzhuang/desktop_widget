#pragma once
#include <lvgl.h>
#include <math.h>
#include "config.h"

/**
 * 传感器屏 — Apple Watch Activity Rings 风格  v1.2
 *
 * v1.2 自适应旋转：
 *   create(parent, W, H) 接受当前逻辑分辨率
 *   所有 y 坐标按 sy = H/448 比例缩放，宽度使用实际 W
 *   destroy() 重置所有指针，供旋转重建使用
 *
 * 布局基准（368×448）：
 *   y=0-56    标题
 *   y=56-220  三轴弧形仪表（水平排列，X=红/Y=绿/Z=蓝）
 *   y=220-350 加速度 ECG 折线图
 *   y=350-412 实时数值 + 电量条
 *   y=412-448 底部导航提示
 */

namespace ScreenSensor {

static lv_obj_t *scr;

// 三轴弧形仪表
static lv_obj_t *arc_x, *arc_y, *arc_z;
static lv_obj_t *lbl_xv, *lbl_yv, *lbl_zv;

// ECG 折线图
static lv_chart_series_t *ser_ax, *ser_ay, *ser_az;
static lv_obj_t *chart_accel;

// 电量条
static lv_obj_t *bar_battery;
static lv_obj_t *lbl_battery;

// 步数
static lv_obj_t *lbl_steps;
static long  step_count     = 0;
static float last_accel_mag = 0;
static unsigned long last_step_time = 0;

/** 重置所有 UI 指针（由调用方 lv_obj_delete(parent) 完成实际对象销毁） */
static void destroy() {
    scr          = nullptr;
    arc_x        = nullptr; arc_y     = nullptr; arc_z     = nullptr;
    lbl_xv       = nullptr; lbl_yv    = nullptr; lbl_zv    = nullptr;
    ser_ax       = nullptr; ser_ay    = nullptr; ser_az    = nullptr;
    chart_accel  = nullptr;
    bar_battery  = nullptr; lbl_battery = nullptr;
    lbl_steps    = nullptr;
}

// 轴仪表辅助（270° 半圆弧形仪表）
// card_w / card_h 由调用方根据屏幕尺寸计算后传入
static lv_obj_t *_make_gauge(lv_obj_t *parent, lv_obj_t **arc_out, lv_obj_t **lbl_out,
                              int x_ofs, int y_ofs, int card_w, int card_h,
                              uint32_t color, const char *title) {
    // 仪表卡片背景
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, card_w, card_h);
    lv_obj_align(card, LV_ALIGN_TOP_MID, x_ofs, y_ofs);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // 弧形仪表（固定 80px，卡片高度变化时上下留白自适应）
    *arc_out = lv_arc_create(card);
    lv_obj_set_size(*arc_out, 80, 80);
    lv_obj_align(*arc_out, LV_ALIGN_TOP_MID, 0, 10);
    lv_arc_set_rotation(*arc_out, 135);
    lv_arc_set_bg_angles(*arc_out, 0, 270);
    lv_arc_set_range(*arc_out, 0, 100);
    lv_arc_set_value(*arc_out, 50);   // 中点（0g）
    lv_obj_set_style_opa(*arc_out, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(*arc_out, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(*arc_out, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_color(*arc_out, lv_color_hex(COLOR_SURFACE2), LV_PART_MAIN);
    lv_obj_set_style_arc_color(*arc_out, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(*arc_out, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(*arc_out, 8, LV_PART_INDICATOR);

    // 轴标签（弧内居中）
    lv_obj_t *axis_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(axis_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(axis_lbl, lv_color_hex(color), 0);
    lv_label_set_text(axis_lbl, title);
    lv_obj_align(axis_lbl, LV_ALIGN_TOP_MID, 0, 46);

    // 数值标签（卡片中下）
    *lbl_out = lv_label_create(card);
    lv_obj_set_style_text_font(*lbl_out, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(*lbl_out, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(*lbl_out, "0.00g");
    lv_obj_align(*lbl_out, LV_ALIGN_BOTTOM_MID, 0, -10);

    return card;
}

static void create(lv_obj_t *parent, int W, int H) {
    // y 方向等比缩放系数（基准 448px）
    const float sy = (float)H / 448.0f;
    // 辅助：将原始 y 坐标缩放为整数
    auto Y  = [sy](int y) -> int { return (int)roundf((float)y * sy); };
    auto SH = [sy](int h) -> int { return (int)roundf((float)h * sy); };
    // x 方向等比缩放系数（基准 368px）
    const float sx = (float)W / 368.0f;
    auto SX = [sx](int x) -> int { return (int)roundf((float)x * sx); };

    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, W, H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── 标题 ─────────────────────────────────────────────────────────────────
    lv_obj_t *lbl_title = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(lbl_title, LV_SYMBOL_SETTINGS "  Sensors");
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, Y(14));

    // ── 三轴弧形仪表（水平排列）──────────────────────────────────────────────
    // X:红色(Move) Y:绿色(Exercise) Z:蓝色(Stand) — AW Activity 配色
    const int card_w = SX(108);
    const int card_h = SH(154);
    const int g_xofs = SX(120);
    _make_gauge(scr, &arc_x, &lbl_xv, -g_xofs, Y(46), card_w, card_h, COLOR_ACCENT,  "X");
    _make_gauge(scr, &arc_y, &lbl_yv,        0, Y(46), card_w, card_h, COLOR_GREEN,   "Y");
    _make_gauge(scr, &arc_z, &lbl_zv,  g_xofs, Y(46), card_w, card_h, COLOR_PRIMARY, "Z");

    // ── 加速度 ECG 折线图（AW 心率风格：黑底、亮线）──────────────────────────
    chart_accel = lv_chart_create(scr);
    lv_obj_set_size(chart_accel, W - 8, SH(110));
    lv_obj_align(chart_accel, LV_ALIGN_TOP_MID, 0, Y(212));
    lv_obj_set_style_bg_color(chart_accel, lv_color_hex(0x050505), 0);
    lv_obj_set_style_border_color(chart_accel, lv_color_hex(COLOR_SEPARATOR), 0);
    lv_obj_set_style_border_width(chart_accel, 1, 0);
    lv_obj_set_style_radius(chart_accel, 10, 0);
    lv_chart_set_type(chart_accel, LV_CHART_TYPE_LINE);
    lv_chart_set_axis_range(chart_accel, LV_CHART_AXIS_PRIMARY_Y, -20, 20);
    lv_chart_set_point_count(chart_accel, 60);
    lv_chart_set_div_line_count(chart_accel, 2, 0);
    lv_obj_set_style_line_color(chart_accel, lv_color_hex(0x1C1C1E), 0);
    ser_ax = lv_chart_add_series(chart_accel, lv_color_hex(COLOR_ACCENT),  LV_CHART_AXIS_PRIMARY_Y);
    ser_ay = lv_chart_add_series(chart_accel, lv_color_hex(COLOR_GREEN),   LV_CHART_AXIS_PRIMARY_Y);
    ser_az = lv_chart_add_series(chart_accel, lv_color_hex(COLOR_PRIMARY), LV_CHART_AXIS_PRIMARY_Y);

    // 图例（recolor）
    lv_obj_t *legend = lv_label_create(scr);
    lv_obj_set_style_text_font(legend, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(legend, lv_color_hex(COLOR_SUBTEXT), 0);
    lv_label_set_text(legend, "#FF375F X#   #30D158 Y#   #0A84FF Z#");
    lv_label_set_recolor(legend, true);
    lv_obj_align(legend, LV_ALIGN_TOP_MID, 0, Y(328));

    // ── 电量条 ───────────────────────────────────────────────────────────────
    lbl_battery = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_battery, lv_color_hex(COLOR_GREEN), 0);
    lv_label_set_text(lbl_battery, LV_SYMBOL_BATTERY_FULL "  --% (--mV)");
    lv_obj_align(lbl_battery, LV_ALIGN_TOP_MID, 0, Y(352));

    bar_battery = lv_bar_create(scr);
    lv_obj_set_size(bar_battery, W - 32, 6);
    lv_obj_align(bar_battery, LV_ALIGN_TOP_MID, 0, Y(374));
    lv_obj_set_style_bg_color(bar_battery, lv_color_hex(COLOR_SURFACE2), 0);
    lv_obj_set_style_bg_color(bar_battery, lv_color_hex(COLOR_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_battery, 3, 0);
    lv_obj_set_style_radius(bar_battery, 3, LV_PART_INDICATOR);
    lv_bar_set_range(bar_battery, 0, 100);
    lv_bar_set_value(bar_battery, 0, LV_ANIM_OFF);

    // ── 步数 ─────────────────────────────────────────────────────────────────
    lbl_steps = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_steps, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_steps, lv_color_hex(COLOR_ACCENT), 0);
    lv_label_set_text(lbl_steps, LV_SYMBOL_SHUFFLE "  Steps: 0");
    lv_obj_align(lbl_steps, LV_ALIGN_TOP_MID, 0, Y(392));

    // 底部导航提示
    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_DIM), 0);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Weather    Clock " LV_SYMBOL_RIGHT);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void update(float ax, float ay, float az,
                   float gx, float gy, float gz,
                   int batt_pct, int batt_mv) {
    if (!arc_x || !chart_accel || !lbl_battery) return;  // 重建期间保护

    // 弧形仪表：-2g~+2g → 0~100%（中点50%=0g）
    auto axisToArc = [](float v) -> int {
        int pct = (int)((v + 2.0f) / 4.0f * 100.0f);
        return pct < 0 ? 0 : pct > 100 ? 100 : pct;
    };
    lv_arc_set_value(arc_x, axisToArc(ax));
    lv_arc_set_value(arc_y, axisToArc(ay));
    lv_arc_set_value(arc_z, axisToArc(az));

    char buf[24];
    snprintf(buf, sizeof(buf), "%+.2fg", ax); lv_label_set_text(lbl_xv, buf);
    snprintf(buf, sizeof(buf), "%+.2fg", ay); lv_label_set_text(lbl_yv, buf);
    snprintf(buf, sizeof(buf), "%+.2fg", az); lv_label_set_text(lbl_zv, buf);

    // ECG 折线图
    lv_chart_set_next_value(chart_accel, ser_ax, (int16_t)(ax * 10));
    lv_chart_set_next_value(chart_accel, ser_ay, (int16_t)(ay * 10));
    lv_chart_set_next_value(chart_accel, ser_az, (int16_t)(az * 10));
    lv_chart_refresh(chart_accel);

    // 电量（低电量变红）
    lv_color_t bc = lv_color_hex(batt_pct <= 20 ? COLOR_ACCENT : COLOR_GREEN);
    const char *ico = batt_pct > 80 ? LV_SYMBOL_BATTERY_FULL :
                      batt_pct > 60 ? LV_SYMBOL_BATTERY_3 :
                      batt_pct > 40 ? LV_SYMBOL_BATTERY_2 :
                      batt_pct > 20 ? LV_SYMBOL_BATTERY_1 : LV_SYMBOL_BATTERY_EMPTY;
    snprintf(buf, sizeof(buf), "%s  %d%% (%dmV)", ico, batt_pct, batt_mv);
    lv_label_set_text(lbl_battery, buf);
    lv_obj_set_style_text_color(lbl_battery, bc, 0);
    lv_bar_set_value(bar_battery, batt_pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar_battery, bc, LV_PART_INDICATOR);

    // 简易步数检测
    float mag   = sqrtf(ax*ax + ay*ay + az*az);
    float delta = fabsf(mag - last_accel_mag);
    if (delta > 0.3f && (millis() - last_step_time) > 300) {
        step_count++;
        last_step_time = millis();
    }
    last_accel_mag = mag;

    snprintf(buf, sizeof(buf), LV_SYMBOL_SHUFFLE "  Steps: %ld", step_count);
    lv_label_set_text(lbl_steps, buf);
}

} // namespace ScreenSensor
