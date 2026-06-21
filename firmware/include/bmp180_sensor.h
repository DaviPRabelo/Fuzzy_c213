#pragma once

#include <Adafruit_BMP085.h>
#include "i_temperature_sensor.h"

class Bmp180TemperatureSensor final : public ITemperatureSensor
{
public:
    bool begin() override;
    bool readTemperatureC(float& temperatureC) override;
    const char* name() const override;

private:
    Adafruit_BMP085 bmp_;
};
