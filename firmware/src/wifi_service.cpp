#include "wifi_service.h"

#include <Arduino.h>
#include <WiFi.h>

#include "app_config.h"

namespace
{
    uint32_t lastReconnectAttemptMs = 0;
}

void WifiService::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void WifiService::loop()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return;
    }

    const uint32_t now = millis();

    if (now - lastReconnectAttemptMs >= AppConfig::WIFI_RECONNECT_PERIOD_MS)
    {
        lastReconnectAttemptMs = now;
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}

bool WifiService::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}
