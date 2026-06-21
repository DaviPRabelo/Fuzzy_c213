#include "fuzzy_controller.h"

#include <algorithm>
#include <cmath>

namespace
{
    enum ErrorSet : unsigned int
    {
        ERROR_NEG_BIG = 0,
        ERROR_NEG_SMALL,
        ERROR_ZERO,
        ERROR_POS_SMALL,
        ERROR_POS_BIG,
        ERROR_COUNT
    };

    enum DeltaErrorSet : unsigned int
    {
        DELTA_FALLING = 0,
        DELTA_STABLE,
        DELTA_RISING,
        DELTA_COUNT
    };

    enum OutputSet : unsigned int
    {
        OUT_OFF = 0,
        OUT_LOW,
        OUT_MEDIUM,
        OUT_HIGH,
        OUT_MAX,
        OUT_COUNT
    };

    constexpr OutputSet RULES[ERROR_COUNT][DELTA_COUNT] =
    {
        // Delta erro:  caindo        estavel       subindo
        /* e NG */    { OUT_OFF,     OUT_OFF,      OUT_LOW    },
        /* e NP */    { OUT_OFF,     OUT_LOW,      OUT_MEDIUM },
        /* e ZE */    { OUT_LOW,     OUT_MEDIUM,   OUT_HIGH   },
        /* e PP */    { OUT_MEDIUM,  OUT_HIGH,     OUT_MAX    },
        /* e PG */    { OUT_HIGH,    OUT_MAX,      OUT_MAX    }
    };
}

float FuzzyController::trapmf(float x, float a, float b, float c, float d)
{
    if (a == b && x <= b)
    {
        return 1.0f;
    }

    if (c == d && x >= c)
    {
        return 1.0f;
    }

    if (x <= a || x >= d)
    {
        return 0.0f;
    }

    if (x >= b && x <= c)
    {
        return 1.0f;
    }

    if (x > a && x < b)
    {
        return (x - a) / (b - a);
    }

    if (x > c && x < d)
    {
        return (d - x) / (d - c);
    }

    return 0.0f;
}

float FuzzyController::membershipError(unsigned int setIndex, float errorC)
{
    switch (setIndex)
    {
        case ERROR_NEG_BIG:
            return trapmf(errorC, -30.0f, -30.0f, -15.0f, -5.0f);

        case ERROR_NEG_SMALL:
            return trapmf(errorC, -15.0f, -6.0f, -2.0f, 0.0f);

        case ERROR_ZERO:
            return trapmf(errorC, -1.5f, -0.4f, 0.4f, 1.5f);

        case ERROR_POS_SMALL:
            return trapmf(errorC, 0.0f, 2.0f, 6.0f, 15.0f);

        case ERROR_POS_BIG:
            return trapmf(errorC, 5.0f, 15.0f, 30.0f, 30.0f);

        default:
            return 0.0f;
    }
}

float FuzzyController::membershipDeltaError(unsigned int setIndex, float deltaErrorC)
{
    switch (setIndex)
    {
        case DELTA_FALLING:
            return trapmf(deltaErrorC, -3.0f, -3.0f, -1.0f, -0.15f);

        case DELTA_STABLE:
            return trapmf(deltaErrorC, -0.5f, -0.1f, 0.1f, 0.5f);

        case DELTA_RISING:
            return trapmf(deltaErrorC, 0.15f, 1.0f, 3.0f, 3.0f);

        default:
            return 0.0f;
    }
}

float FuzzyController::membershipOutput(unsigned int setIndex, float pwmPercent)
{
    switch (setIndex)
    {
        case OUT_OFF:
            return trapmf(pwmPercent, 0.0f, 0.0f, 0.0f, 8.0f);

        case OUT_LOW:
            return trapmf(pwmPercent, 5.0f, 15.0f, 25.0f, 40.0f);

        case OUT_MEDIUM:
            return trapmf(pwmPercent, 30.0f, 45.0f, 55.0f, 70.0f);

        case OUT_HIGH:
            return trapmf(pwmPercent, 60.0f, 75.0f, 85.0f, 95.0f);

        case OUT_MAX:
            return trapmf(pwmPercent, 85.0f, 95.0f, 100.0f, 100.0f);

        default:
            return 0.0f;
    }
}

float FuzzyController::computeDutyPercent(float errorC, float deltaErrorC) const
{
    float numerator = 0.0f;
    float denominator = 0.0f;

    for (int pwm = 0; pwm <= 100; ++pwm)
    {
        float aggregatedMembership = 0.0f;

        for (unsigned int e = 0; e < ERROR_COUNT; ++e)
        {
            const float muError = membershipError(e, errorC);

            for (unsigned int de = 0; de < DELTA_COUNT; ++de)
            {
                const float muDelta = membershipDeltaError(de, deltaErrorC);
                const float ruleStrength = std::min(muError, muDelta);

                if (ruleStrength <= 0.0f)
                {
                    continue;
                }

                const OutputSet outputSet = RULES[e][de];
                const float muOutput = membershipOutput(outputSet, static_cast<float>(pwm));
                const float clippedOutput = std::min(ruleStrength, muOutput);

                aggregatedMembership = std::max(aggregatedMembership, clippedOutput);
            }
        }

        numerator += static_cast<float>(pwm) * aggregatedMembership;
        denominator += aggregatedMembership;
    }

    if (denominator <= 0.0001f)
    {
        return 0.0f;
    }

    return numerator / denominator;
}
