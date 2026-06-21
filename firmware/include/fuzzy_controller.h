#pragma once

class FuzzyController
{
public:
    float computeDutyPercent(float errorC, float deltaErrorC) const;

private:
    static float trapmf(float x, float a, float b, float c, float d);
    static float membershipError(unsigned int setIndex, float errorC);
    static float membershipDeltaError(unsigned int setIndex, float deltaErrorC);
    static float membershipOutput(unsigned int setIndex, float pwmPercent);
};
