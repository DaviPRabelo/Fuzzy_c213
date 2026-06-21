#pragma once

class ITemperatureSensor
{
public:
    virtual ~ITemperatureSensor() = default;

    virtual bool begin() = 0;
    virtual bool readTemperatureC(float& temperatureC) = 0;
    virtual const char* name() const = 0;
};
