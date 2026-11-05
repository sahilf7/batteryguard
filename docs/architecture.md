# BatteryGuard Architecture

## 1. Overview

BatteryGuard separates sensor acquisition from battery
protection logic.

The protection engine is written in platform-independent C
and is shared by the host simulator and ESP32 firmware.

## 2. Architecture

Host simulation:

    sensor_sim.c
        |
        v
    BatteryMeasurements
        |
        v
    battery.c
        |
        v
    BatteryStatus
        |
        v
    main.c (terminal output)

ESP32 firmware:

    Hardware sensor adapter
        |
        v
    BatteryMeasurements
        |
        v
    battery.c
        |
        v
    BatteryStatus
        |
        v
    UART telemetry

Both environments use the same battery protection engine.

## 3. Core Modules

### sensor.h

Defines BatteryMeasurements and sensor validity flags.

### sensor_sim.c

Provides deterministic measurements for host simulation.

### battery.h

Defines the protection states, fault types, configuration,
status structures and public protection API.

### battery.c

Implements sensor validation, warning detection,
critical fault detection, fault latching and recovery.

### main.c

Initializes the monitor, reads simulated measurements,
updates the protection engine and displays the resulting
status.

## 4. Protection States

- BATTERY_NORMAL: Measurements are within warning limits.
- BATTERY_WARNING: One or more warning conditions exist.
- BATTERY_FAULT: A critical fault has been detected or
  sensor failures have exceeded the configured tolerance.

## 5. Fault Handling

Critical fault detection takes priority over warning
evaluation.

Faults remain latched until the configured number of
consecutive safe measurements is reached.

Recovery requires valid sensor measurements and values
within the configured recovery limits.

## 6. Platform Independence

The protection engine does not depend on Arduino APIs,
ESP32 registers, ADC drivers or operating-system services.

Platform-specific code is responsible for acquiring
measurements and passing them to the shared engine.

## 7. Current Scope

The host simulation and protection engine are implemented.

UART telemetry, CRC-8 framing, ESP32 integration and
Wokwi demonstration are planned for subsequent phases.

No physical battery or protection hardware has been tested.
