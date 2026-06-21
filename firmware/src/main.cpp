#include <Arduino.h>
#include <Wire.h>
#include <cmath>

#include "app_config.h"
#include "pins.h"
#include "bmp180_sensor.h"
#include "fan_pwm.h"
#include "fuzzy_controller.h"
#include "wifi_service.h"
#include "mqtt_service.h"
#include "control_types.h"

namespace
{
    Bmp180TemperatureSensor temperatureSensor;
    FanPwm fan;
    FuzzyController fuzzyController;

    float setpointC = AppConfig::DEFAULT_SETPOINT_C;

    float lastTemperatureC = NAN;
    float lastErrorC = 0.0f;
    float lastDeltaErrorC = 0.0f;
    float lastPwmDutyPercent = 0.0f;

    bool lastSensorOk = false;
    bool safetyMode = false;

    uint8_t consecutiveSensorFailures = 0;

    uint32_t lastControlUpdateMs = 0;
    uint32_t lastTelemetryMs = 0;

    float rateLimit(float target, float current, float maxStep)
    {
        if (target > current + maxStep)
        {
            return current + maxStep;
        }

        if (target < current - maxStep)
        {
            return current - maxStep;
        }

        return target;
    }

    void onSetpointReceived(float candidateSetpointC)
    {
        if (!std::isfinite(candidateSetpointC))
        {
            MqttService::publishStatus("setpoint_rejected_not_finite");
            return;
        }

        if (
            candidateSetpointC < AppConfig::SETPOINT_MIN_C ||
            candidateSetpointC > AppConfig::SETPOINT_MAX_C
        )
        {
            MqttService::publishStatus("setpoint_rejected_out_of_range");
            return;
        }

        setpointC = candidateSetpointC;

        Serial.print("Novo setpoint recebido: ");
        Serial.print(setpointC);
        Serial.println(" C");

        MqttService::publishStatus("setpoint_updated");
    }

    void updateControlLoop()
    {
        float measuredTemperatureC = NAN;
        const bool sensorOk = temperatureSensor.readTemperatureC(measuredTemperatureC);

        lastSensorOk = sensorOk;

        if (!sensorOk)
        {
            ++consecutiveSensorFailures;

            if (consecutiveSensorFailures >= AppConfig::SENSOR_MAX_CONSECUTIVE_FAILURES)
            {
                safetyMode = true;
                lastPwmDutyPercent = AppConfig::FAN_MAX_DUTY_PERCENT;
                fan.setDutyPercent(lastPwmDutyPercent);
                MqttService::publishStatus("sensor_failure_fan_forced_max");
            }

            return;
        }

        consecutiveSensorFailures = 0;
        lastTemperatureC = measuredTemperatureC;

        const float errorC = measuredTemperatureC - setpointC;
        const float deltaErrorC = errorC - lastErrorC;

        float requestedPwmPercent = fuzzyController.computeDutyPercent(errorC, deltaErrorC);

        safetyMode = false;

        if (measuredTemperatureC >= AppConfig::TEMP_CRITICAL_C)
        {
            safetyMode = true;
            requestedPwmPercent = AppConfig::FAN_MAX_DUTY_PERCENT;
        }

        float limitedPwmPercent = requestedPwmPercent;

        if (!safetyMode)
        {
            limitedPwmPercent = rateLimit(
                requestedPwmPercent,
                lastPwmDutyPercent,
                AppConfig::MAX_PWM_STEP_PERCENT
            );
        }

        fan.setDutyPercent(limitedPwmPercent);

        lastErrorC = errorC;
        lastDeltaErrorC = deltaErrorC;
        lastPwmDutyPercent = fan.dutyPercent();

        Serial.print("T=");
        Serial.print(measuredTemperatureC, 2);
        Serial.print(" C | SP=");
        Serial.print(setpointC, 2);
        Serial.print(" C | e=");
        Serial.print(errorC, 2);
        Serial.print(" C | de=");
        Serial.print(deltaErrorC, 2);
        Serial.print(" C | PWM=");
        Serial.print(lastPwmDutyPercent, 1);
        Serial.println(" %");
    }

    void publishTelemetry()
    {
        TelemetryData telemetry;

        telemetry.temperatureC = lastTemperatureC;
        telemetry.setpointC = setpointC;
        telemetry.errorC = lastErrorC;
        telemetry.deltaErrorC = lastDeltaErrorC;
        telemetry.pwmDutyPercent = lastPwmDutyPercent;
        telemetry.sensorOk = lastSensorOk;
        telemetry.safetyMode = safetyMode;
        telemetry.uptimeMs = millis();

        MqttService::publishTelemetry(telemetry);
    }
}

void setup()
{
    Serial.begin(AppConfig::SERIAL_BAUDRATE);
    delay(500);

    Serial.println();
    Serial.println("Inicializando camara termica fuzzy - ESP32");

    Wire.begin(Pins::I2C_SDA, Pins::I2C_SCL);

    if (!temperatureSensor.begin())
    {
        Serial.println("Falha ao inicializar BMP180. Ventoinha sera acionada em seguranca.");
        safetyMode = true;
    }
    else
    {
        Serial.print("Sensor inicializado: ");
        Serial.println(temperatureSensor.name());
    }

    if (!fan.begin())
    {
        Serial.println("Falha ao inicializar PWM da ventoinha.");
    }

    if (safetyMode)
    {
        fan.setDutyPercent(AppConfig::FAN_MAX_DUTY_PERCENT);
    }

    WifiService::begin();
    MqttService::begin(onSetpointReceived);

    lastControlUpdateMs = millis();
    lastTelemetryMs = millis();
}

void loop()
{
    WifiService::loop();
    MqttService::loop();

    const uint32_t now = millis();

    if (now - lastControlUpdateMs >= AppConfig::CONTROL_PERIOD_MS)
    {
        lastControlUpdateMs = now;
        updateControlLoop();
    }

    if (now - lastTelemetryMs >= AppConfig::TELEMETRY_PERIOD_MS)
    {
        lastTelemetryMs = now;
        publishTelemetry();
    }
}
