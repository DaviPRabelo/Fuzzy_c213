#pragma once

#include "control_types.h"

namespace MqttService
{
    using SetpointCallback = void (*)(float setpointC);

    void begin(SetpointCallback callback);
    void loop();
    bool isConnected();

    void publishTelemetry(const TelemetryData& data);
    void publishStatus(const char* status);
}
