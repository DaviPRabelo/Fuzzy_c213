#include "bmp180_sensor.h"

#include <cmath>
#include "app_config.h"

bool Bmp180TemperatureSensor::begin()
{
    return bmp_.begin();
}

bool Bmp180TemperatureSensor::readTemperatureC(float& temperatureC)
{
    const float value = bmp_.readTemperature();

    if (!std::isfinite(value))
    {
        return false;
    }

    if (value < AppConfig::TEMP_SENSOR_MIN_C || value > AppConfig::TEMP_SENSOR_MAX_C)
    {
        return false;
    }

    temperatureC = value;
    return true;
}

const char* Bmp180TemperatureSensor::name() const
{
    return "BMP180";
}
