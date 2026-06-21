#pragma once

class FanPwm
{
public:
    bool begin();
    void setDutyPercent(float dutyPercent);
    float dutyPercent() const;

private:
    float currentDutyPercent_ = 0.0f;
};
