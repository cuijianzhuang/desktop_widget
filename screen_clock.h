#pragma once
#include <lvgl.h>
#include <time.h>
#include <math.h>
#include "config.h"

/**
 * 时钟屏 — Apple Watch Infograph Modular 风格  v1.7
 *
 * v1.7 自适应旋转：
 *   create(parent, W, H) 接受当前逻辑分辨率
 *   · 竖屏（H > W）：原始 340px 表盘 + 表盘下方数字时间 + 导航提示
 *   · 横屏（W > H）：表盘充满高度（DIAL = H-8），隐藏表盘外文字
 *   destroy() 重置所有指针，供旋转重建使用
 *
 * z-order（dial_bg 子控件，低→高）：
 *   lbl_date → 时位数字 → scale_face(刻度) → hand_hour/min/sec → ctr
 */

namespace ScreenClock {

static lv_obj_t *scr;
static lv_obj_t *scale_face;
static lv_obj_t *hand_hour, *hand_min, *hand_sec;
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_date;

static int      s_rtc_sec   = 0;
static uint32_t s_update_ms = 0;

// lv_line 点数组（需在 set_points 后持续有效）
static lv_point_precise_t pts_hour[2], pts_min[2], pts_sec[2];

// 当前枢轴（create 时写入，setNeedle/update/updateSec 使用）
static int s_cx = 170, s_cy = 170;

static const char *DAY_SHORT[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
static const char *MON_SHORT[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                   "JUL","AUG","SEP","OCT","NOV","DEC"};

/** 更新表针端点。value: 0–3600 对应一圈；0 = 12 点 */
static void setNeedle(lv_obj_t *hand, lv_point_precise_t *pts, int length, int32_t value) {
    float theta = (270.0f + (float)value / 10.0f) * ((float)M_PI / 180.0f);
    pts[0].x = (lv_value_precise_t)s_cx;
    pts[0].y = (lv_value_precise_t)s_cy;
    pts[1].x = (lv_value_precise_t)(s_cx + length * cosf(theta));
    pts[1].y = (lv_value_precise_t)(s_cy + length * sinf(theta));
    lv_line_set_points(hand, pts, 2);
}

/** 重置所有 UI 指针（由调用方 lv_obj_delete(parent) 完成实际对象销毁） */
static void destroy() {
    scr = nullptr; scale_face = nullptr;
    hand_hour = nullptr; hand_min = nullptr; hand_sec = nullptr;
    lbl_time = nullptr; lbl_date = nullptr;
}

static void create(lv_obj_t *parent, int W, int H) {
    const bool landscape = (W > H);

    // 表盘尺寸与位置
    const int DIAL = landscape ? (H - 8) : 340;
    const int CX   = DIAL / 2;
    const int CY   = DIAL / 2;
    const int NR   = DIAL * 125 / 340;   // 时位数字半径（等比缩放）
    const int dial_y = landscape ? (H - DIAL) / 2 : 18;

    s_cx = CX; s_cy = CY;

    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, W, H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── 圆形表盘底板 ──────────────────────────────────────────────────────────
    lv_obj_t *dial_bg = lv_obj_create(scr);
    lv_obj_set_size(dial_bg, DIAL, DIAL);
    lv_obj_align(dial_bg, LV_ALIGN_TOP_MID, 0, dial_y);
    lv_obj_set_style_bg_color(dial_bg, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_border_width(dial_bg, 0, 0);
    lv_obj_set_style_outline_color(dial_bg, lv_color_hex(0x48484A), 0);
    lv_obj_set_style_outline_width(dial_bg, 2, 0);
    lv_obj_set_style_outline_pad(dial_bg, 0, 0);
    lv_obj_set_style_radius(dial_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(dial_bg, 0, 0);
    lv_obj_clear_flag(dial_bg, LV_OBJ_FLAG_SCROLLABLE);

    // ── Layer 0：日期文字 ─────────────────────────────────────────────────────
    lbl_date = lv_label_create(dial_bg);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(0xAEAEB2), 0);
    lv_label_set_text(lbl_date, "--- --- --");
    lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, -(DIAL * 80 / 340));

    // ── Layer 1：时位数字 1–12 ────────────────────────────────────────────────
    static const char *HOUR_STRS[12] = {
        "12","1","2","3","4","5","6","7","8","9","10","11"
    };
    for (int h = 0; h < 12; h++) {
        float theta = (float)h / 12.0f * 2.0f * (float)M_PI - (float)M_PI / 2.0f;
        int dx = (int)roundf(NR * cosf(theta));
        int dy = (int)roundf(NR * sinf(theta));
        lv_obj_t *nl = lv_label_create(dial_bg);
        lv_obj_set_style_text_font(nl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(nl, lv_color_hex(0xE5E5EA), 0);
        lv_label_set_text(nl, HOUR_STRS[h]);
        lv_obj_align(nl, LV_ALIGN_CENTER, dx, dy);
    }

    // ── Layer 2：刻度环（scale_face，仅绘制刻度，不持有表针）─────────────────
    scale_face = lv_scale_create(dial_bg);
    lv_obj_set_size(scale_face, DIAL, DIAL);
    lv_obj_align(scale_face, LV_ALIGN_CENTER, 0, 0);
    lv_scale_set_mode(scale_face, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(scale_face, 0, 3600);
    lv_scale_set_angle_range(scale_face, 360);
    lv_scale_set_rotation(scale_face, 270);
    lv_scale_set_total_tick_count(scale_face, 61);
    lv_scale_set_major_tick_every(scale_face, 5);
    lv_scale_set_label_show(scale_face, false);
    lv_obj_set_style_bg_opa(scale_face, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scale_face, 0, 0);
    lv_obj_set_style_outline_width(scale_face, 0, 0);
    lv_obj_set_style_pad_all(scale_face, 0, 0);
    // pad 缩小 → 刻度更靠近表盘边缘，更易辨认
    lv_obj_set_style_pad_top(scale_face, 6, 0);
    lv_obj_set_style_pad_bottom(scale_face, 6, 0);
    lv_obj_set_style_pad_left(scale_face, 6, 0);
    lv_obj_set_style_pad_right(scale_face, 6, 0);
    // 分钟刻度（次刻度）：更亮、更粗
    lv_obj_set_style_line_color(scale_face, lv_color_hex(0x636366), LV_PART_ITEMS);
    lv_obj_set_style_length(scale_face, 8,  LV_PART_ITEMS);
    lv_obj_set_style_line_width(scale_face, 2, LV_PART_ITEMS);
    // 5 分钟主刻度：显著加长加亮
    lv_obj_set_style_line_color(scale_face, lv_color_hex(0xC7C7CC), LV_PART_INDICATOR);
    lv_obj_set_style_length(scale_face, 16, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(scale_face, 3, LV_PART_INDICATOR);

    // ── Layer 3：三根表针（dial_bg 直接子控件，枢轴硬编码在 (CX, CY)）──────────
    const int L_HR = DIAL * 88  / 340;
    const int L_MN = DIAL * 134 / 340;
    const int L_SC = DIAL * 152 / 340;

    hand_hour = lv_line_create(dial_bg);
    lv_obj_set_style_line_color(hand_hour, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_width(hand_hour, 8, 0);
    lv_obj_set_style_line_rounded(hand_hour, true, 0);
    pts_hour[0] = {(lv_value_precise_t)CX, (lv_value_precise_t)CY};
    pts_hour[1] = {(lv_value_precise_t)CX, (lv_value_precise_t)(CY - L_HR)};
    lv_line_set_points(hand_hour, pts_hour, 2);

    hand_min = lv_line_create(dial_bg);
    lv_obj_set_style_line_color(hand_min, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_line_width(hand_min, 5, 0);
    lv_obj_set_style_line_rounded(hand_min, true, 0);
    pts_min[0] = {(lv_value_precise_t)CX, (lv_value_precise_t)CY};
    pts_min[1] = {(lv_value_precise_t)CX, (lv_value_precise_t)(CY - L_MN)};
    lv_line_set_points(hand_min, pts_min, 2);

    hand_sec = lv_line_create(dial_bg);
    lv_obj_set_style_line_color(hand_sec, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_line_width(hand_sec, 2, 0);
    lv_obj_set_style_line_rounded(hand_sec, true, 0);
    pts_sec[0] = {(lv_value_precise_t)CX, (lv_value_precise_t)CY};
    pts_sec[1] = {(lv_value_precise_t)CX, (lv_value_precise_t)(CY - L_SC)};
    lv_line_set_points(hand_sec, pts_sec, 2);

    // ── Layer 4：表心红点 ─────────────────────────────────────────────────────
    lv_obj_t *ctr = lv_obj_create(dial_bg);
    lv_obj_set_size(ctr, 16, 16);
    lv_obj_align(ctr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ctr, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_color(ctr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(ctr, 2, 0);
    lv_obj_set_style_radius(ctr, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(ctr, 0, 0);

    // ── 竖屏额外元素（横屏表盘已撑满，无空间放置）──────────────────────────────
    if (!landscape) {
        lbl_time = lv_label_create(scr);
        lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(lbl_time, lv_color_hex(COLOR_TEXT), 0);
        lv_label_set_text(lbl_time, "00:00");
        lv_obj_align(lbl_time, LV_ALIGN_BOTTOM_MID, 0, -32);

        lv_obj_t *hint = lv_label_create(scr);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_DIM), 0);
        lv_label_set_text(hint, LV_SYMBOL_LEFT " Sensor    Weather " LV_SYMBOL_RIGHT);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
    } else {
        lbl_time = nullptr;
    }
}

static void update(const struct tm &t) {
    if (!hand_hour || !hand_min || !hand_sec || !lbl_date) return;  // 重建期间保护

    int min  = t.tm_min;
    int hour = t.tm_hour % 12;

    const int DIAL = s_cx * 2;
    const int L_HR = DIAL * 88  / 340;
    const int L_MN = DIAL * 134 / 340;
    const int L_SC = DIAL * 152 / 340;

    setNeedle(hand_hour, pts_hour, L_HR, hour * 300 + min * 5);
    setNeedle(hand_min,  pts_min,  L_MN, min * 60);

    s_rtc_sec   = t.tm_sec;
    s_update_ms = millis();
    setNeedle(hand_sec, pts_sec, L_SC, (int32_t)s_rtc_sec * 60);

    if (lbl_time) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
        lv_label_set_text(lbl_time, buf);
    }

    char dbuf[24];
    snprintf(dbuf, sizeof(dbuf), "%s  %s %d",
             DAY_SHORT[t.tm_wday], MON_SHORT[t.tm_mon], t.tm_mday);
    lv_label_set_text(lbl_date, dbuf);
}

static void updateSec() {
    if (!hand_sec) return;
    const int DIAL = s_cx * 2;
    const int L_SC = DIAL * 152 / 340;
    uint32_t elapsed   = millis() - s_update_ms;
    uint32_t ms_in_min = (uint32_t)s_rtc_sec * 1000UL + elapsed;
    ms_in_min %= 60000UL;
    setNeedle(hand_sec, pts_sec, L_SC,
              (int32_t)(ms_in_min * 3600UL / 60000UL));
}

} // namespace ScreenClock
