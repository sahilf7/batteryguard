#include <Arduino.h>

#include "battery.h"
#include "protocol.h"

/* ---------------------------------------------------------
 * ESP32 pin allocation
 * --------------------------------------------------------- */

static const uint8_t VOLTAGE_PIN = 34U;
static const uint8_t CURRENT_PIN = 35U;
static const uint8_t TEMPERATURE_PIN = 32U;
static const uint8_t SENSOR_FAULT_PIN = 25U;

/* ADC configuration */
static const int ADC_MAX_VALUE = 4095;

/* Simulated sensor ranges */
static const float VOLTAGE_MAX_V = 18.0f;
static const float CURRENT_MAX_A = 12.0f;
static const float TEMPERATURE_MAX_C = 80.0f;

/* ---------------------------------------------------------
 * Shared battery protection configuration
 * Keep these values consistent with the host simulation.
 * --------------------------------------------------------- */

static const BatteryConfig battery_config =
{
    11.5f,  /* voltage warning minimum */
    14.5f,  /* voltage warning maximum */
    5.0f,   /* current warning */
    45.0f,  /* temperature warning */

    10.5f,  /* critical voltage minimum */
    15.0f,  /* critical voltage maximum */
    10.0f,  /* critical current */
    60.0f,  /* critical temperature */

    11.8f,  /* voltage recovery minimum */
    14.2f,  /* voltage recovery maximum */
    4.0f,   /* current recovery */
    40.0f,  /* temperature recovery */

    3U,     /* consecutive safe samples required */
    3U      /* invalid samples before sensor fault */
};

/* Shared C protection engine instance */
static BatteryMonitor battery_monitor;

/* Telemetry sequence number */
static uint8_t sequence_number = 0U;

/* ---------------------------------------------------------
 * ADC conversion
 * Convert raw ADC readings into simulated sensor values.
 * --------------------------------------------------------- */

static float adc_to_range(int raw_value, float maximum)
{
    if (raw_value < 0)
    {
        raw_value = 0;
    }

    if (raw_value > ADC_MAX_VALUE)
    {
        raw_value = ADC_MAX_VALUE;
    }

    return ((float)raw_value / (float)ADC_MAX_VALUE) * maximum;
}

/* ---------------------------------------------------------
 * Read simulated sensors
 * --------------------------------------------------------- */

static void read_measurements(BatteryMeasurements *measurements)
{
    const int voltage_raw = analogRead(VOLTAGE_PIN);
    const int current_raw = analogRead(CURRENT_PIN);
    const int temperature_raw = analogRead(TEMPERATURE_PIN);

    measurements->voltage_v =
        adc_to_range(voltage_raw, VOLTAGE_MAX_V);

    measurements->current_a =
        adc_to_range(current_raw, CURRENT_MAX_A);

    measurements->temperature_c =
        adc_to_range(temperature_raw, TEMPERATURE_MAX_C);

    measurements->valid_mask = SENSOR_VALID_ALL;

    /*
     * Button uses INPUT_PULLUP.
     * Pressed = LOW.
     * Simulate all sensors becoming invalid.
     */
    if (digitalRead(SENSOR_FAULT_PIN) == LOW)
    {
        measurements->valid_mask = 0U;
    }
}

/* ---------------------------------------------------------
 * Human-readable status names
 * --------------------------------------------------------- */

static const char *state_name(BatteryState state)
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

static const char *fault_name(BatteryFault fault)
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

/* ---------------------------------------------------------
 * UART telemetry output
 * --------------------------------------------------------- */

static void print_telemetry(const BatteryStatus *status)
{
    uint8_t frame[PROTOCOL_FRAME_SIZE];

    const size_t frame_length = protocol_encode(
        status,
        sequence_number,
        frame,
        sizeof(frame)
    );

    if (frame_length == 0U)
    {
        Serial.println("Telemetry encoding failed.");
        return;
    }

    Serial.print("Telemetry: ");

    for (size_t i = 0U; i < frame_length; i++)
    {
        if (frame[i] < 0x10U)
        {
            Serial.print('0');
        }

        Serial.print(frame[i], HEX);
        Serial.print(' ');
    }

    Serial.println();

    sequence_number++;
}

/* ---------------------------------------------------------
 * Arduino initialization
 * Called once after ESP32 startup.
 * --------------------------------------------------------- */

void setup()
{
    Serial.begin(115200);

    analogReadResolution(12);

    pinMode(SENSOR_FAULT_PIN, INPUT_PULLUP);

    battery_init(&battery_monitor, &battery_config);

    Serial.println();
    Serial.println("================================");
    Serial.println("       BATTERYGUARD ESP32       ");
    Serial.println("================================");
    Serial.println("ADC resolution: 12-bit");
    Serial.println("UART baud rate: 115200");
    Serial.println("Protection engine: Shared C core");
    Serial.println();
}

/* ---------------------------------------------------------
 * Main application loop
 * Repeatedly acquire, evaluate and transmit.
 * --------------------------------------------------------- */

void loop()
{
    BatteryMeasurements measurements = {};

    read_measurements(&measurements);

    /* Execute the shared protection state machine. */
    battery_update(&battery_monitor, &measurements);

    /* Retrieve the latest protection status. */
    const BatteryStatus status =
        battery_get_status(&battery_monitor);

    Serial.println("--------------------------------");

    Serial.print("Voltage: ");
    Serial.print(measurements.voltage_v, 2);
    Serial.println(" V");

    Serial.print("Current: ");
    Serial.print(measurements.current_a, 2);
    Serial.println(" A");

    Serial.print("Temperature: ");
    Serial.print(measurements.temperature_c, 2);
    Serial.println(" C");

    Serial.print("Validity mask: 0x");
    Serial.println(measurements.valid_mask, HEX);

    Serial.print("State: ");
    Serial.println(state_name(status.state));

    Serial.print("Fault: ");
    Serial.println(fault_name(status.fault));

    print_telemetry(&status);

    delay(500);
}
