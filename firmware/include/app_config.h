#pragma once

#include <stdint.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

namespace AppConfig
{
    constexpr const char* DEVICE_ID = "thermal-chamber-esp32-01";

    constexpr uint32_t SERIAL_BAUDRATE = 115200;

    constexpr uint32_t CONTROL_PERIOD_MS = 1000;
    constexpr uint32_t TELEMETRY_PERIOD_MS = 2000;
    constexpr uint32_t WIFI_RECONNECT_PERIOD_MS = 5000;
    constexpr uint32_t MQTT_RECONNECT_PERIOD_MS = 5000;

    constexpr float DEFAULT_SETPOINT_C = 45.0f;
    constexpr float SETPOINT_MIN_C = 25.0f;
    constexpr float SETPOINT_MAX_C = 80.0f;

    constexpr float TEMP_SENSOR_MIN_C = -40.0f;
    constexpr float TEMP_SENSOR_MAX_C = 85.0f;
    constexpr float TEMP_CRITICAL_C = 84.0f;

    constexpr uint8_t SENSOR_MAX_CONSECUTIVE_FAILURES = 3;

    constexpr uint32_t PWM_FREQUENCY_HZ = 25000;
    constexpr uint8_t PWM_RESOLUTION_BITS = 10;
    constexpr uint8_t PWM_CHANNEL = 0;

    constexpr float FAN_OFF_THRESHOLD_PERCENT = 3.0f;
    constexpr float FAN_MIN_EFFECTIVE_DUTY_PERCENT = 20.0f;
    constexpr float FAN_MAX_DUTY_PERCENT = 100.0f;

    constexpr float MAX_PWM_STEP_PERCENT = 12.0f;
}
