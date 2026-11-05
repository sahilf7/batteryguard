#include <stddef.h>

#include "sensor.h"

/* Predefined deterministic sensor measurements */
/* Deterministic sensor measurements and fault scenarios */
static const BatteryMeasurements samples[] =
{
    /* Original five normal operating samples */
    {
        .voltage_v = 12.50f,
        .current_a = 1.80f,
        .temperature_c = 29.0f,
        .valid_mask = SENSOR_VALID_ALL
    },
    {
        .voltage_v = 12.48f,
        .current_a = 2.00f,
        .temperature_c = 29.5f,
        .valid_mask = SENSOR_VALID_ALL
    },
    {
        .voltage_v = 12.45f,
        .current_a = 2.20f,
        .temperature_c = 30.0f,
        .valid_mask = SENSOR_VALID_ALL
    },
    {
        .voltage_v = 12.42f,
        .current_a = 2.10f,
        .temperature_c = 30.5f,
        .valid_mask = SENSOR_VALID_ALL
    },
    {
        .voltage_v = 12.40f,
        .current_a = 1.90f,
        .temperature_c = 30.0f,
        .valid_mask = SENSOR_VALID_ALL
    },

    /* Cycle 6: undervoltage warning */
    {
        .voltage_v = 11.40f,
        .current_a = 1.90f,
        .temperature_c = 30.0f,
        .valid_mask = SENSOR_VALID_ALL
    },

    /* Cycle 7: critical undervoltage */
    {
        .voltage_v = 10.00f,
        .current_a = 1.90f,
        .temperature_c = 30.0f,
        .valid_mask = SENSOR_VALID_ALL
    },

    /* Cycles 8-10: consecutive safe recovery samples */
    {
        .voltage_v = 12.50f,
        .current_a = 1.50f,
        .temperature_c = 25.0f,
        .valid_mask = SENSOR_VALID_ALL
    },
    {
        .voltage_v = 12.50f,
        .current_a = 1.50f,
        .temperature_c = 25.0f,
        .valid_mask = SENSOR_VALID_ALL
    },
    {
        .voltage_v = 12.50f,
        .current_a = 1.50f,
        .temperature_c = 25.0f,
        .valid_mask = SENSOR_VALID_ALL
    }
};

/* Number of predefined samples */
#define SAMPLE_COUNT (sizeof(samples) / sizeof(samples[0]))

void sensor_read(BatteryMeasurements *measurements)
{
    static size_t sample_index = 0U;

    /* Protect against a NULL pointer */
    if (measurements == NULL)
    {
        return;
    }

    /* Copy the current sample into the caller's structure */
    *measurements = samples[sample_index];

    /* Advance until the final sample */
    if (sample_index < SAMPLE_COUNT - 1U)
    {
        sample_index++;
    }
}