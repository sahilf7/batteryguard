# BatteryGuard UART Telemetry Protocol

## 1. Overview

BatteryGuard uses a fixed-length binary telemetry protocol to transmit battery measurements and protection status.

The protocol is designed for UART communication between the ESP32 firmware and a monitoring interface.

The frame contains:

- Start-of-frame markers.
- Protocol version.
- Sequence number.
- Protection state.
- Fault code.
- Voltage measurement.
- Current measurement.
- Temperature measurement.
- Sensor validity mask.
- CRC-8 checksum.

### Protocol properties

| Property | Value |
|---|---|
| Frame size | 14 bytes |
| Protocol version | `0x01` |
| Start markers | `0xAA 0x55` |
| Byte order | Big-endian |
| Checksum | CRC-8/ATM |
| UART baud rate | 115200 |

## 2. Frame Structure

Each telemetry frame contains exactly 14 bytes.

| Byte offset | Field | Size | Description |
|---:|---|---:|---|
| 0 | SOF 1 | 1 byte | First start marker: `0xAA` |
| 1 | SOF 2 | 1 byte | Second start marker: `0x55` |
| 2 | Version | 1 byte | Protocol version: `0x01` |
| 3 | Sequence | 1 byte | Frame sequence number |
| 4 | State | 1 byte | Protection state |
| 5 | Fault | 1 byte | Active fault reason |
| 6–7 | Voltage | 2 bytes | Unsigned voltage in millivolts |
| 8–9 | Current | 2 bytes | Signed current in milliamps |
| 10–11 | Temperature | 2 bytes | Signed temperature in centidegrees Celsius |
| 12 | Validity mask | 1 byte | Indicates valid measurements |
| 13 | CRC | 1 byte | CRC-8/ATM checksum |

Total frame size:

```text
2 + 1 + 1 + 1 + 1 + 2 + 2 + 2 + 1 + 1 = 14 bytes
```

## 3. State and Fault Encoding

### 3.1 Protection state

The state field uses the following values:

| Value | State |
|---:|---|
| `0x00` | NORMAL |
| `0x01` | WARNING |
| `0x02` | FAULT |

These values correspond to the `BatteryState` enumeration.

### 3.2 Fault code

The fault field identifies the active critical fault.

| Value | Fault |
|---:|---|
| `0x00` | FAULT_NONE |
| `0x01` | FAULT_OVERVOLTAGE |
| `0x02` | FAULT_UNDERVOLTAGE |
| `0x03` | FAULT_OVERCURRENT |
| `0x04` | FAULT_OVERTEMPERATURE |
| `0x05` | FAULT_SENSOR |

A warning condition does not necessarily have a critical fault code. In that case, the state can be WARNING while the fault field remains `0x00`.

## 4. Measurement Encoding

Measurements are converted from floating-point engineering values into fixed-width integer representations.

| Measurement | Encoding | Data type |
|---|---|---|
| Voltage | Millivolts (mV) | Unsigned 16-bit |
| Current | Milliamps (mA) | Signed 16-bit |
| Temperature | Centidegrees Celsius (0.01 °C) | Signed 16-bit |

Multi-byte values use **big-endian byte order**, meaning the most significant byte is transmitted first.

### 4.1 Voltage

Voltage is multiplied by 1000.

Example:

```text
Voltage = 12.50 V

12.50 × 1000 = 12500

12500 decimal = 0x30D4

Encoded bytes:
30 D4
```

### 4.2 Current

Current is multiplied by 1000 and represented as a signed 16-bit integer.

Signed values use two's-complement representation.

Example:

```text
Current = -1.25 A

-1.25 × 1000 = -1250

-1250 as signed 16-bit = 0xFB1E

Encoded bytes:
FB 1E
```

### 4.3 Temperature

Temperature is multiplied by 100.

Example:

```text
Temperature = 25.50 °C

25.50 × 100 = 2550

2550 decimal = 0x09F6

Encoded bytes:
09 F6
```

## 5. Sensor Validity Mask

The validity mask indicates whether each measurement can be trusted.

| Bit | Mask | Meaning |
|---|---|---|
| Bit 0 | `0x01` | Voltage valid |
| Bit 1 | `0x02` | Current valid |
| Bit 2 | `0x04` | Temperature valid |

Examples:

| Validity mask | Meaning |
|---|---|
| `0x07` | All three measurements valid |
| `0x05` | Voltage and temperature valid |
| `0x03` | Voltage and current valid |
| `0x00` | All measurements invalid |

If a measurement is invalid, its encoded numeric field is set to zero.

The validity mask is authoritative. A zero measurement field must not automatically be interpreted as a genuine zero-valued measurement.

If a measurement is marked valid but contains a non-finite or unrepresentable value, the encoder reports failure rather than generating a misleading frame.

## 6. CRC-8/ATM

CRC stands for **Cyclic Redundancy Check**.

It is an error-detection mechanism used to detect many forms of data corruption during transmission.

BatteryGuard uses CRC-8/ATM.

### 6.1 CRC parameters

| Parameter | Value |
|---|---|
| Width | 8 bits |
| Polynomial | `0x07` |
| Initial value | `0x00` |
| Input reflection | False |
| Output reflection | False |
| Final XOR | `0x00` |

### 6.2 CRC coverage

The CRC is calculated over bytes 0 through 12.

The resulting checksum is stored in byte 13.

```text
Bytes 0–12
    |
    v
 CRC-8/ATM
    |
    v
 Byte 13
```

The checksum byte itself is not included in the CRC calculation.

### 6.3 Standard check value

The standard CRC-8/ATM check vector is:

```text
Input:
123456789

Expected CRC:
0xF4
```

This value is used to verify the CRC implementation.

## 7. Complete Example Frame

Consider the following measurement and status values:

| Parameter | Value |
|---|---|
| Sequence | `0x01` |
| State | NORMAL |
| Fault | NONE |
| Voltage | 12.50 V |
| Current | -1.25 A |
| Temperature | 25.50 °C |
| Validity mask | `0x07` |

The encoded frame is:

```text
AA 55 01 01 00 00 30 D4 FB 1E 09 F6 07 A9
```

Byte interpretation:

| Offset | Hex | Meaning |
|---:|---:|---|
| 0 | `AA` | First start marker |
| 1 | `55` | Second start marker |
| 2 | `01` | Protocol version |
| 3 | `01` | Sequence number |
| 4 | `00` | NORMAL state |
| 5 | `00` | No active fault |
| 6 | `30` | Voltage high byte |
| 7 | `D4` | Voltage low byte |
| 8 | `FB` | Current high byte |
| 9 | `1E` | Current low byte |
| 10 | `09` | Temperature high byte |
| 11 | `F6` | Temperature low byte |
| 12 | `07` | All measurements valid |
| 13 | `A9` | CRC-8/ATM |

The final byte `0xA9` is calculated over the first 13 bytes.

## 8. Protocol Implementation

Files:

- `include/protocol.h`
- `src/protocol.c`

The protocol module exposes two functions.

### 8.1 CRC function

```c
uint8_t protocol_crc8(
    const uint8_t *data,
    size_t length
);
```

Calculates the CRC-8/ATM checksum for the supplied byte sequence.

### 8.2 Frame encoder

```c
size_t protocol_encode(
    const BatteryStatus *status,
    uint8_t sequence,
    uint8_t *buffer,
    size_t buffer_size
);
```

Converts a `BatteryStatus` structure into the 14-byte telemetry frame.

Return behavior:

| Return value | Meaning |
|---|---|
| `14` | Frame encoded successfully |
| `0` | Encoding failed |

Encoding fails when required pointers, buffer capacity or measurement representation are invalid.

The encoder does not produce a partial frame on failure.

## 9. Design Decisions

| Decision | Reason |
|---|---|
| Fixed 14-byte frame | Predictable frame size and parsing boundaries |
| Start markers | Identify the beginning of a frame |
| Version field | Allows future protocol revisions |
| Sequence number | Distinguishes successive frames |
| Integer measurement encoding | Avoids transmitting platform-dependent floating-point representations |
| Big-endian byte order | Defines an explicit multi-byte representation |
| Validity mask | Separates sensor validity from numeric measurement values |
| CRC-8/ATM | Detects many transmission errors |

## 10. Implementation Scope and Limitations

The current implementation provides:

- CRC-8/ATM calculation.
- Fixed-length telemetry frame encoding.
- Measurement scaling and integer representation.
- Sensor validity encoding.
- Buffer and measurement validation.

It does not currently implement:

- A UART receiver.
- A complete frame parser or decoder.
- Frame resynchronization.
- Retransmission.
- Acknowledgement or handshake mechanisms.

The protocol is therefore an implemented telemetry encoder, not a complete bidirectional communication stack.