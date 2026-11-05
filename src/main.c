
#include <stdio.h>

#include "battery.h"
#include "sensor.h"
#include "protocol.h"

/* Demonstration configuration */
static const BatteryConfig battery_config =
{
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

/* Convert FSM state to readable text */
static const char *battery_state_name(BatteryState state)
{
    switch (state)
    {
        case BATTERY_NORMAL:
            return "NORMAL";

        case BATTERY_WARNING:
            return "WARNING";

        case BATTERY_FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}

/* Convert fault reason to readable text */
static const char *battery_fault_name(BatteryFault fault)
{
    switch (fault)
    {
        case FAULT_NONE:
            return "NONE";

        case FAULT_OVERVOLTAGE:
            return "OVERVOLTAGE";

        case FAULT_UNDERVOLTAGE:
            return "UNDERVOLTAGE";

        case FAULT_OVERCURRENT:
            return "OVERCURRENT";

        case FAULT_OVERTEMPERATURE:
            return "OVERTEMPERATURE";

        case FAULT_SENSOR:
            return "SENSOR";

        default:
            return "UNKNOWN";
    }
}

int main(void)
{
    BatteryMonitor monitor;
    BatteryMeasurements measurements = {0};

    battery_init(&monitor, &battery_config);

    printf("============================================\n");
    printf("          BATTERYGUARD HOST SIM             \n");
    printf("============================================\n");

    for (unsigned int cycle = 1U; cycle <= 10U; cycle++)
    {
        sensor_read(&measurements);

        battery_update(&monitor, &measurements);

        BatteryStatus status = battery_get_status(&monitor);

        uint8_t frame[PROTOCOL_FRAME_SIZE];

        size_t frame_length = protocol_encode(
        &status,
        (uint8_t)cycle,
        frame,
        sizeof(frame)
    );

        printf("\nCycle %u\n", cycle);

        printf(
            "Voltage: %.2f V | Current: %.2f A | "
            "Temperature: %.1f C\n",
            measurements.voltage_v,
            measurements.current_a,
            measurements.temperature_c
        );

        printf(
            "Sensor validity: %s (0x%02X)\n",
            measurements.valid_mask == SENSOR_VALID_ALL
                ? "VALID"
                : "PARTIAL/INVALID",
            (unsigned int)measurements.valid_mask
        );

        printf(
            "Protection state: %-7s | Fault: %s\n",
            battery_state_name(status.state),
            battery_fault_name(status.fault)
        );

        if (frame_length == PROTOCOL_FRAME_SIZE)
        {
        printf("Telemetry frame: ");

        for (size_t i = 0U; i < frame_length; i++)
        {
        printf("%02X ", (unsigned int)frame[i]);
        }

        printf("\n");
        }
        else
        {
        printf("Telemetry encoding failed.\n");
        }
    }

    printf("\n============================================\n");
    printf("          SIMULATION COMPLETE               \n");
    printf("============================================\n");

    return 0;
}
