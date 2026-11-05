#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "protocol.h"

static BatteryStatus make_valid_status(void)
{
    BatteryStatus status = {0};

    status.state = BATTERY_NORMAL;
    status.fault = FAULT_NONE;

    status.measurements.voltage_v = 12.50f;
    status.measurements.current_a = -1.25f;
    status.measurements.temperature_c = 25.50f;
    status.measurements.valid_mask = SENSOR_VALID_ALL;

    return status;
}

static void test_crc_known_vector(void)
{
    const uint8_t data[] = "123456789";

    assert(protocol_crc8(data, 9U) == 0xF4U);
}

static void test_crc_empty_input(void)
{
    assert(protocol_crc8(NULL, 0U) == 0x00U);
}

static void test_crc_single_byte(void)
{
    const uint8_t data[] = {0x01U};

    assert(protocol_crc8(data, 1U) == 0x07U);
}

static void test_crc_null_pointer(void)
{
    assert(protocol_crc8(NULL, 1U) == 0x00U);
}

static void test_encode_exact_frame(void)
{
    BatteryStatus status = make_valid_status();
    uint8_t buffer[PROTOCOL_FRAME_SIZE] = {0};

    const uint8_t expected[PROTOCOL_FRAME_SIZE] =
    {
        0xAAU, 0x55U, 0x01U, 0x01U,
        0x00U, 0x00U,
        0x30U, 0xD4U,
        0xFBU, 0x1EU,
        0x09U, 0xF6U,
        0x07U, 0xA9U
    };

    size_t length = protocol_encode(
        &status,
        1U,
        buffer,
        sizeof(buffer)
    );

    assert(length == PROTOCOL_FRAME_SIZE);
    assert(memcmp(buffer, expected, sizeof(expected)) == 0);
}

static void test_encode_partial_sensor_validity(void)
{
    BatteryStatus status = make_valid_status();
    uint8_t buffer[PROTOCOL_FRAME_SIZE] = {0};

    status.measurements.valid_mask =
        SENSOR_VALID_VOLTAGE | SENSOR_VALID_TEMPERATURE;

    /* Invalid current is deliberately non-finite. */
    status.measurements.current_a = NAN;

    size_t length = protocol_encode(
        &status,
        2U,
        buffer,
        sizeof(buffer)
    );

    assert(length == PROTOCOL_FRAME_SIZE);

    /* Invalid current must be encoded as zero. */
    assert(buffer[8] == 0x00U);
    assert(buffer[9] == 0x00U);

    /* Validity mask must be preserved. */
    assert(buffer[12] ==
           (SENSOR_VALID_VOLTAGE | SENSOR_VALID_TEMPERATURE));

    assert(buffer[13] == protocol_crc8(buffer, 13U));
}

static void test_encode_rejects_invalid_measurement(void)
{
    BatteryStatus status = make_valid_status();
    uint8_t buffer[PROTOCOL_FRAME_SIZE];

    memset(buffer, 0xA5, sizeof(buffer));

    status.measurements.voltage_v = INFINITY;

    size_t length = protocol_encode(
        &status,
        3U,
        buffer,
        sizeof(buffer)
    );

    assert(length == 0U);

    /* Failed encoding must not modify the caller's buffer. */
    for (size_t i = 0U; i < sizeof(buffer); i++)
    {
        assert(buffer[i] == 0xA5U);
    }
}

static void test_encode_rejects_short_buffer(void)
{
    BatteryStatus status = make_valid_status();
    uint8_t buffer[PROTOCOL_FRAME_SIZE] = {0};

    size_t length = protocol_encode(
        &status,
        4U,
        buffer,
        PROTOCOL_FRAME_SIZE - 1U
    );

    assert(length == 0U);
}

static void test_encode_rejects_null_arguments(void)
{
    BatteryStatus status = make_valid_status();
    uint8_t buffer[PROTOCOL_FRAME_SIZE] = {0};

    assert(protocol_encode(
        NULL, 0U, buffer, sizeof(buffer)
    ) == 0U);

    assert(protocol_encode(
        &status, 0U, NULL, PROTOCOL_FRAME_SIZE
    ) == 0U);
}

static void test_encode_rejects_unknown_validity_bits(void)
{
    BatteryStatus status = make_valid_status();
    uint8_t buffer[PROTOCOL_FRAME_SIZE] = {0};

    status.measurements.valid_mask = 0x80U;

    assert(protocol_encode(
        &status, 5U, buffer, sizeof(buffer)
    ) == 0U);
}

int main(void)
{
    test_crc_known_vector();
    test_crc_empty_input();
    test_crc_single_byte();
    test_crc_null_pointer();

    test_encode_exact_frame();
    test_encode_partial_sensor_validity();
    test_encode_rejects_invalid_measurement();
    test_encode_rejects_short_buffer();
    test_encode_rejects_null_arguments();
    test_encode_rejects_unknown_validity_bits();

    printf("BatteryGuard: all 10 protocol tests passed.\n");

    return 0;
}