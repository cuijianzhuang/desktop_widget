#pragma once
/**
 * 自动息屏、触摸/摇一摇唤醒、整点屏幕提示。
 * 背光：displayHardwareSetBrightness(0–255)。语音唤醒/播报需 ES8311 + ESP-SR / TTS 另接库。
 */
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <time.h>
#include "preferences_manager.h"
#include "wifi_manager.h"

extern void displayHardwareSetBrightness(int level);

namespace DisplayPower {

static unsigned long s_last_act_ms      = 0;
static bool          s_asleep         = false;
static unsigned long s_last_shake_ms = 0;
static float         s_last_mag       = 1.0f;
static lv_obj_t     *s_hour_toast     = nullptr;
static int           s_ann_key        = -1;  // tm_yday*24+tm_hour，避免次日同整点不播
static unsigned long s_toast_hide_at  = 0;

inline void init() {
    s_last_act_ms = millis();
    s_asleep = false;
}

inline bool isAsleep() { return s_asleep; }

/** 仅刷新空闲计时（例如 Web 改了息屏时间，不强制亮屏） */
inline void resetIdleTimer() { s_last_act_ms = millis(); }

/** 触摸等交互：刷新计时，若息屏则唤醒 */
inline void notifyInteraction() {
    s_last_act_ms = millis();
    if (s_asleep) {
        s_asleep = false;
        displayHardwareSetBrightness(PreferencesManager::cfg.brightness);
    }
}

inline void enterSleep() {
    if (s_asleep || WiFiManager::isAPMode()) return;
    s_asleep = true;
    if (s_hour_toast) {
        lv_obj_add_flag(s_hour_toast, LV_OBJ_FLAG_HIDDEN);
        s_toast_hide_at = 0;
    }
    displayHardwareSetBrightness(0);
}

/** 每秒：超时息屏 + 整点横幅 + 横幅自动隐藏 */
inline void onEverySecond(const struct tm *t) {
    if (WiFiManager::isAPMode() || !t) return;

    const int lim = PreferencesManager::cfg.auto_sleep_sec;
    if (!s_asleep && lim > 0 &&
        (millis() - s_last_act_ms >= (unsigned long)lim * 1000UL))
        enterSleep();

    if (s_hour_toast && s_toast_hide_at != 0 && millis() >= s_toast_hide_at) {
        lv_obj_add_flag(s_hour_toast, LV_OBJ_FLAG_HIDDEN);
        s_toast_hide_at = 0;
    }

    if (s_asleep || !PreferencesManager::cfg.hourly_notify) return;

    if (t->tm_min == 0 && t->tm_sec == 0) {
        const int key = t->tm_yday * 24 + t->tm_hour;
        if (key != s_ann_key) {
            s_ann_key = key;
            lv_obj_t *par = lv_layer_top();
            if (!par) par = lv_scr_act();
            if (!s_hour_toast) {
                s_hour_toast = lv_label_create(par);
                lv_obj_set_style_bg_color(s_hour_toast, lv_color_hex(0x0a1620), 0);
                lv_obj_set_style_bg_opa(s_hour_toast, LV_OPA_COVER, 0);
                lv_obj_set_style_text_color(s_hour_toast, lv_color_hex(0x00D4FF), 0);
                lv_obj_set_style_pad_hor(s_hour_toast, 16, 0);
                lv_obj_set_style_pad_ver(s_hour_toast, 10, 0);
                lv_obj_set_style_radius(s_hour_toast, 10, 0);
                lv_obj_set_width(s_hour_toast, 300);
                lv_obj_align(s_hour_toast, LV_ALIGN_TOP_MID, 0, 24);
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "整点 %02d:00", t->tm_hour);
            lv_label_set_text(s_hour_toast, buf);
            lv_obj_clear_flag(s_hour_toast, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_hour_toast);
            s_toast_hide_at = millis() + 4000;
        }
    }
}

/** 约 20Hz IMU：息屏时摇一摇唤醒 */
inline void onImuSample(float ax, float ay, float az, bool imu_ok) {
    if (!s_asleep || !imu_ok) return;
    if (!PreferencesManager::cfg.wake_shake) return;

    const float mag = sqrtf(ax * ax + ay * ay + az * az);
    const float d   = fabsf(mag - s_last_mag);
    s_last_mag = mag;
    const unsigned long now = millis();
    if (d > 0.35f && (now - s_last_shake_ms) > 400) {
        s_last_shake_ms = now;
        notifyInteraction();
    }
}

} // namespace DisplayPower
