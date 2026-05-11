#pragma once
#include <lvgl.h>
#include "config.h"
#include "weather_client.h"

/**
 * 天气屏 — Apple Watch Weather App 风格  v1.2
 *
 * v1.2 自适应旋转：
 *   create(parent, W, H) 接受当前逻辑分辨率
 *   所有 y 坐标按 sy = H/448 比例缩放，宽度使用实际 W
 *   destroy() 重置所有指针，供旋转重建使用
 *
 * 布局基准（368×448）：
 *   y=0-52   城市栏（深色胶囊居中）
 *   y=52-170 大温度 + 状况文字
 *   y=170-430 三张信息卡：体感温度 / 湿度（含弧形） / 风速
 *   y=430-448 底部导航提示
 */

// ── 天气状况 ID → 可读文字（原在 weather_client.h，展示逻辑归属展示层）──────
static const char *conditionToText(int id) {
    if (id >= 200 && id < 300) return "Thunder";
    if (id >= 300 && id < 400) return "Drizzle";
    if (id >= 500 && id < 510) return "Light Rain";
    if (id >= 510 && id < 520) return "Rain";
    if (id >= 520 && id < 600) return "Heavy Rain";
    if (id >= 600 && id < 700) return "Snow";
    if (id >= 700 && id < 800) return "Haze";
    if (id == 800)             return "Sunny";
    if (id == 801)             return "Few Clouds";
    if (id == 802)             return "Clouds";
    if (id >= 803)             return "Overcast";
    return "N/A";
}

namespace ScreenWeather {

static lv_obj_t *scr;
static lv_obj_t *lbl_city;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_hi_lo;
static lv_obj_t *lbl_condition;
static lv_obj_t *lbl_feels;
static lv_obj_t *lbl_humidity;
static lv_obj_t *lbl_wind;
static lv_obj_t *lbl_update;
static lv_obj_t *arc_humidity;  // 湿度弧形仪表

// 根据天气 ID 返回颜色
static lv_color_t conditionColor(int id) {
    if (id == 800)             return lv_color_hex(COLOR_YELLOW);   // 晴
    if (id >= 801 && id < 804) return lv_color_hex(COLOR_TEAL);     // 多云
    if (id >= 500 && id < 600) return lv_color_hex(COLOR_PRIMARY);  // 雨
    if (id >= 200 && id < 300) return lv_color_hex(0x5E5CE6);       // 雷雨
    if (id >= 600 && id < 700) return lv_color_hex(0xE5E5EA);       // 雪
    return lv_color_hex(COLOR_SUBTEXT);
}

/** 重置所有 UI 指针（由调用方 lv_obj_delete(parent) 完成实际对象销毁） */
static void destroy() {
    scr          = nullptr;
    lbl_city     = nullptr; lbl_temp      = nullptr;
    lbl_hi_lo    = nullptr; lbl_condition = nullptr;
    lbl_feels    = nullptr; lbl_humidity  = nullptr;
    lbl_wind     = nullptr; lbl_update    = nullptr;
    arc_humidity = nullptr;
}

// 信息卡辅助函数（接受实际宽度）
static lv_obj_t *_make_card(lv_obj_t *parent, int W, int y_ofs, int h) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, W - 24, h);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y_ofs);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_hor(card, 16, 0);
    lv_obj_set_style_pad_ver(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void create(lv_obj_t *parent, int W, int H) {
    // y 方向等比缩放系数（基准 448px）
    const float sy = (float)H / 448.0f;
    // 辅助宏：将原始 y 坐标缩放为整数
    auto Y = [sy](int y) -> int { return (int)roundf((float)y * sy); };
    // 高度缩放（向上取整保证内容可见）
    auto SH = [sy](int h) -> int { return (int)roundf((float)h * sy); };

    scr = lv_obj_create(parent);
    lv_obj_set_size(scr, W, H);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── 城市名胶囊（顶部居中）────────────────────────────────────────────────
    lv_obj_t *city_pill = lv_obj_create(scr);
    lv_obj_set_size(city_pill, 200, 36);
    lv_obj_align(city_pill, LV_ALIGN_TOP_MID, 0, Y(10));
    lv_obj_set_style_bg_color(city_pill, lv_color_hex(COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(city_pill, 0, 0);
    lv_obj_set_style_radius(city_pill, 18, 0);
    lv_obj_set_style_pad_all(city_pill, 0, 0);
    lv_obj_clear_flag(city_pill, LV_OBJ_FLAG_SCROLLABLE);

    lbl_city = lv_label_create(city_pill);
    lv_obj_set_style_text_font(lbl_city, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_city, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(lbl_city, LV_SYMBOL_GPS "  Weather");
    lv_obj_align(lbl_city, LV_ALIGN_CENTER, 0, 0);

    // ── 大温度（核心区）──────────────────────────────────────────────────────
    lbl_temp = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_temp, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(lbl_temp, "--");
    lv_obj_align(lbl_temp, LV_ALIGN_TOP_MID, 0, Y(56));

    // 最高/最低温（温度右侧小字）
    lbl_hi_lo = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_hi_lo, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_hi_lo, lv_color_hex(COLOR_SUBTEXT), 0);
    lv_label_set_text(lbl_hi_lo, "H:-- L:--");
    lv_obj_align(lbl_hi_lo, LV_ALIGN_TOP_MID, 0, Y(118));

    // ── 天气状况（颜色随天气变化）────────────────────────────────────────────
    lbl_condition = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_condition, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_condition, lv_color_hex(COLOR_TEAL), 0);
    lv_label_set_text(lbl_condition, "Loading...");
    lv_obj_align(lbl_condition, LV_ALIGN_TOP_MID, 0, Y(144));

    // ── 卡片 1：体感温度 ──────────────────────────────────────────────────────
    lv_obj_t *card1 = _make_card(scr, W, Y(178), SH(52));
    lv_obj_t *ico1 = lv_label_create(card1);
    lv_obj_set_style_text_font(ico1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ico1, lv_color_hex(COLOR_TEAL), 0);
    lv_label_set_text(ico1, LV_SYMBOL_EYE_OPEN "  FEELS LIKE");
    lv_obj_align(ico1, LV_ALIGN_LEFT_MID, 0, 0);

    lbl_feels = lv_label_create(card1);
    lv_obj_set_style_text_font(lbl_feels, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_feels, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(lbl_feels, "-- \xc2\xb0" "C");
    lv_obj_align(lbl_feels, LV_ALIGN_RIGHT_MID, 0, 0);

    // ── 卡片 2：湿度 + 弧形 ──────────────────────────────────────────────────
    lv_obj_t *card2 = _make_card(scr, W, Y(240), SH(80));

    lv_obj_t *ico2 = lv_label_create(card2);
    lv_obj_set_style_text_font(ico2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ico2, lv_color_hex(COLOR_PRIMARY), 0);
    lv_label_set_text(ico2, LV_SYMBOL_REFRESH "  HUMIDITY");
    lv_obj_align(ico2, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_humidity = lv_label_create(card2);
    lv_obj_set_style_text_font(lbl_humidity, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_humidity, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(lbl_humidity, "--%");
    lv_obj_align(lbl_humidity, LV_ALIGN_LEFT_MID, 0, 10);

    // 湿度弧形（右侧）
    arc_humidity = lv_arc_create(card2);
    lv_obj_set_size(arc_humidity, 56, 56);
    lv_obj_align(arc_humidity, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_arc_set_rotation(arc_humidity, 135);
    lv_arc_set_bg_angles(arc_humidity, 0, 270);
    lv_arc_set_range(arc_humidity, 0, 100);
    lv_arc_set_value(arc_humidity, 0);
    lv_obj_set_style_opa(arc_humidity, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(arc_humidity, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(arc_humidity, LV_OPA_TRANSP, 0);
    lv_obj_set_style_arc_color(arc_humidity, lv_color_hex(COLOR_SURFACE2), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_humidity, lv_color_hex(COLOR_PRIMARY), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_humidity, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_humidity, 5, LV_PART_INDICATOR);

    // ── 卡片 3：风速 ──────────────────────────────────────────────────────────
    lv_obj_t *card3 = _make_card(scr, W, Y(330), SH(52));
    lv_obj_t *ico3 = lv_label_create(card3);
    lv_obj_set_style_text_font(ico3, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ico3, lv_color_hex(COLOR_TEAL), 0);
    lv_label_set_text(ico3, LV_SYMBOL_UP "  WIND");
    lv_obj_align(ico3, LV_ALIGN_LEFT_MID, 0, 0);

    lbl_wind = lv_label_create(card3);
    lv_obj_set_style_text_font(lbl_wind, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_wind, lv_color_hex(COLOR_TEXT), 0);
    lv_label_set_text(lbl_wind, "-- m/s");
    lv_obj_align(lbl_wind, LV_ALIGN_RIGHT_MID, 0, 0);

    // ── 更新时间（底部灰色小字）──────────────────────────────────────────────
    lbl_update = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl_update, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_update, lv_color_hex(COLOR_DIM), 0);
    lv_label_set_text(lbl_update, "Waiting...");
    lv_obj_align(lbl_update, LV_ALIGN_BOTTOM_MID, 0, -26);

    // 底部导航提示
    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_DIM), 0);
    lv_label_set_text(hint, LV_SYMBOL_LEFT " Clock    Sensor " LV_SYMBOL_RIGHT);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

static void update(const WeatherData &w, const struct tm &t) {
    if (!lbl_condition) return;  // 重建期间保护
    if (!w.valid) {
        lv_label_set_text(lbl_condition, LV_SYMBOL_WARNING "  No data");
        return;
    }

    char buf[64];

    // 城市名
    lv_label_set_text(lbl_city, w.city_name[0]
        ? (String(LV_SYMBOL_GPS "  ") + w.city_name).c_str()
        : LV_SYMBOL_GPS "  Weather");

    // 温度（仅整数，AW 风格）
    snprintf(buf, sizeof(buf), "%d \xc2\xb0" "C", (int)roundf(w.temp));
    lv_label_set_text(lbl_temp, buf);

    snprintf(buf, sizeof(buf), "H:%d  L:%d",
             (int)roundf(w.temp_max), (int)roundf(w.temp_min));
    lv_label_set_text(lbl_hi_lo, buf);

    // 状况文字 + 颜色
    snprintf(buf, sizeof(buf), "%s", conditionToText(w.condition_id));
    lv_label_set_text(lbl_condition, buf);
    lv_obj_set_style_text_color(lbl_condition, conditionColor(w.condition_id), 0);

    // 体感
    snprintf(buf, sizeof(buf), "%d \xc2\xb0" "C", (int)roundf(w.feels_like));
    lv_label_set_text(lbl_feels, buf);

    // 湿度
    snprintf(buf, sizeof(buf), "%d%%", w.humidity);
    lv_label_set_text(lbl_humidity, buf);
    lv_arc_set_value(arc_humidity, w.humidity);

    // 风速
    snprintf(buf, sizeof(buf), "%.1f m/s", w.wind_speed);
    lv_label_set_text(lbl_wind, buf);

    // 更新时间
    snprintf(buf, sizeof(buf), "Updated %02d:%02d", t.tm_hour, t.tm_min);
    lv_label_set_text(lbl_update, buf);
}

} // namespace ScreenWeather
