#include "fan_pwm.h"

#include <Arduino.h>
#include <cmath>

#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "app_config.h"
#include "pins.h"

static uint32_t dutyPercentToRaw(float dutyPercent)
{
    const uint32_t maxRaw = (1UL << AppConfig::PWM_RESOLUTION_BITS) - 1UL;
    dutyPercent = constrain(dutyPercent, 0.0f, 100.0f);
    return static_cast<uint32_t>((dutyPercent / 100.0f) * maxRaw);
}

bool FanPwm::begin()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    const bool ok = ledcAttach(
        Pins::FAN_PWM,
        AppConfig::PWM_FREQUENCY_HZ,
        AppConfig::PWM_RESOLUTION_BITS
    );
#else
    ledcSetup(
        AppConfig::PWM_CHANNEL,
        AppConfig::PWM_FREQUENCY_HZ,
        AppConfig::PWM_RESOLUTION_BITS
    );

    ledcAttachPin(Pins::FAN_PWM, AppConfig::PWM_CHANNEL);
    const bool ok = true;
#endif

    setDutyPercent(0.0f);
    return ok;
}

void FanPwm::setDutyPercent(float dutyPercent)
{
    float duty = constrain(
        dutyPercent,
        0.0f,
        AppConfig::FAN_MAX_DUTY_PERCENT
    );

    if (duty < AppConfig::FAN_OFF_THRESHOLD_PERCENT)
    {
        duty = 0.0f;
    }
    else if (duty < AppConfig::FAN_MIN_EFFECTIVE_DUTY_PERCENT)
    {
        duty = AppConfig::FAN_MIN_EFFECTIVE_DUTY_PERCENT;
    }

    const uint32_t rawDuty = dutyPercentToRaw(duty);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(Pins::FAN_PWM, rawDuty);
#else
    ledcWrite(AppConfig::PWM_CHANNEL, rawDuty);
#endif

    currentDutyPercent_ = duty;
}

float FanPwm::dutyPercent() const
{
    return currentDutyPercent_;
}
