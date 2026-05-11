#pragma once
#include <Preferences.h>
#include <Arduino.h>

/**
 * 所有运行时可配置参数集中管理
 * 底层使用 ESP32 NVS（Non-Volatile Storage）持久化
 */
struct AppConfig {
    // WiFi
    char wifi_ssid[64];
    char wifi_pass[64];
    // NTP
    char ntp1[64];
    int  tz_offset;         // UTC偏移小时数，如 8 = UTC+8
    // 天气
    char owm_key[48];
    char city_id[16];
    int  wx_interval;       // 更新间隔（分钟）
    char wx_units[12];      // "metric" / "imperial"
    // 显示
    int  brightness;        // 0-255
    int  default_screen;    // 0=时钟 1=天气 2=传感器
    int  auto_sleep_sec;    // 无操作自动息屏秒数，0=关闭
    int  wake_shake;        // 息屏后摇一摇唤醒 1=开 0=关
    int  hourly_notify;     // 整点屏幕提示 1=开 0=关（语音播报需 ES8311 扩展）
};

class PreferencesManager {
public:
    static AppConfig cfg;

    static void load() {
        Preferences prefs;
        prefs.begin("widget", true);  // read-only

        strlcpy(cfg.wifi_ssid,     prefs.getString("wifi_ssid", "").c_str(),       sizeof(cfg.wifi_ssid));
        strlcpy(cfg.wifi_pass,     prefs.getString("wifi_pass", "").c_str(),       sizeof(cfg.wifi_pass));
        strlcpy(cfg.ntp1,          prefs.getString("ntp1", "ntp.aliyun.com").c_str(), sizeof(cfg.ntp1));
        cfg.tz_offset    = prefs.getInt("tz_offset", 8);
        strlcpy(cfg.owm_key,       prefs.getString("owm_key", "").c_str(),         sizeof(cfg.owm_key));
        strlcpy(cfg.city_id,       prefs.getString("city_id", "1796236").c_str(),  sizeof(cfg.city_id));
        cfg.wx_interval  = prefs.getInt("wx_interval", 30);
        strlcpy(cfg.wx_units,      prefs.getString("wx_units", "metric").c_str(),  sizeof(cfg.wx_units));
        cfg.brightness   = prefs.getInt("brightness", 200);
        cfg.default_screen = prefs.getInt("def_screen", 0);
        cfg.auto_sleep_sec = prefs.getInt("auto_sleep", 0);
        cfg.wake_shake     = prefs.getInt("wake_shake", 1);
        cfg.hourly_notify  = prefs.getInt("hourly", 0);

        prefs.end();
    }

    static void save() {
        Preferences prefs;
        prefs.begin("widget", false);  // read-write

        prefs.putString("wifi_ssid",   cfg.wifi_ssid);
        prefs.putString("wifi_pass",   cfg.wifi_pass);
        prefs.putString("ntp1",        cfg.ntp1);
        prefs.putInt(   "tz_offset",   cfg.tz_offset);
        prefs.putString("owm_key",     cfg.owm_key);
        prefs.putString("city_id",     cfg.city_id);
        prefs.putInt(   "wx_interval", cfg.wx_interval);
        prefs.putString("wx_units",    cfg.wx_units);
        prefs.putInt(   "brightness",  cfg.brightness);
        prefs.putInt(   "def_screen",  cfg.default_screen);
        prefs.putInt(   "auto_sleep",  cfg.auto_sleep_sec);
        prefs.putInt(   "wake_shake",  cfg.wake_shake);
        prefs.putInt(   "hourly",      cfg.hourly_notify);

        prefs.end();
    }

    // 擦除所有配置（恢复出厂设置）
    static void erase() {
        Preferences prefs;
        prefs.begin("widget", false);
        prefs.clear();
        prefs.end();
    }

    static bool hasWiFiConfig() {
        return strlen(cfg.wifi_ssid) > 0;
    }
};

// C++17 inline 静态成员：允许多个翻译单元包含此头文件而不产生 ODR 冲突
inline AppConfig PreferencesManager::cfg = {};
