#pragma once
#include <stdint.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "preferences_manager.h"
#include "wifi_manager.h"
#include "web_ui.h"

// 固件版本号，改这里后重新烧录即可在状态页看到
#define FW_VERSION "1.0.0"

// 外部声明：设备状态（在 .ino 中定义）
extern int  g_battery_pct;
extern bool g_wifi_connected;

// 亮度接口：在全局作用域声明，否则在 namespace 内的 extern 会被当作
// WebConfig::applyBrightness 而非 ::applyBrightness，导致链接错误
extern void applyBrightness(int v);

/* Web 保存配置后同步到硬件（在 desktop_widget.ino 中实现） */
#define CFG_WEB_NTP      (1u << 0)
#define CFG_WEB_WEATHER  (1u << 1)
#define CFG_WEB_SCREEN   (1u << 2)
#define CFG_WEB_POWER    (1u << 3)
extern void applyConfigFromWeb(uint32_t mask);

namespace WebConfig {

static WebServer server(80);

// AP 配网保存 WiFi 并成功连上 STA 后重启，让 setup() 重新创建三屏 UI
static void wifiReconnectTask(void *arg) {
    const bool came_from_ap = ((uintptr_t)arg) != 0;
    delay(800);
    if (WiFiManager::reconnectSTA()) {
        if (came_from_ap) {
            delay(500);
            ESP.restart();
        }
    }
    vTaskDelete(NULL);
}

// ── 工具：发 JSON 响应 ───────────────────────────────────────────────────────
static void sendJSON(int code, const String &json) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(code, "application/json", json);
}

// ── GET /  →  后台页面 ───────────────────────────────────────────────────────
static void handleRoot() {
    server.sendHeader("Content-Encoding", "identity");
    // 仅用 text/html，charset 由页面内 <meta charset="UTF-8"> 声明，避免少数栈对长 Content-Type + send_P 异常
    server.send_P(200, "text/html", WEB_UI);
}

static void handlePing() {
    server.send(200, "text/plain", "ok");
}

// ── GET /api/config  →  当前配置 JSON ───────────────────────────────────────
static void handleGetConfig() {
    AppConfig &c = PreferencesManager::cfg;
    StaticJsonDocument<768> doc;
    doc["wifi_ssid"]      = c.wifi_ssid;
    doc["wifi_pass"]      = c.wifi_pass;
    doc["ntp1"]           = c.ntp1;
    doc["tz_offset"]      = c.tz_offset;
    doc["owm_key"]        = c.owm_key;
    doc["city_id"]        = c.city_id;
    doc["wx_interval"]    = c.wx_interval;
    doc["wx_units"]       = c.wx_units;
    doc["brightness"]     = c.brightness;
    doc["default_screen"] = c.default_screen;
    doc["auto_sleep_sec"] = c.auto_sleep_sec;
    doc["wake_shake"]     = c.wake_shake;
    doc["hourly_notify"]  = c.hourly_notify;

    String out;
    serializeJson(doc, out);
    sendJSON(200, out);
}

// ── POST /api/config  →  合并更新配置 ───────────────────────────────────────
static void handlePostConfig() {
    if (!server.hasArg("plain")) { sendJSON(400, "{\"error\":\"no body\"}"); return; }

    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        sendJSON(400, "{\"error\":\"invalid JSON\"}");
        return;
    }

    AppConfig &c = PreferencesManager::cfg;
    bool needWiFiReconnect = false;

    if (doc.containsKey("wifi_ssid")) {
        strlcpy(c.wifi_ssid, doc["wifi_ssid"] | "", sizeof(c.wifi_ssid));
        needWiFiReconnect = true;
    }
    if (doc.containsKey("wifi_pass")) {
        strlcpy(c.wifi_pass, doc["wifi_pass"] | "", sizeof(c.wifi_pass));
        needWiFiReconnect = true;
    }
    if (doc.containsKey("ntp1"))        strlcpy(c.ntp1,     doc["ntp1"]     | "ntp.aliyun.com", sizeof(c.ntp1));
    if (doc.containsKey("tz_offset"))   c.tz_offset    = doc["tz_offset"]   | 8;
    if (doc.containsKey("owm_key"))     strlcpy(c.owm_key,  doc["owm_key"]  | "", sizeof(c.owm_key));
    if (doc.containsKey("city_id"))     strlcpy(c.city_id,  doc["city_id"]  | "1796236", sizeof(c.city_id));
    if (doc.containsKey("wx_interval")) c.wx_interval  = doc["wx_interval"] | 30;
    if (doc.containsKey("wx_units"))    strlcpy(c.wx_units, doc["wx_units"] | "metric", sizeof(c.wx_units));
    if (doc.containsKey("brightness"))  c.brightness   = doc["brightness"]  | 200;
    if (doc.containsKey("default_screen")) c.default_screen = doc["default_screen"] | 0;
    if (doc.containsKey("auto_sleep_sec"))
        c.auto_sleep_sec = constrain((int)doc["auto_sleep_sec"].as<int>(), 0, 3600);
    if (doc.containsKey("wake_shake"))
        c.wake_shake = doc["wake_shake"].as<bool>() ? 1 : 0;
    if (doc.containsKey("hourly_notify"))
        c.hourly_notify = doc["hourly_notify"].as<bool>() ? 1 : 0;

    PreferencesManager::save();
    sendJSON(200, "{\"ok\":true}");

    // WiFi 凭据变更 → 延迟重连（先把响应发出去）
    if (needWiFiReconnect) {
        const bool was_ap = WiFiManager::isAPMode();
        xTaskCreate(wifiReconnectTask, "wifi_rc", 4096,
                    (void *)(uintptr_t)(was_ap ? 1 : 0), 1, NULL);
    }

    // 亮度立即生效（全局函数 applyBrightness，在 .ino 中实现）
    if (doc.containsKey("brightness")) applyBrightness(c.brightness);

    // 其余项：下发到硬件 / 触发后台同步（NTP、天气、默认屏）
    uint32_t web = 0;
    if (doc.containsKey("ntp1") || doc.containsKey("tz_offset"))
        web |= CFG_WEB_NTP;
    if (doc.containsKey("owm_key") || doc.containsKey("city_id") ||
        doc.containsKey("wx_interval") || doc.containsKey("wx_units"))
        web |= CFG_WEB_WEATHER;
    if (doc.containsKey("default_screen"))
        web |= CFG_WEB_SCREEN;
    if (doc.containsKey("auto_sleep_sec") || doc.containsKey("wake_shake") ||
        doc.containsKey("hourly_notify"))
        web |= CFG_WEB_POWER;
    if (web) applyConfigFromWeb(web);
}

// ── GET /api/status  →  设备实时状态 ────────────────────────────────────────
static void handleStatus() {
    StaticJsonDocument<256> doc;
    doc["ip"]            = WiFiManager::isAPMode()
                             ? "192.168.4.1"
                             : WiFiManager::localIP();
    doc["rssi"]          = WiFiManager::isAPMode() ? 0 : WiFiManager::rssi();
    doc["wifi_connected"]= WiFiManager::isConnected();
    doc["uptime"]        = (int)(millis() / 1000);
    doc["free_heap"]     = (int)ESP.getFreeHeap();
    doc["battery"]       = g_battery_pct;
    doc["version"]       = FW_VERSION;
    doc["ap_mode"]       = WiFiManager::isAPMode();

    String out;
    serializeJson(doc, out);
    sendJSON(200, out);
}

// ── POST /api/restart  →  重启 ───────────────────────────────────────────────
static void handleRestart() {
    sendJSON(200, "{\"ok\":true}");
    delay(200);
    ESP.restart();
}

// ── POST /api/reset  →  恢复出厂设置 ────────────────────────────────────────
static void handleReset() {
    sendJSON(200, "{\"ok\":true}");
    delay(200);
    PreferencesManager::erase();
    ESP.restart();
}

// ── POST /update  →  OTA 固件上传 ───────────────────────────────────────────
static void handleOTAResponse() {
    server.sendHeader("Connection", "close");
    bool ok = !Update.hasError();
    server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.errorString());
    if (ok) {
        delay(500);
        ESP.restart();
    }
}

static void handleOTAUpload() {
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] 开始: %s  大小: %u bytes\n",
                      upload.filename.c_str(), upload.totalSize);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Serial.println("[OTA] begin() 失败");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
        Serial.printf("[OTA] 进度: %u KB\r", upload.totalSize / 1024);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("\n[OTA] 完成: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

// ── 初始化服务器 ─────────────────────────────────────────────────────────────
static void begin() {
    server.on("/",           HTTP_GET,  handleRoot);
    server.on("/ping",      HTTP_GET,  handlePing);
    server.on("/api/config", HTTP_GET,  handleGetConfig);
    server.on("/api/config", HTTP_POST, handlePostConfig);
    server.on("/api/status", HTTP_GET,  handleStatus);
    server.on("/api/restart",HTTP_POST, handleRestart);
    server.on("/api/reset",  HTTP_POST, handleReset);

    server.on("/update", HTTP_POST, handleOTAResponse, handleOTAUpload);

    // OPTIONS 预检（浏览器跨域）
    server.onNotFound([](){
        if (server.method() == HTTP_OPTIONS) {
            server.sendHeader("Access-Control-Allow-Origin",  "*");
            server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
            server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
            server.send(204);
        } else {
            server.send(404, "text/plain", "Not found");
        }
    });

    server.begin();
    Serial.println("[Web] HTTP 服务器已启动 (port 80)");
}

static void loop() {
    server.handleClient();
}

} // namespace WebConfig
