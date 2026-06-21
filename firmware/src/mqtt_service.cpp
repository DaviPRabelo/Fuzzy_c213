#include "mqtt_service.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "app_config.h"
#include "mqtt_topics.h"
#include "wifi_service.h"

namespace
{
    WiFiClient wifiClient;
    PubSubClient mqttClient(wifiClient);

    MqttService::SetpointCallback setpointCallback = nullptr;
    uint32_t lastReconnectAttemptMs = 0;

    bool parseFloatFromText(const String& text, float& value)
    {
        char* endPtr = nullptr;
        value = strtof(text.c_str(), &endPtr);
        return endPtr != text.c_str();
    }

    bool parseSetpointPayload(const uint8_t* payload, unsigned int length, float& setpointC)
    {
        String message;

        for (unsigned int i = 0; i < length; ++i)
        {
            message += static_cast<char>(payload[i]);
        }

        message.trim();

        if (message.length() == 0)
        {
            return false;
        }

        if (!message.startsWith("{"))
        {
            return parseFloatFromText(message, setpointC);
        }

        int keyIndex = message.indexOf("setpoint");

        if (keyIndex < 0)
        {
            keyIndex = message.indexOf("\"sp\"");
        }

        if (keyIndex < 0)
        {
            return false;
        }

        const int colonIndex = message.indexOf(':', keyIndex);

        if (colonIndex < 0)
        {
            return false;
        }

        int endIndex = message.indexOf(',', colonIndex);

        if (endIndex < 0)
        {
            endIndex = message.indexOf('}', colonIndex);
        }

        if (endIndex < 0)
        {
            endIndex = message.length();
        }

        String numberText = message.substring(colonIndex + 1, endIndex);
        numberText.trim();

        return parseFloatFromText(numberText, setpointC);
    }

    void onMqttMessage(char* topic, uint8_t* payload, unsigned int length)
    {
        if (strcmp(topic, MqttTopics::SETPOINT) != 0)
        {
            return;
        }

        float receivedSetpoint = 0.0f;

        if (!parseSetpointPayload(payload, length, receivedSetpoint))
        {
            MqttService::publishStatus("invalid_setpoint_payload");
            return;
        }

        if (setpointCallback != nullptr)
        {
            setpointCallback(receivedSetpoint);
        }
    }

    String buildClientId()
    {
        String clientId = AppConfig::DEVICE_ID;
        clientId += "-";
        clientId += String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
        return clientId;
    }

    void connectMqtt()
    {
        if (!WifiService::isConnected())
        {
            return;
        }

        const String clientId = buildClientId();

        bool connected = false;

        if (strlen(MQTT_USER) > 0)
        {
            connected = mqttClient.connect(
                clientId.c_str(),
                MQTT_USER,
                MQTT_PASSWORD,
                MqttTopics::STATUS,
                1,
                true,
                "offline"
            );
        }
        else
        {
            connected = mqttClient.connect(
                clientId.c_str(),
                MqttTopics::STATUS,
                1,
                true,
                "offline"
            );
        }

        if (connected)
        {
            mqttClient.publish(MqttTopics::STATUS, "online", true);
            mqttClient.subscribe(MqttTopics::SETPOINT);
        }
    }

    void appendFloatOrNull(String& target, float value, unsigned int decimals)
    {
        if (std::isfinite(value))
        {
            target += String(value, decimals);
        }
        else
        {
            target += "null";
        }
    }
}

void MqttService::begin(SetpointCallback callback)
{
    setpointCallback = callback;

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(onMqttMessage);
    mqttClient.setBufferSize(512);
}

void MqttService::loop()
{
    if (mqttClient.connected())
    {
        mqttClient.loop();
        return;
    }

    const uint32_t now = millis();

    if (now - lastReconnectAttemptMs >= AppConfig::MQTT_RECONNECT_PERIOD_MS)
    {
        lastReconnectAttemptMs = now;
        connectMqtt();
    }
}

bool MqttService::isConnected()
{
    return mqttClient.connected();
}

void MqttService::publishTelemetry(const TelemetryData& data)
{
    if (!mqttClient.connected())
    {
        return;
    }

    String payload = "{";

    payload += "\"device_id\":\"";
    payload += AppConfig::DEVICE_ID;
    payload += "\",";

    payload += "\"temp_c\":";
    appendFloatOrNull(payload, data.temperatureC, 2);
    payload += ",";

    payload += "\"setpoint_c\":";
    appendFloatOrNull(payload, data.setpointC, 2);
    payload += ",";

    payload += "\"error_c\":";
    appendFloatOrNull(payload, data.errorC, 2);
    payload += ",";

    payload += "\"delta_error_c\":";
    appendFloatOrNull(payload, data.deltaErrorC, 2);
    payload += ",";

    payload += "\"pwm_pct\":";
    appendFloatOrNull(payload, data.pwmDutyPercent, 1);
    payload += ",";

    payload += "\"sensor_ok\":";
    payload += data.sensorOk ? "true" : "false";
    payload += ",";

    payload += "\"safety_mode\":";
    payload += data.safetyMode ? "true" : "false";
    payload += ",";

    payload += "\"uptime_ms\":";
    payload += String(data.uptimeMs);

    payload += "}";

    mqttClient.publish(MqttTopics::TELEMETRY, payload.c_str(), false);
}

void MqttService::publishStatus(const char* status)
{
    if (!mqttClient.connected())
    {
        return;
    }

    mqttClient.publish(MqttTopics::STATUS, status, true);
}
