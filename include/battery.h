#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include "sensor.h"

typedef enum
{
    BATTERY_NORMAL = 0,
    BATTERY_WARNING,
    BATTERY_FAULT

} BatteryState;

typedef enum
{
    FAULT_NONE = 0,
    FAULT_OVERVOLTAGE,
    FAULT_UNDERVOLTAGE,
    FAULT_OVERCURRENT,
    FAULT_OVERTEMPERATURE,
    FAULT_SENSOR

} BatteryFault;

typedef struct
{
    float voltage_warn_min_v;
    float voltage_warn_max_v;
    float current_warn_a;
    float temperature_warn_c;

    float voltage_min_v;
    float voltage_max_v;
    float current_max_a;
    float temperature_max_c;

    float voltage_recover_min_v;
    float voltage_recover_max_v;
    float current_recover_a;
    float temperature_recover_c;

    uint8_t recovery_samples_required;
    uint8_t sensor_fault_count;

} BatteryConfig;

typedef struct
{
    BatteryState state;
    BatteryFault fault;

    BatteryMeasurements measurements;

} BatteryStatus;

typedef struct
{
    BatteryConfig config;
    BatteryStatus status;

    uint8_t recovery_count;
    uint8_t invalid_sample_count;
    uint8_t warning_mask;

} BatteryMonitor;

#ifdef __cplusplus
extern "C" {
#endif

void battery_init(
    BatteryMonitor *monitor,
    const BatteryConfig *config
);

void battery_update(
    BatteryMonitor *monitor,
    const BatteryMeasurements *measurements
);

BatteryStatus battery_get_status(
    const BatteryMonitor *monitor
);

#ifdef __cplusplus
}
#endif

#endif