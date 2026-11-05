#include "protocol.h"

#include <math.h>
#include <string.h>

/* Convert a float into a rounded, bounded integer. */
static int convert_scaled(
    float value,
    double scale,
    int32_t minimum,
    int32_t maximum,
    int32_t *result
)
{
    double scaled;
    int32_t rounded;

    if ((result == NULL) || !isfinite(value))
    {
        return 0;
    }

    scaled = (double)value * scale;

    /* Reject values that cannot round into the allowed range. */
    if ((scaled < ((double)minimum - 0.5)) ||
        (scaled > ((double)maximum + 0.5)))
    {
        return 0;
    }

    rounded = (int32_t)scaled;

    /* Round to nearest integer, including negative values. */
    if (scaled >= 0.0)
    {
        if ((scaled - (double)rounded) >= 0.5)
        {
            rounded++;
        }
    }
    else
    {
        if (((double)rounded - scaled) >= 0.5)
        {
            rounded--;
        }
    }

    if ((rounded < minimum) || (rounded > maximum))
    {
        return 0;
    }

    *result = rounded;
    return 1;
}

uint8_t protocol_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0x00U;

    if ((data == NULL) && (length > 0U))
    {
        return 0x00U;
    }

    for (size_t i = 0U; i < length; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1U) ^ 0x07U);
            }
            else
            {
                crc = (uint8_t)(crc << 1U);
            }
        }
    }

    return crc;
}

size_t protocol_encode(
    const BatteryStatus *status,
    uint8_t sequence,
    uint8_t *buffer,
    size_t buffer_size
)
{
    uint8_t frame[PROTOCOL_FRAME_SIZE];
    int32_t voltage_mv = 0;
    int32_t current_ma = 0;
    int32_t temperature_centi = 0;
    uint8_t valid_mask;

    if ((status == NULL) ||
        (buffer == NULL) ||
        (buffer_size < PROTOCOL_FRAME_SIZE))
    {
        return 0U;
    }

    /* Validate state values. */
    if ((status->state != BATTERY_NORMAL) &&
        (status->state != BATTERY_WARNING) &&
        (status->state != BATTERY_FAULT))
    {
        return 0U;
    }

    /* Validate fault values. */
    if ((status->fault < FAULT_NONE) ||
        (status->fault > FAULT_SENSOR))
    {
        return 0U;
    }

    valid_mask = status->measurements.valid_mask;

    /* Reject unknown validity bits. */
    if ((valid_mask & (uint8_t)(~SENSOR_VALID_ALL)) != 0U)
    {
        return 0U;
    }

    /*
     * Convert only measurements marked valid.
     * Invalid measurements remain zero placeholders.
     */

    if ((valid_mask & SENSOR_VALID_VOLTAGE) != 0U)
    {
        if (!convert_scaled(
                status->measurements.voltage_v,
                1000.0,
                0,
                65535,
                &voltage_mv))
        {
            return 0U;
        }
    }

    if ((valid_mask & SENSOR_VALID_CURRENT) != 0U)
    {
        if (!convert_scaled(
                status->measurements.current_a,
                1000.0,
                -32768,
                32767,
                &current_ma))
        {
            return 0U;
        }
    }

    if ((valid_mask & SENSOR_VALID_TEMPERATURE) != 0U)
    {
        if (!convert_scaled(
                status->measurements.temperature_c,
                100.0,
                -32768,
                32767,
                &temperature_centi))
        {
            return 0U;
        }
    }

    /* Frame header. */
    frame[0] = PROTOCOL_SOF_1;
    frame[1] = PROTOCOL_SOF_2;
    frame[2] = PROTOCOL_VERSION;
    frame[3] = sequence;

    /* Protection state and fault reason. */
    frame[4] = (uint8_t)status->state;
    frame[5] = (uint8_t)status->fault;

    /* Voltage: unsigned 16-bit, big-endian. */
    frame[6] = (uint8_t)((uint16_t)voltage_mv >> 8U);
    frame[7] = (uint8_t)voltage_mv;

    /* Current: signed 16-bit, big-endian. */
    frame[8] = (uint8_t)((uint16_t)current_ma >> 8U);
    frame[9] = (uint8_t)current_ma;

    /* Temperature: signed 16-bit, big-endian. */
    frame[10] = (uint8_t)((uint16_t)temperature_centi >> 8U);
    frame[11] = (uint8_t)temperature_centi;

    /* Sensor validity flags. */
    frame[12] = valid_mask;

    /* CRC covers bytes 0 through 12. */
    frame[13] = protocol_crc8(frame, 13U);

    /* Commit the complete frame only after successful encoding. */
    memcpy(buffer, frame, PROTOCOL_FRAME_SIZE);

    return PROTOCOL_FRAME_SIZE;
}