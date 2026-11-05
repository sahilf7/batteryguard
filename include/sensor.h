#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

/* Sensor validity flags */
#define SENSOR_VALID_VOLTAGE     (1U << 0)
#define SENSOR_VALID_CURRENT     (1U << 1)
#define SENSOR_VALID_TEMPERATURE (1U << 2)

#define SENSOR_VALID_ALL         (0x07U)

/* One complete measurement cycle */
typedef struct
{
    float voltage_v;
    float current_a;
    float temperature_c;

    uint8_t valid_mask;

} BatteryMeasurements;

/* Acquire one measurement cycle */
#ifdef __cplusplus
extern "C" {
#endif

void sensor_read(BatteryMeasurements *measurements);

#ifdef __cplusplus
}
#endif

#endif