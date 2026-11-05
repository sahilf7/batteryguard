#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#include "battery.h"

#define PROTOCOL_SOF_1       0xAAU
#define PROTOCOL_SOF_2       0x55U

#define PROTOCOL_VERSION     0x01U

#define PROTOCOL_FRAME_SIZE  14U

#ifdef __cplusplus
extern "C" {
#endif

uint8_t protocol_crc8(
    const uint8_t *data,
    size_t length
);

size_t protocol_encode(
    const BatteryStatus *status,
    uint8_t sequence,
    uint8_t *buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif