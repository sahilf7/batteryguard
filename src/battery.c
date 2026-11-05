
#include "battery.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

/* Independent warning flags */
#define WARNING_UNDERVOLTAGE   (1U << 0)
#define WARNING_OVERVOLTAGE    (1U << 1)
#define WARNING_OVERCURRENT    (1U << 2)
#define WARNING_OVERTEMPERATURE (1U << 3)

/* ---------------------------------------------------------
 * Sensor validation
 * --------------------------------------------------------- */

static uint8_t sensor_channel_valid(
    const BatteryMeasurements *measurements,
    uint8_t flag,
    float value
)
{
    return (uint8_t)(
        ((measurements->valid_mask & flag) != 0U) &&
        (isfinite(value) != 0)
    );
}

static uint8_t measurements_valid(
    const BatteryMeasurements *measurements
)
{
    return (uint8_t)(
        (measurements->valid_mask == SENSOR_VALID_ALL) &&
        (isfinite(measurements->voltage_v) != 0) &&
        (isfinite(measurements->current_a) != 0) &&
        (isfinite(measurements->temperature_c) != 0)
    );
}

/* ---------------------------------------------------------
 * Fault priority
 * Lower rank means higher priority.
 * --------------------------------------------------------- */

static uint8_t fault_priority(BatteryFault fault)
{
    switch (fault)
    {
        case FAULT_SENSOR:
            return 0U;

        case FAULT_OVERTEMPERATURE:
            return 1U;

        case FAULT_OVERCURRENT:
            return 2U;

        case FAULT_OVERVOLTAGE:
            return 3U;

        case FAULT_UNDERVOLTAGE:
            return 4U;

        case FAULT_NONE:
        default:
            return UINT8_MAX;
    }
}

static void select_fault(
    BatteryFault *selected,
    BatteryFault candidate
)
{
    if (fault_priority(candidate) < fault_priority(*selected))
    {
        *selected = candidate;
    }
}

/* ---------------------------------------------------------
 * Critical fault evaluation
 * Evaluate every individually valid sensor channel.
 * --------------------------------------------------------- */

static BatteryFault evaluate_critical_fault(
    const BatteryMonitor *monitor,
    const BatteryMeasurements *measurements
)
{
    BatteryFault fault = FAULT_NONE;
    const BatteryConfig *config = &monitor->config;

    if (sensor_channel_valid(
            measurements,
            SENSOR_VALID_VOLTAGE,
            measurements->voltage_v) != 0U)
    {
        if (measurements->voltage_v > config->voltage_max_v)
        {
            select_fault(&fault, FAULT_OVERVOLTAGE);
        }
        else if (measurements->voltage_v < config->voltage_min_v)
        {
            select_fault(&fault, FAULT_UNDERVOLTAGE);
        }
    }

    if (sensor_channel_valid(
            measurements,
            SENSOR_VALID_CURRENT,
            measurements->current_a) != 0U)
    {
        if (fabsf(measurements->current_a) > config->current_max_a)
        {
            select_fault(&fault, FAULT_OVERCURRENT);
        }
    }

    if (sensor_channel_valid(
            measurements,
            SENSOR_VALID_TEMPERATURE,
            measurements->temperature_c) != 0U)
    {
        if (measurements->temperature_c > config->temperature_max_c)
        {
            select_fault(&fault, FAULT_OVERTEMPERATURE);
        }
    }

    return fault;
}

/* ---------------------------------------------------------
 * Warning hysteresis
 * Each warning has independent activation and recovery.
 * --------------------------------------------------------- */

static void update_warning(
    uint8_t *mask,
    uint8_t flag,
    uint8_t activate,
    uint8_t recover
)
{
    if (activate != 0U)
    {
        *mask |= flag;
    }
    else if (recover != 0U)
    {
        *mask &= (uint8_t)~flag;
    }
}

static void update_warning_mask(
    BatteryMonitor *monitor,
    const BatteryMeasurements *measurements
)
{
    const BatteryConfig *config = &monitor->config;
    const float current = fabsf(measurements->current_a);

    update_warning(
        &monitor->warning_mask,
        WARNING_UNDERVOLTAGE,
        (uint8_t)(measurements->voltage_v < config->voltage_warn_min_v),
        (uint8_t)(measurements->voltage_v >= config->voltage_recover_min_v)
    );

    update_warning(
        &monitor->warning_mask,
        WARNING_OVERVOLTAGE,
        (uint8_t)(measurements->voltage_v > config->voltage_warn_max_v),
        (uint8_t)(measurements->voltage_v <= config->voltage_recover_max_v)
    );

    update_warning(
        &monitor->warning_mask,
        WARNING_OVERCURRENT,
        (uint8_t)(current > config->current_warn_a),
        (uint8_t)(current <= config->current_recover_a)
    );

    update_warning(
        &monitor->warning_mask,
        WARNING_OVERTEMPERATURE,
        (uint8_t)(measurements->temperature_c > config->temperature_warn_c),
        (uint8_t)(
            measurements->temperature_c <= config->temperature_recover_c
        )
    );
}

/* ---------------------------------------------------------
 * Fault recovery
 * All channels must be valid and all conditions safe.
 * --------------------------------------------------------- */

static uint8_t recovery_conditions_met(
    const BatteryMonitor *monitor,
    const BatteryMeasurements *measurements
)
{
    const BatteryConfig *config = &monitor->config;

    return (uint8_t)(
        (measurements->voltage_v >= config->voltage_recover_min_v) &&
        (measurements->voltage_v <= config->voltage_recover_max_v) &&
        (fabsf(measurements->current_a) <= config->current_recover_a) &&
        (measurements->temperature_c <= config->temperature_recover_c)
    );
}

static void update_recovery(
    BatteryMonitor *monitor,
    const BatteryMeasurements *measurements
)
{
    if ((measurements_valid(measurements) == 0U) ||
        (recovery_conditions_met(monitor, measurements) == 0U))
    {
        monitor->recovery_count = 0U;
        return;
    }

    if (monitor->recovery_count < UINT8_MAX)
    {
        monitor->recovery_count++;
    }

    if (monitor->recovery_count >=
        monitor->config.recovery_samples_required)
    {
        monitor->status.state = BATTERY_NORMAL;
        monitor->status.fault = FAULT_NONE;

        monitor->recovery_count = 0U;
        monitor->warning_mask = 0U;
    }
}

/* ---------------------------------------------------------
 * Initialization
 * --------------------------------------------------------- */

void battery_init(
    BatteryMonitor *monitor,
    const BatteryConfig *config
)
{
    if (monitor == NULL)
    {
        return;
    }

    monitor->config = (config != NULL)
        ? *config
        : (BatteryConfig){0};

    monitor->status = (BatteryStatus){0};

    monitor->status.state = (config != NULL)
        ? BATTERY_NORMAL
        : BATTERY_FAULT;

    monitor->status.fault = (config != NULL)
        ? FAULT_NONE
        : FAULT_SENSOR;

    monitor->recovery_count = 0U;
    monitor->invalid_sample_count = 0U;
    monitor->warning_mask = 0U;
}

/* ---------------------------------------------------------
 * Main protection update
 * Called once per measurement cycle.
 * --------------------------------------------------------- */

void battery_update(
    BatteryMonitor *monitor,
    const BatteryMeasurements *measurements
)
{
    BatteryFault detected_fault;
    uint8_t valid;

    if (monitor == NULL)
    {
        return;
    }

    if (measurements == NULL)
    {
        monitor->status.measurements = (BatteryMeasurements){0};
        monitor->invalid_sample_count =
            (monitor->invalid_sample_count < UINT8_MAX)
            ? (uint8_t)(monitor->invalid_sample_count + 1U)
            : UINT8_MAX;

        monitor->recovery_count = 0U;

        if ((monitor->config.sensor_fault_count == 0U) ||
            (monitor->invalid_sample_count >=
             monitor->config.sensor_fault_count))
        {
            monitor->status.state = BATTERY_FAULT;
            monitor->status.fault = FAULT_SENSOR;
        }

        return;
    }

    monitor->status.measurements = *measurements;

    valid = measurements_valid(measurements);

    if (valid != 0U)
    {
        monitor->invalid_sample_count = 0U;
    }
    else if (monitor->invalid_sample_count < UINT8_MAX)
    {
        monitor->invalid_sample_count++;
    }

    /* Check critical faults on available valid channels. */
    detected_fault = evaluate_critical_fault(monitor, measurements);

    /* Persistent sensor failure has the highest priority. */
    if ((monitor->config.sensor_fault_count == 0U) ||
        (monitor->invalid_sample_count >=
         monitor->config.sensor_fault_count))
    {
        select_fault(&detected_fault, FAULT_SENSOR);
    }

    if (detected_fault != FAULT_NONE)
    {
        if (monitor->status.state != BATTERY_FAULT)
        {
            monitor->status.state = BATTERY_FAULT;
            monitor->status.fault = detected_fault;
        }
        else if (fault_priority(detected_fault) <
                 fault_priority(monitor->status.fault))
        {
            monitor->status.fault = detected_fault;
        }

        monitor->recovery_count = 0U;
        return;
    }

    /* A fault remains latched until recovery completes. */
    if (monitor->status.state == BATTERY_FAULT)
    {
        update_recovery(monitor, measurements);
        return;
    }

    /* Do not evaluate warnings from incomplete measurements. */
    if (valid == 0U)
    {
        return;
    }

    update_warning_mask(monitor, measurements);

    if (monitor->warning_mask != 0U)
    {
        monitor->status.state = BATTERY_WARNING;
    }
    else
    {
        monitor->status.state = BATTERY_NORMAL;
    }

    monitor->status.fault = FAULT_NONE;
}

/* ---------------------------------------------------------
 * Status retrieval
 * --------------------------------------------------------- */

BatteryStatus battery_get_status(
    const BatteryMonitor *monitor
)
{
    if (monitor == NULL)
    {
        BatteryStatus invalid_status = {0};

        invalid_status.state = BATTERY_FAULT;
        invalid_status.fault = FAULT_SENSOR;

        return invalid_status;
    }

    return monitor->status;
}
