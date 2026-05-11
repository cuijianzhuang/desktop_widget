/**
 * ESP32-S3-Touch-AMOLED-1.8 桌面摆件  v1.1
 *
 * 功能：
 *   · 三屏 UI（时钟 / 天气 / 传感器），左右滑动切换
 *   · 后台 Web 管理页（http://设备IP），配置 WiFi / 天气 / 显示
 *   · OTA 固件空中升级
 *   · PCF85063 RTC 持久计时，WiFi 连接后 NTP 自动校准
 *
 * 依赖库（Arduino 库管理器安装）：
 *   GFX Library for Arduino · Arduino_DriveBus_Library（手动）
 *   SensorLib · XPowersLib · lvgl(v9.5) · ArduinoJson
 *
 * 开发板设置：ESP32S3 Dev Module
 *   Flash=16MB · Partition=16M(3MB APP) · PSRAM=OPI · USB=Hardware CDC
 */

// ── Includes ──────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <stdint.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <HWCDC.h>

#include "Arduino_GFX_Library.h"
#include "Arduino_DriveBus_Library.h"
#include <lvgl.h>
#include "draw/lv_draw_buf.h"
#include "draw/sw/lv_draw_sw_utils.h"
#include "SensorPCF85063.hpp"
#include "SensorQMI8658.hpp"
#include "XPowersLib.h"

#include "pin_config.h"
#include "config.h"
#include "preferences_manager.h"
#include "wifi_manager.h"
#include "web_config.h"
#include "weather_client.h"
#include "screen_clock.h"
#include "screen_weather.h"
#include "screen_sensor.h"
#include "display_power.h"
#include "orientation.h"

// ══════════════════════════════════════════════════════════════════════════════
// 全局状态（web_config.h 中 extern 引用）
// ══════════════════════════════════════════════════════════════════════════════
int  g_battery_pct    = -1;
bool g_wifi_connected = false;

// ══════════════════════════════════════════════════════════════════════════════
// 硬件对象
// ══════════════════════════════════════════════════════════════════════════════
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_SH8601  *gfx = new Arduino_SH8601(
    bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT);

std::shared_ptr<Arduino_IIC_DriveBus> IIC_Bus =
    std::make_shared<Arduino_HWIIC>(IIC_SDA, IIC_SCL, &Wire);

void onTouchIRQ();
std::unique_ptr<Arduino_IIC> FT3168(
    new Arduino_FT3x68(IIC_Bus, FT3168_DEVICE_ADDRESS,
                       DRIVEBUS_DEFAULT_VALUE, TP_INT, onTouchIRQ));

SensorPCF85063 rtc;
SensorQMI8658  qmi;
XPowersAXP2101 pmu;

// ══════════════════════════════════════════════════════════════════════════════
// LVGL 帧缓冲
// ══════════════════════════════════════════════════════════════════════════════
#define LVGL_TICK_MS 2
#define DRAW_LINES   40
EXT_RAM_BSS_ATTR static lv_color_t lv_fb[LCD_WIDTH * DRAW_LINES];
static lv_color_t   *s_lv_fb_full = nullptr;
static uint8_t      *s_flush_rot_buf = nullptr; // 非 0° 时 flush 旋转暂存（PSRAM）
static lv_display_t *s_lv_disp    = nullptr;   // 全局显示句柄（旋转 / 重建 UI 使用）

// ══════════════════════════════════════════════════════════════════════════════
// 屏幕切换
// ══════════════════════════════════════════════════════════════════════════════
static lv_obj_t *s_page[3]   = {nullptr, nullptr, nullptr};
static int        cur_scr     = 0;
static volatile int s_pending_tab = -1;
/** 各屏内容是否已 create（旋转/启动时只建当前屏，其余首次滑到再建，减轻卡顿） */
static bool       s_scr_built[3] = {false, false, false};

static bool mainPagesReady() { return s_page[0] != nullptr; }

static void ensureScreenBuilt(int idx);

static void applyVisiblePage(int idx) {
    if (!s_page[0]) return;
    idx = constrain(idx, 0, 2);
    ensureScreenBuilt(idx);
    for (int i = 0; i < 3; i++) {
        if (i == idx) lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_add_flag  (s_page[i], LV_OBJ_FLAG_HIDDEN);
    }
    cur_scr = idx;
    for (int i = 0; i < 3; i++) lv_obj_invalidate(s_page[i]);
    lv_obj_invalidate(lv_scr_act());
}

// ══════════════════════════════════════════════════════════════════════════════
// 天气状态（仅在 .ino 内部使用；跨 Task 访问由 s_wx_mutex 保护）
// ══════════════════════════════════════════════════════════════════════════════
static WeatherData        weather      = {};
static WeatherClient      wclient;
static unsigned long      wx_last      = 0;
static volatile bool      wx_fetch_busy = false;
static SemaphoreHandle_t  s_wx_mutex   = nullptr;

// ══════════════════════════════════════════════════════════════════════════════
// 触摸手势
// ══════════════════════════════════════════════════════════════════════════════
static int32_t swipe_x0   = -1;
static bool    touch_down = false;

// ══════════════════════════════════════════════════════════════════════════════
// LVGL 回调
// ══════════════════════════════════════════════════════════════════════════════
void onTouchIRQ() { FT3168->IIC_Interrupt_Flag = true; }

void lv_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const int32_t src_w = lv_area_get_width(area);
    const int32_t src_h = lv_area_get_height(area);
    const lv_display_rotation_t rot = lv_display_get_rotation(disp);
    const lv_color_format_t cf = lv_display_get_color_format(disp);

    if (rot == LV_DISPLAY_ROTATION_0 || s_flush_rot_buf == nullptr) {
        gfx->draw16bitBeRGBBitmap(area->x1, area->y1,
                                  (uint16_t *)px_map, src_w, src_h);
        lv_display_flush_ready(disp);
        return;
    }

    lv_area_t ra = *area;
    lv_display_rotate_area(disp, &ra);
    const int32_t dst_w = lv_area_get_width(&ra);
    const int32_t dst_h = lv_area_get_height(&ra);
    const uint32_t src_stride = lv_draw_buf_width_to_stride(src_w, cf);
    const uint32_t dst_stride = lv_draw_buf_width_to_stride(dst_w, cf);
    /* RGB565_SWAPPED 的像素仍以 16bit 为单元参与旋转，与 RGB565 路径一致 */
    const lv_color_format_t rot_cf =
        (cf == LV_COLOR_FORMAT_RGB565_SWAPPED) ? LV_COLOR_FORMAT_RGB565 : cf;

    lv_draw_sw_rotate(px_map, s_flush_rot_buf, src_w, src_h, src_stride, dst_stride, rot, rot_cf);
    gfx->draw16bitBeRGBBitmap(ra.x1, ra.y1, (uint16_t *)s_flush_rot_buf, dst_w, dst_h);
    lv_display_flush_ready(disp);
}

void lv_touch(lv_indev_t *indev, lv_indev_data_t *data) {
    if (!mainPagesReady()) { data->state = LV_INDEV_STATE_REL; return; }

    if (!FT3168->IIC_Interrupt_Flag) {
        if (touch_down) {
            DisplayPower::notifyInteraction();
            touch_down = false;
            int32_t cx = FT3168->IIC_Read_Device_Value(
                FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
            int32_t cy = FT3168->IIC_Read_Device_Value(
                FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
            int32_t dx = cx - swipe_x0;
            if (swipe_x0 >= 0 && abs(dx) > 48)
                s_pending_tab = (dx < 0) ? (cur_scr + 1) % 3 : (cur_scr + 2) % 3;
            swipe_x0   = -1;
            data->point = {cx, cy};
        }
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    FT3168->IIC_Interrupt_Flag = false;
    int32_t tx = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_X);
    int32_t ty = FT3168->IIC_Read_Device_Value(
        FT3168->Arduino_IIC_Touch::Value_Information::TOUCH_COORDINATE_Y);
    if (!touch_down) { touch_down = true; swipe_x0 = tx; DisplayPower::notifyInteraction(); }
    data->state   = LV_INDEV_STATE_PR;
    data->point   = {tx, ty};
}

void lv_tick_cb(void *) { lv_tick_inc(LVGL_TICK_MS); }

// ══════════════════════════════════════════════════════════════════════════════
// 公共接口（供 web_config.h extern 调用）
// ══════════════════════════════════════════════════════════════════════════════

/** display_power.h extern 实现：直接驱动 GFX 背光 */
void displayHardwareSetBrightness(int level) {
    gfx->setBrightness((uint8_t)constrain(level, 0, 255));
}

void applyBrightness(int v) {
    DisplayPower::notifyInteraction();
    gfx->setBrightness(constrain(v, 20, 255));
}

void applyConfigFromWeb(uint32_t mask) {
    AppConfig &c = PreferencesManager::cfg;
    if (mask & CFG_WEB_NTP) {
        xTaskCreatePinnedToCore([](void *){ syncNTP(); vTaskDelete(NULL); },
                                "ntp_web", 6144, NULL, 1, NULL, 0);
    }
    if (mask & CFG_WEB_WEATHER) {
        if (WiFiManager::isConnected() && !WiFiManager::isAPMode() && !wx_fetch_busy) {
            wx_fetch_busy = true;
            if (xTaskCreate(fetchWeatherTask, "wx_web", 8192, NULL, 1, NULL) != pdPASS) {
                wx_fetch_busy = false; wx_last = millis();
            }
        }
    }
    if (mask & CFG_WEB_SCREEN)
        if (!WiFiManager::isAPMode() && mainPagesReady())
            applyVisiblePage((int)c.default_screen);
    if (mask & CFG_WEB_POWER)
        DisplayPower::resetIdleTimer();
}

// ══════════════════════════════════════════════════════════════════════════════
// UI 通知辅助
// ══════════════════════════════════════════════════════════════════════════════
static void showAPNotice() {
    lv_obj_t *s = lv_scr_act();
    lv_obj_set_style_bg_color(s, lv_color_hex(0x000000), 0);
    lv_obj_t *t = lv_label_create(s);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0x00D4FF), 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(t,
        LV_SYMBOL_WIFI "  SETUP\n\n"
        "Join WiFi:\n"
        "Widget-Setup\n"
        "Pass: 12345678\n\n"
        "Open in browser:\n"
        "192.168.4.1");
    lv_obj_align(t, LV_ALIGN_CENTER, 0, 0);
}

static void showConnectedNotice(const String &ip) {
    lv_obj_t *popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(popup, LCD_WIDTH, 40);
    lv_obj_align(popup, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x003344), 0);
    lv_obj_set_style_border_width(popup, 0, 0);
    lv_obj_set_style_radius(popup, 0, 0);
    lv_obj_t *lbl = lv_label_create(popup);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00D4FF), 0);
    char buf[64];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  http://%s", ip.c_str());
    lv_label_set_text(lbl, buf);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_anim_t anim; lv_anim_init(&anim);
    lv_anim_set_var(&anim, popup);
    lv_anim_set_delay(&anim, 3000);
    lv_anim_set_time(&anim, 400);
    lv_anim_set_exec_cb(&anim, [](void *obj, int32_t v){
        lv_obj_set_style_opa((lv_obj_t*)obj, v, 0);
    });
    lv_anim_set_values(&anim, 255, 0);
    lv_anim_set_deleted_cb(&anim, [](lv_anim_t *a){
        lv_obj_delete((lv_obj_t*)a->var);
    });
    lv_anim_start(&anim);
}

// ══════════════════════════════════════════════════════════════════════════════
// 后台任务
// ══════════════════════════════════════════════════════════════════════════════
static void syncNTP() {
    AppConfig &c = PreferencesManager::cfg;
    configTime((long)c.tz_offset * 3600, 0, c.ntp1, "pool.ntp.org");
    struct tm t; int tries = 0;
    while (!getLocalTime(&t, 1000) && tries++ < 10);
    if (tries < 10) {
        rtc.setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                        t.tm_hour, t.tm_min, t.tm_sec);
        Serial.println("[NTP] 同步成功，已写入 RTC");
    } else {
        Serial.println("[NTP] 同步超时，使用 RTC 时间");
    }
}

static void fetchWeatherTask(void *) {
    WeatherData wd = {};
    if (wclient.fetch(wd) && wd.valid) {
        if (s_wx_mutex && xSemaphoreTake(s_wx_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            weather = wd;
            xSemaphoreGive(s_wx_mutex);
        }
    }
    wx_last = millis();
    wx_fetch_busy = false;
    vTaskDelete(NULL);
}

// ══════════════════════════════════════════════════════════════════════════════
// 初始化子函数
// ══════════════════════════════════════════════════════════════════════════════

/** I2C / PMU / RTC / IMU / 触摸 */
static void initHardware() {
    Wire.begin(IIC_SDA, IIC_SCL);

    if (pmu.begin(Wire, 0x34, IIC_SDA, IIC_SCL))
        Serial.println("[PMU] AXP2101 OK");

    if (!rtc.begin(Wire, IIC_SDA, IIC_SCL))
        Serial.println("[RTC] 初始化失败");

    if (qmi.begin(Wire, 0x6B, IIC_SDA, IIC_SCL)) {
        qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                                SensorQMI8658::ACC_ODR_1000Hz,
                                SensorQMI8658::LPF_MODE_0);
        qmi.enableAccelerometer();
        Serial.println("[IMU] QMI8658 OK");
    }

    while (!FT3168->begin()) { Serial.println("[TP] FT3168 重试..."); delay(1000); }
    FT3168->IIC_Write_Device_State(
        FT3168->Arduino_IIC_Touch::Device::TOUCH_POWER_MODE,
        FT3168->Arduino_IIC_Touch::Device_Mode::TOUCH_POWER_MONITOR);
    Serial.println("[TP] FT3168 OK");
}

/** GFX + LVGL 显示驱动、帧缓冲、输入设备、Tick 定时器 */
static void initLVGL() {
    gfx->begin();
    lv_init();
    s_lv_disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_t *lv_disp = s_lv_disp;
    lv_display_set_flush_cb(lv_disp, lv_flush);

    const size_t full_bytes = (size_t)LCD_WIDTH * (size_t)LCD_HEIGHT * sizeof(lv_color_t);
    s_lv_fb_full = (lv_color_t *)ps_malloc(full_bytes);
    if (s_lv_fb_full) {
        lv_display_set_buffers(lv_disp, s_lv_fb_full, nullptr,
                               full_bytes, LV_DISPLAY_RENDER_MODE_FULL);
        Serial.printf("[LVGL] 全帧缓冲 %u bytes（PSRAM）\n", (unsigned)full_bytes);
    } else {
        lv_display_set_buffers(lv_disp, lv_fb, nullptr,
                               sizeof(lv_fb), LV_DISPLAY_RENDER_MODE_PARTIAL);
        Serial.println("[LVGL] 全帧缓冲分配失败，回退部分刷新");
    }
    lv_display_set_color_format(lv_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);

    s_flush_rot_buf = (uint8_t *)ps_malloc(full_bytes);
    if (s_flush_rot_buf)
        Serial.printf("[LVGL] 旋转 flush 缓冲 %u bytes\n", (unsigned)full_bytes);
    else
        Serial.println("[LVGL] 旋转 flush 缓冲分配失败（非竖屏可能异常）");

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lv_touch);
    lv_indev_set_display(indev, lv_disp);

    const esp_timer_create_args_t tick_args = { .callback = lv_tick_cb, .name = "lv" };
    esp_timer_handle_t th;
    esp_timer_create(&tick_args, &th);
    esp_timer_start_periodic(th, LVGL_TICK_MS * 1000);
}

// ── 三屏容器创建辅助（initUI / rebuildUI 共用）─────────────────────────────
/** 仅三个全屏占位容器，内容由 ensureScreenBuilt 按需挂载 */
static void _buildPageShells() {
    const int W = (int)lv_display_get_horizontal_resolution(s_lv_disp);
    const int H = (int)lv_display_get_vertical_resolution(s_lv_disp);

    lv_obj_t *root = lv_scr_act();
    for (int i = 0; i < 3; i++) {
        s_page[i] = lv_obj_create(root);
        lv_obj_set_size(s_page[i], W, H);
        lv_obj_set_pos(s_page[i], 0, 0);
        lv_obj_set_style_bg_color(s_page[i],     lv_color_hex(COLOR_BG), 0);
        lv_obj_set_style_bg_opa(s_page[i],       LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(s_page[i],      0, 0);
        lv_obj_set_style_border_width(s_page[i], 0, 0);
        lv_obj_set_style_outline_width(s_page[i],0, 0);
        lv_obj_set_style_radius(s_page[i],       0, 0);
        lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_SCROLLABLE);
    }
}

static void ensureScreenBuilt(int idx) {
    if (!mainPagesReady()) return;
    idx = constrain(idx, 0, 2);
    if (s_scr_built[idx]) return;
    const int W = (int)lv_display_get_horizontal_resolution(s_lv_disp);
    const int H = (int)lv_display_get_vertical_resolution(s_lv_disp);
    switch (idx) {
        case 0: ScreenClock::create(s_page[0], W, H); break;
        case 1: ScreenWeather::create(s_page[1], W, H); break;
        default: ScreenSensor::create(s_page[2], W, H); break;
    }
    s_scr_built[idx] = true;
}

/** 旋转后重建三屏 UI（由 onTick50ms 触发） */
static void rebuildUI() {
    if (WiFiManager::isAPMode() || !mainPagesReady()) return;

    /* 顶层纯色遮罩：避免删树/换向瞬间的碎屏与残影（尺寸取面板长边，任意朝向铺满） */
    const int cov = (LCD_WIDTH > LCD_HEIGHT) ? LCD_WIDTH : LCD_HEIGHT;
    lv_obj_t *shade = lv_obj_create(lv_layer_top());
    lv_obj_set_size(shade, cov, cov);
    lv_obj_align(shade, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(shade, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(shade, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(shade, 0, 0);
    lv_obj_set_style_pad_all(shade, 0, 0);
    lv_obj_set_style_radius(shade, 0, 0);
    lv_obj_clear_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    lv_refr_now(s_lv_disp);

    // ① 先销毁全部控件——LVGL 树清空后才能安全旋转
    //    若先旋转，lv_display_set_rotation 内部触发 LV_EVENT_RESOLUTION_CHANGED
    //    → LVGL 标记全屏重绘，随后删掉控件会访问悬挂指针导致崩溃
    ScreenClock::destroy();
    ScreenWeather::destroy();
    ScreenSensor::destroy();
    for (int i = 0; i < 3; i++) {
        lv_obj_delete(s_page[i]);
        s_page[i] = nullptr;
    }

    // ② 树已清空，再设置旋转；LVGL 的内部刷新此时无任何控件可访问
    lv_display_set_rotation(s_lv_disp,
                            (lv_display_rotation_t)Orientation::current());
    /* 勿再调用 gfx->setRotation：与 LVGL v9 + flush 软件旋转双重变换会卡住/花屏 */

    // ③ 占位容器 + 仅当前可见屏内容（其余首次切入再建）
    s_scr_built[0] = s_scr_built[1] = s_scr_built[2] = false;
    _buildPageShells();
    applyVisiblePage(cur_scr);

    lv_obj_delete(shade);
    lv_refr_now(s_lv_disp);
    yield();
}

/** 创建三个全屏容器并渲染各屏 UI */
static void initUI() {
    AppConfig &cfg = PreferencesManager::cfg;
    DisplayPower::init();
    applyBrightness(cfg.brightness);
    cur_scr = cfg.default_screen;

    if (WiFiManager::isAPMode()) { showAPNotice(); return; }

    lv_obj_t *root = lv_scr_act();
    lv_obj_set_style_bg_color(root, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 0, 0);

    s_scr_built[0] = s_scr_built[1] = s_scr_built[2] = false;
    _buildPageShells();
    applyVisiblePage(cur_scr);
    showConnectedNotice(WiFiManager::localIP());

    // NTP 同步 + 首次天气拉取（后台任务，避免阻塞 UI 渲染）
    xTaskCreatePinnedToCore([](void *) {
        syncNTP();
        wx_fetch_busy = true;
        if (xTaskCreate(fetchWeatherTask, "wx0", 8192, NULL, 1, NULL) != pdPASS) {
            wx_fetch_busy = false; wx_last = millis();
        }
        vTaskDelete(NULL);
    }, "init_net", 6144, NULL, 1, NULL, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Loop 子函数（各自维护自己的 last-fire 时间戳）
// ══════════════════════════════════════════════════════════════════════════════
static unsigned long t_sec  = 0;
static unsigned long t_imu  = 0;
static unsigned long t_wifi = 0;

/** 处理手势触发的切屏请求（在 LVGL timer_handler 之后执行，避免回调内改树）*/
static void handlePendingTab() {
    if (s_pending_tab >= 0 && mainPagesReady()) {
        int idx = s_pending_tab;
        s_pending_tab = -1;
        applyVisiblePage(idx);
    }
}

/** 1 Hz：读 RTC → 刷时钟/天气 → 触发天气拉取 → 更新电量 */
static void onTick1000ms(unsigned long now) {
    t_sec = now;
    g_wifi_connected = WiFiManager::isConnected();

    RTC_DateTime dt = rtc.getDateTime();
    struct tm t = {};
    t.tm_year = dt.getYear() - 1900; t.tm_mon  = dt.getMonth() - 1;
    t.tm_mday = dt.getDay();         t.tm_hour  = dt.getHour();
    t.tm_min  = dt.getMinute();      t.tm_sec   = dt.getSecond();
    t.tm_wday = dt.getWeek();
    mktime(&t);

    if (!WiFiManager::isAPMode()) {
        DisplayPower::onEverySecond(&t);
        if (cur_scr == 0) ScreenClock::update(t);
        if (cur_scr == 1 && (weather.valid || wx_last > 0)) {
            WeatherData wx_snap;
            if (s_wx_mutex && xSemaphoreTake(s_wx_mutex, 0) == pdTRUE) {
                wx_snap = weather;
                xSemaphoreGive(s_wx_mutex);
                ScreenWeather::update(wx_snap, t);
            }
        }
        // 天气定时刷新
        unsigned long intv = max(1UL, (unsigned long)PreferencesManager::cfg.wx_interval) * 60000UL;
        if (g_wifi_connected && !wx_fetch_busy && (wx_last == 0 || now - wx_last >= intv)) {
            wx_fetch_busy = true;
            if (xTaskCreate(fetchWeatherTask, "wx", 8192, NULL, 1, NULL) != pdPASS) {
                wx_fetch_busy = false; wx_last = millis();
            }
        }
    }
    if (pmu.isBatteryConnect())
        g_battery_pct = (int)pmu.getBatteryPercent();
}

/** 50 ms：IMU 采样 → 旋转检测 → 息屏判断 → 秒针平滑 → 传感器页刷新 */
static void onTick50ms(unsigned long now) {
    t_imu = now;
    IMUdata acc = {}, gyr = {};
    bool imu_ok = qmi.getDataReady();
    if (imu_ok) {
        qmi.getAccelerometer(acc.x, acc.y, acc.z);
        qmi.getGyroscope(gyr.x, gyr.y, gyr.z);
    }
    DisplayPower::onImuSample(acc.x, acc.y, acc.z, imu_ok);

    // 旋转检测：稳定 750ms 后切换显示方向并重建 UI
    if (!WiFiManager::isAPMode() && imu_ok) {
        if (Orientation::updateMapped(acc.x, acc.y)) {
            rebuildUI();
            return;   // UI 刚重建，跳过本轮刷新避免写已删除控件
        }
    }

    if (cur_scr == 0) ScreenClock::updateSec();
    if (cur_scr == 2 && imu_ok) {
        int mv = pmu.isBatteryConnect() ? (int)pmu.getBattVoltage() : 0;
        ScreenSensor::update(acc.x, acc.y, acc.z, gyr.x, gyr.y, gyr.z, g_battery_pct, mv);
    }
}

/** 30 s：WiFi 掉线自动重连 */
static void onTick30s(unsigned long now) {
    t_wifi = now;
    if (!WiFiManager::isAPMode() && !WiFiManager::isConnected())
        WiFi.reconnect();
}

// ══════════════════════════════════════════════════════════════════════════════
// Arduino 入口
// ══════════════════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    delay(400);
    Serial.println("\n=== 桌面摆件 v1.1 启动 ===");

    s_wx_mutex = xSemaphoreCreateMutex();   // 在任何 Task 启动前创建

    initHardware();
    initLVGL();

    g_wifi_connected = WiFiManager::begin();
    PreferencesManager::load();             // WiFiManager::begin() 内已调用，此处保险重读

    Orientation::init();                   // IMU 旋转检测初始化（竖屏 0°）
    initUI();
    WebConfig::begin();
    Serial.println("=== 初始化完成 ===");
}

void loop() {
    lv_timer_handler();
    handlePendingTab();
    WebConfig::loop();
    if (WiFiManager::isAPMode()) WiFiManager::dnsLoop();
    delay(4);

    const unsigned long now = millis();
    if (now - t_sec  >= 1000)                          onTick1000ms(now);
    if (!WiFiManager::isAPMode() && now - t_imu >= 50) onTick50ms(now);
    if (now - t_wifi >= 30000)                         onTick30s(now);
}
