#pragma once

#define MQTT_BASE_TOPIC "thermal_chamber/c3_01"

namespace MqttTopics
{
    constexpr const char* SETPOINT = MQTT_BASE_TOPIC "/setpoint";
    constexpr const char* TELEMETRY = MQTT_BASE_TOPIC "/telemetry";
    constexpr const char* STATUS = MQTT_BASE_TOPIC "/status";
}
