#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "preferences_manager.h"

struct WeatherData {
    float temp;
    float feels_like;
    float temp_min;
    float temp_max;
    int   humidity;
    float wind_speed;
    char  description[32];
    char  city_name[32];
    int   condition_id;
    bool  valid;
};

// conditionToText() 已移至 screen_weather.h（展示逻辑归属展示层）

class WeatherClient {
public:
    bool fetch(WeatherData &out) {
        out.valid = false;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WX] WiFi not connected");
            return false;
        }

        AppConfig &c = PreferencesManager::cfg;
        if (!c.owm_key[0]) {
            Serial.println("[WX] owm_key empty, set in Web UI");
            return false;
        }
        if (!c.city_id[0]) {
            Serial.println("[WX] city_id empty");
            return false;
        }

        // HTTPS：部分 ESP/路由环境会拦截明文 HTTP，导致一直拉不到天气
        String url = "https://api.openweathermap.org/data/2.5/weather?id=";
        url += c.city_id;
        url += "&appid=";
        url += c.owm_key;
        url += "&units=";
        url += c.wx_units;
        url += "&lang=en";

        WiFiClientSecure sec;
        sec.setInsecure();  // OWM 公网证书链；若需校验可改为 setCACert 打包根证书

        HTTPClient http;
        http.setTimeout(20000);
        http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
        if (!http.begin(sec, url)) {
            Serial.println("[WX] http.begin failed");
            return false;
        }
        http.addHeader("Accept", "application/json");

        const int code = http.GET();
        if (code != HTTP_CODE_OK) {
            Serial.printf("[WX] HTTP %d: %s\n", code, http.errorToString(code).c_str());
            http.end();
            return false;
        }

        String payload = http.getString();
        http.end();

        StaticJsonDocument<1536> doc;
        const DeserializationError err = deserializeJson(doc, payload);
        if (err) {
            Serial.printf("[WX] JSON err: %s\n", err.c_str());
            return false;
        }
        if (doc["cod"].is<int>() && doc["cod"].as<int>() != 200) {
            Serial.printf("[WX] API cod: %d\n", doc["cod"].as<int>());
            return false;
        }

        JsonObject main = doc["main"];
        if (main.isNull()) {
            Serial.println("[WX] no main in JSON");
            return false;
        }

        out.temp         = main["temp"]       | 0.0f;
        out.feels_like   = main["feels_like"] | out.temp;
        out.temp_min     = main["temp_min"]   | out.temp;
        out.temp_max     = main["temp_max"]   | out.temp;
        out.humidity     = main["humidity"]   | 0;
        JsonObject wind  = doc["wind"];
        out.wind_speed   = wind.isNull() ? 0.0f : (float)(wind["speed"] | 0.0);
        strlcpy(out.city_name, doc["name"] | "Weather", sizeof(out.city_name));

        JsonArray wa = doc["weather"].as<JsonArray>();
        if (wa.isNull() || wa.size() == 0) {
            Serial.println("[WX] no weather[]");
            return false;
        }
        out.condition_id = wa[0]["id"] | 0;
        strlcpy(out.description, wa[0]["description"] | "N/A", sizeof(out.description));

        out.valid = true;
        Serial.println("[WX] fetch OK");
        return true;
    }
};
