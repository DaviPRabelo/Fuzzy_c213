#pragma once

#include <stdint.h>

struct ControlInput
{
    float temperatureC;
    float setpointC;
    float errorC;
    float deltaErrorC;
    bool sensorOk;
    uint32_t timestampMs;
};

struct ControlOutput
{
    float pwmDutyPercent;
    float errorC;
    float deltaErrorC;
};

struct TelemetryData
{
    float temperatureC;
    float setpointC;
    float errorC;
    float deltaErrorC;
    float pwmDutyPercent;
    bool sensorOk;
    bool safetyMode;
    uint32_t uptimeMs;
};
