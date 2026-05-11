#pragma once
#include <WiFi.h>
#include <DNSServer.h>
#include <Arduino.h>
#include "preferences_manager.h"

/**
 * WiFi 管理器
 *
 * 启动流程：
 *   1. 有保存的凭据 → 尝试 STA 连接（15秒超时）
 *   2. 连接失败或无凭据 → 启动 AP 模式
 *
 * AP 模式下 SSID = "Widget-Setup"，IP = 192.168.4.1
 * 用户通过后台配置 WiFi 后，调用 reconnectSTA() 切换回 STA 模式
 */
namespace WiFiManager {

static bool _apMode = false;

static const char *AP_SSID = "Widget-Setup";
static const char *AP_PASS = "12345678";   // 至少8位，或留空开放

static DNSServer s_dns;
static bool      s_dns_on = false;

static void stopDnsCaptive() {
    if (s_dns_on) {
        s_dns.stop();
        s_dns_on = false;
    }
}

/** AP 下轮询，须放在 loop 中（将任意域名解析到本机，便于手机弹出/打开配网页） */
inline void dnsLoop() {
    if (s_dns_on) s_dns.processNextRequest();
}

bool isAPMode()      { return _apMode; }
bool isConnected()   { return WiFi.status() == WL_CONNECTED; }
String localIP()     { return WiFi.localIP().toString(); }
int    rssi()        { return WiFi.RSSI(); }

// 启动 AP 热点
void startAP() {
    stopDnsCaptive();
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    // 固定网段，避免部分机型 DHCP/网关异常导致无法打开 http://192.168.4.1
    IPAddress ap_ip(192, 168, 4, 1);
    WiFi.softAPConfig(ap_ip, ap_ip, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.setSleep(false);  // AP 下关闭 WiFi 省电，减少连接后掉线、网页打不开
    _apMode = true;
    // 捕获 DNS，避免安卓/iOS 检测“无互联网”后拦截浏览器，无法打开 192.168.4.1
    s_dns.start(53, "*", WiFi.softAPIP());
    s_dns_on = true;
    Serial.println("[WiFi] DNS 辅助(53) 已启动（改善手机打开配网页）");
    Serial.printf("[WiFi] AP 模式启动: SSID=%s  IP=%s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
    Serial.println("[WiFi] Web 后台: http://192.168.4.1/");
}

// 尝试连接 STA，返回是否成功
bool connectSTA(const char *ssid, const char *pass, int timeout_s = 15) {
    if (!ssid || strlen(ssid) == 0) return false;

    stopDnsCaptive();
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    Serial.printf("[WiFi] 连接中: %s", ssid);
    int elapsed = 0;
    while (WiFi.status() != WL_CONNECTED && elapsed < timeout_s * 2) {
        delay(500);
        Serial.print(".");
        elapsed++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _apMode = false;
        WiFi.setSleep(false);
        Serial.printf("[WiFi] 已连接  IP=%s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Web 后台: http://%s/\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.println("[WiFi] 连接失败（ESP32 仅 2.4GHz：勿选 5G/6G 的 SSID）");
    return false;
}

// 使用 Preferences 中的凭据重连，失败则退回 AP
bool begin() {
    PreferencesManager::load();
    AppConfig &c = PreferencesManager::cfg;

    if (PreferencesManager::hasWiFiConfig()) {
        if (connectSTA(c.wifi_ssid, c.wifi_pass)) return true;
    }
    startAP();
    return false;
}

// 后台配置新凭据后调用，保存并切换 STA
bool reconnectSTA() {
    AppConfig &c = PreferencesManager::cfg;
    PreferencesManager::save();
    if (connectSTA(c.wifi_ssid, c.wifi_pass))
        return true;
    // 密码错误或路由器不可达时若不回退 AP，设备会“失联”，Web 再也无法打开
    Serial.println("[WiFi] 重连失败，恢复 AP 配置模式");
    startAP();
    return false;
}

} // namespace WiFiManager
