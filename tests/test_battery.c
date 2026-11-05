
#include "battery.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static BatteryConfig test_config(void)
{
    BatteryConfig config = {
        .voltage_warn_min_v = 11.5f,
        .voltage_warn_max_v = 14.5f,
        .current_warn_a = 5.0f,
        .temperature_warn_c = 45.0f,

        .voltage_min_v = 10.5f,
        .voltage_max_v = 15.0f,
        .current_max_a = 10.0f,
        .temperature_max_c = 60.0f,

        .voltage_recover_min_v = 11.8f,
        .voltage_recover_max_v = 14.2f,
        .current_recover_a = 4.0f,
        .temperature_recover_c = 40.0f,

        .recovery_samples_required = 3U,
        .sensor_fault_count = 3U
    };

    return config;
}

static BatteryMeasurements safe_measurement(void)
{
    BatteryMeasurements measurements = {
        .voltage_v = 12.5f,
        .current_a = 1.5f,
        .temperature_c = 25.0f,
        .valid_mask = SENSOR_VALID_ALL
    };

    return measurements;
}

static void reset_monitor(BatteryMonitor *monitor)
{
    BatteryConfig config = test_config();
    battery_init(monitor, &config);
}

static void assert_state(
    const BatteryMonitor *monitor,
    BatteryState expected
)
{
    BatteryStatus status = battery_get_status(monitor);

    if (status.state != expected)
    {
        fprintf(
            stderr,
            "State mismatch: expected=%d, actual=%d, fault=%d\n",
            expected,
            status.state,
            status.fault
        );

        assert(status.state == expected);
    }
}

static void assert_fault(
    const BatteryMonitor *monitor,
    BatteryFault expected
)
{
    BatteryStatus status = battery_get_status(monitor);
    assert(status.fault == expected);
}

/* ---------------------------------------------------------
 * Test 1: Initialization
 * --------------------------------------------------------- */

static void test_initialization(void)
{
    BatteryMonitor monitor;
    reset_monitor(&monitor);

    assert_state(&monitor, BATTERY_NORMAL);
    assert_fault(&monitor, FAULT_NONE);
}

/* ---------------------------------------------------------
 * Test 2: Normal operation
 * --------------------------------------------------------- */

static void test_normal_operation(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_NORMAL);
    assert_fault(&monitor, FAULT_NONE);
}

/* ---------------------------------------------------------
 * Test 3: Undervoltage warning and hysteresis
 * --------------------------------------------------------- */

static void test_undervoltage_warning(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 11.4f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    measurements.voltage_v = 11.6f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    measurements.voltage_v = 11.8f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);
}

/* ---------------------------------------------------------
 * Test 4: Overvoltage warning and hysteresis
 * --------------------------------------------------------- */

static void test_overvoltage_warning(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 14.6f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    measurements.voltage_v = 14.4f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    measurements.voltage_v = 14.2f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);
}

/* ---------------------------------------------------------
 * Test 5: Overcurrent warning
 * --------------------------------------------------------- */

static void test_overcurrent_warning(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.current_a = 5.5f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    measurements.current_a = 3.5f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);
}

/* ---------------------------------------------------------
 * Test 6: Overtemperature warning
 * --------------------------------------------------------- */

static void test_overtemperature_warning(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.temperature_c = 50.0f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    measurements.temperature_c = 39.0f;
    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);
}

/* ---------------------------------------------------------
 * Test 7: Independent warning flags
 * --------------------------------------------------------- */

static void test_independent_warnings(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 11.4f;
    measurements.temperature_c = 50.0f;

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    /* Voltage recovers, temperature remains abnormal. */
    measurements.voltage_v = 12.5f;

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_WARNING);

    /* Both conditions recover. */
    measurements.temperature_c = 39.0f;

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);
}

/* ---------------------------------------------------------
 * Test 8: Critical undervoltage
 * --------------------------------------------------------- */

static void test_critical_undervoltage(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 10.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_UNDERVOLTAGE);
}

/* ---------------------------------------------------------
 * Test 9: Critical overvoltage
 * --------------------------------------------------------- */

static void test_critical_overvoltage(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 15.5f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_OVERVOLTAGE);
}

/* ---------------------------------------------------------
 * Test 10: Critical overcurrent
 * --------------------------------------------------------- */

static void test_critical_overcurrent(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.current_a = 11.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_OVERCURRENT);
}

/* ---------------------------------------------------------
 * Test 11: Negative current magnitude
 * --------------------------------------------------------- */

static void test_negative_overcurrent(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.current_a = -11.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_OVERCURRENT);
}

/* ---------------------------------------------------------
 * Test 12: Critical overtemperature
 * --------------------------------------------------------- */

static void test_critical_overtemperature(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.temperature_c = 65.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_OVERTEMPERATURE);
}

/* ---------------------------------------------------------
 * Test 13: Fault priority
 * --------------------------------------------------------- */

static void test_fault_priority(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 15.5f;
    measurements.current_a = 11.0f;
    measurements.temperature_c = 65.0f;

    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);

    /* Overtemperature has priority over current and voltage. */
    assert_fault(&monitor, FAULT_OVERTEMPERATURE);
}

/* ---------------------------------------------------------
 * Test 14: Partial sensor failure
 * --------------------------------------------------------- */

static void test_partial_sensor_failure(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 15.5f;
    measurements.valid_mask =
        SENSOR_VALID_VOLTAGE | SENSOR_VALID_CURRENT;

    battery_update(&monitor, &measurements);

    /* A valid critical voltage reading must not be ignored. */
    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_OVERVOLTAGE);
}

/* ---------------------------------------------------------
 * Test 15: Sensor failure tolerance
 * --------------------------------------------------------- */

static void test_sensor_failure_tolerance(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.valid_mask = SENSOR_VALID_VOLTAGE;

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_SENSOR);
}

/* ---------------------------------------------------------
 * Test 16: NaN sensor reading
 * --------------------------------------------------------- */

static void test_nan_sensor_reading(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.temperature_c = NAN;

    battery_update(&monitor, &measurements);
    battery_update(&monitor, &measurements);
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_SENSOR);
}

/* ---------------------------------------------------------
 * Test 17: Fault latching
 * --------------------------------------------------------- */

static void test_fault_latching(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 10.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);

    measurements = safe_measurement();

    battery_update(&monitor, &measurements);

    /* One safe sample cannot clear a latched fault. */
    assert_state(&monitor, BATTERY_FAULT);
    assert_fault(&monitor, FAULT_UNDERVOLTAGE);
}

/* ---------------------------------------------------------
 * Test 18: Consecutive safe-sample recovery
 * --------------------------------------------------------- */

static void test_fault_recovery(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.temperature_c = 65.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);

    measurements = safe_measurement();

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_FAULT);

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_FAULT);

    battery_update(&monitor, &measurements);
    assert_state(&monitor, BATTERY_NORMAL);
    assert_fault(&monitor, FAULT_NONE);
}

/* ---------------------------------------------------------
 * Test 19: Interrupted recovery
 * --------------------------------------------------------- */

static void test_interrupted_recovery(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.temperature_c = 65.0f;
    battery_update(&monitor, &measurements);

    measurements = safe_measurement();

    battery_update(&monitor, &measurements);
    battery_update(&monitor, &measurements);

    /* Unsafe for recovery: temperature above recovery limit. */
    measurements.temperature_c = 41.0f;
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);

    /* Counter must restart from zero. */
    measurements.temperature_c = 25.0f;

    battery_update(&monitor, &measurements);
    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_FAULT);

    battery_update(&monitor, &measurements);

    assert_state(&monitor, BATTERY_NORMAL);
}

/* ---------------------------------------------------------
 * Test 20: Threshold boundaries
 * --------------------------------------------------------- */

static void test_threshold_boundaries(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = safe_measurement();

    reset_monitor(&monitor);

    measurements.voltage_v = 11.5f;
    measurements.current_a = 5.0f;
    measurements.temperature_c = 45.0f;

    battery_update(&monitor, &measurements);

    /* Warning activation uses strict comparisons. */
    assert_state(&monitor, BATTERY_NORMAL);

    measurements.voltage_v = 10.5f;
    measurements.current_a = 10.0f;
    measurements.temperature_c = 60.0f;

    battery_update(&monitor, &measurements);

    /* No critical threshold was crossed. */
    assert_state(&monitor, BATTERY_WARNING);
    assert_fault(&monitor, FAULT_NONE);
}

/* ---------------------------------------------------------
 * Test runner
 * --------------------------------------------------------- */

int main(void)
{
    test_initialization();
    test_normal_operation();

    test_undervoltage_warning();
    test_overvoltage_warning();
    test_overcurrent_warning();
    test_overtemperature_warning();
    test_independent_warnings();

    test_critical_undervoltage();
    test_critical_overvoltage();
    test_critical_overcurrent();
    test_negative_overcurrent();
    test_critical_overtemperature();

    test_fault_priority();
    test_partial_sensor_failure();
    test_sensor_failure_tolerance();
    test_nan_sensor_reading();

    test_fault_latching();
    test_fault_recovery();
    test_interrupted_recovery();

    test_threshold_boundaries();

    printf("BatteryGuard: all 20 tests passed.\n");

    return 0;
}
