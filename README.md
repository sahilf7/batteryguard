# BatteryGuard

Embedded Battery Monitoring and Protection System.

BatteryGuard is an educational embedded C prototype
that monitors battery voltage, current and temperature
and detects abnormal operating conditions.

## Features

- Simulated voltage, current and temperature measurements
- Sensor validity checking
- Three-state protection finite-state machine
- Warning thresholds with hysteresis
- Critical fault detection and prioritization
- Fault latching and recovery validation
- Automated host-side unit tests

## Technology

- C11
- GCC
- CMake
- ESP32
- PlatformIO
- Wokwi

## Architecture

The battery protection engine is platform-independent.

The host simulator and ESP32 firmware are designed to
use the same protection implementation.

Hardware-specific sensor acquisition is kept separate
from the shared protection logic.

## Verification

- 20 automated protection tests
- Host simulation with deterministic sensor scenarios
- CTest integration
- Strict compiler warnings enabled

## Project Status

- Phase 1: Project setup and core foundation - Complete
- Phase 2: Battery protection engine and host integration - Complete
- Phase 3: UART telemetry and CRC-8 protocol - Pending
- Phase 4: ESP32 firmware and Wokwi demonstration - Pending

## Limitations

This is a software simulation prototype. It has not
been tested with a physical battery or protection circuit.