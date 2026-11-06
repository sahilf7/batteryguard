# BatteryGuard Architecture

## 1. Overview

BatteryGuard is an embedded battery monitoring and protection prototype implemented in C.

It monitors three simulated battery parameters:

- Voltage
- Current
- Temperature

The system validates measurement data, detects abnormal operating conditions, manages protection states and generates binary telemetry frames.

The project supports two execution environments:

1. **Host simulation:** Runs on Linux using GCC and CMake.
2. **ESP32 simulation:** Runs on an ESP32 DevKit in Wokwi using PlatformIO.

The central architectural principle is that both environments use the same platform-independent protection and communication code.

## 2. Architectural Design

BatteryGuard separates sensor acquisition, protection logic, telemetry generation and hardware-specific output control.

```text
                         BATTERYGUARD
                              |
                  +-----------+-----------+
                  |                       |
                  v                       v
             HOST SIMULATION        ESP32 SIMULATION
                  |                       |
                  v                       v
             sensor_sim.c           ESP32 ADC Inputs
                  |                  GPIO 34, 35, 32
                  |                       |
                  |                  Fault Button
                  |                    GPIO 25
                  |                       |
                  |                  Status LEDs
                  |              GPIO 26, 27, 14, 13
                  |                       |
                  v                       v
                main.c                main.cpp
                  |                       |
                  +-----------+-----------+
                              |
                              v
                     BatteryMeasurements
                              |
                              v
                         battery.c
                     Protection Engine
                              |
                              v
                        BatteryStatus
                              |
                   +----------+----------+
                   |                     |
                   v                     v
               protocol.c          ESP32 LED Control
             Telemetry + CRC-8           |
                   |              +------+------+------+
                   |              |      |      |      |
                   v              v      v      v      v
              Host Output       Green  Yellow  Red    Blue
                   |            NORMAL WARNING FAULT SENSOR
                   |
                   |             ESP32 UART
                   |                  |
                   |                  v
                   |          Wokwi Serial Monitor
                   |
                   v
              Host Output
```

The host and ESP32 applications provide measurements through different acquisition mechanisms.

Both pass the same `BatteryMeasurements` structure to the shared protection engine.

The protection algorithm is implemented only once.

The ESP32 firmware also uses the resulting `BatteryStatus` to control four status LEDs. This output handling remains separate from the shared protection engine.

## 3. Software Modules

### 3.1 Sensor Interface

File: `include/sensor.h`

Defines the common measurement structure and sensor validity flags.

The `BatteryMeasurements` structure contains:

- `voltage_v`: Voltage in volts.
- `current_a`: Current in amperes.
- `temperature_c`: Temperature in degrees Celsius.
- `valid_mask`: Bitmask indicating measurement validity.

Each measurement has an independent validity bit:

| Bit | Measurement |
|---|---|
| Bit 0 | Voltage |
| Bit 1 | Current |
| Bit 2 | Temperature |

A validity mask of `0x07` indicates that all three measurements are valid.

This interface allows different acquisition implementations to provide data in a common format.

### 3.2 Host Sensor Simulator

File: `src/sensor_sim.c`

Generates deterministic measurement scenarios for host-side execution.

It provides simulated sensor values to `src/main.c` through the sensor interface.

The simulator allows the protection engine to be tested without physical sensors.

### 3.3 ESP32 Firmware Adapter

File: `firmware/esp32/main.cpp`

Implements the ESP32-specific application layer.

It performs:

- ADC acquisition from three potentiometers.
- Conversion of ADC readings into simulated engineering values.
- Sensor fault injection through a pushbutton.
- Construction of the `BatteryMeasurements` structure.
- Calls to the shared protection engine.
- UART telemetry output.
- Status LED control based on the current `BatteryStatus`.

**Status LED indicators**

The ESP32 firmware provides visual indication of the current protection state using four LEDs.

| LED | GPIO | Activation condition |
|---|---:|---|
| Green | 26 | `BATTERY_NORMAL` |
| Yellow | 27 | `BATTERY_WARNING` |
| Red | 14 | `BATTERY_FAULT` with a non-sensor fault |
| Blue | 13 | `BATTERY_FAULT` with `FAULT_SENSOR` |

The firmware retrieves the current `BatteryStatus` from the protection engine and updates the LEDs accordingly.

Before updating the indicators, the firmware turns all four LEDs off. It then activates the LED corresponding to the current protection state and fault reason.

LED control is implemented exclusively in the ESP32 application layer. It does not modify the protection algorithm or affect fault detection and recovery.

The potentiometers and pushbutton are simulated inputs in Wokwi, not physical battery sensors.

The LEDs provide visual feedback only. They do not perform battery disconnection or electrical protection.

### 3.4 Battery Protection Engine

Files:

- `include/battery.h`
- `src/battery.c`

This is the central platform-independent module.

It receives measurements and determines the protection status.

Its responsibilities include:

- Sensor validity checking.
- Non-finite measurement rejection.
- Critical fault detection.
- Warning condition evaluation.
- Warning hysteresis.
- Fault prioritization.
- Fault latching.
- Consecutive-safe-sample recovery.

The engine does not directly access ESP32 registers, Arduino APIs, ADC drivers or Linux-specific functions.

**Protection states**

| State | Meaning |
|---|---|
| `BATTERY_NORMAL` | No active warning or critical fault |
| `BATTERY_WARNING` | One or more warning conditions are active |
| `BATTERY_FAULT` | A critical fault has been detected |

The fault enumeration identifies the reason for a critical fault.

A warning state can exist while the fault reason remains `FAULT_NONE`.

**Critical fault conditions**

| Fault | Condition |
|---|---|
| Overvoltage | Voltage above the configured maximum |
| Undervoltage | Voltage below the configured minimum |
| Overcurrent | Absolute current above the configured maximum |
| Overtemperature | Temperature above the configured maximum |
| Sensor fault | Invalid measurements persist for the configured number of samples |

When multiple critical conditions occur simultaneously, the engine selects a fault reason using deterministic priority.

**Fault recovery**

Critical faults remain latched until the recovery conditions are satisfied.

The current configuration requires three consecutive safe measurement samples.

An unsafe or invalid sample interrupts the recovery sequence.

Warning conditions use hysteresis to avoid rapid switching around warning thresholds.

### 3.5 Telemetry Protocol

Files:

- `include/protocol.h`
- `src/protocol.c`

Converts the protection status and measurement values into a fixed-length binary telemetry frame.

Responsibilities:

- Numeric measurement encoding.
- State and fault encoding.
- Sequence number insertion.
- Sensor validity mask encoding.
- CRC-8/ATM calculation.

The protocol implementation is independent of the hardware interface.

The host application can encode frames for inspection, while ESP32 firmware transmits the same frame format over UART.

See [protocol.md](protocol.md) for the complete frame specification.

### 3.6 Host Application

File: `src/main.c`

Acts as the host-side application entry point.

It connects the sensor simulator, protection engine and telemetry encoder.

Its processing sequence is:

1. Acquire simulated measurements.
2. Update the protection monitor.
3. Retrieve the current protection status.
4. Display measurement and protection information.
5. Encode and display telemetry.

### 3.7 Automated Tests

Files:

- `tests/test_battery.c`
- `tests/test_protocol.c`

The protection tests verify sensor validation, warning conditions, critical faults, fault priority, hysteresis, fault latching and recovery.

The protocol tests verify CRC calculation, frame encoding, numeric representation, validity handling and error cases.

CMake builds and registers both test executables with CTest.

## 4. Protection Configuration

The current demonstration configuration uses the following thresholds:

| Parameter | Warning | Critical fault | Recovery |
|---|---:|---:|---:|
| Voltage | Below 11.5 V or above 14.5 V | Below 10.5 V or above 15.0 V | 11.8–14.2 V |
| Current | Above 5.0 A | Above 10.0 A | Absolute current below 4.0 A |
| Temperature | Above 45 °C | Above 60 °C | Below 40 °C |

Additional settings:

- Invalid sensor samples required for sensor fault: 3.
- Consecutive safe samples required for recovery: 3.

These are illustrative prototype thresholds, not validated safety limits for a particular battery chemistry.

## 5. ESP32 and Wokwi Interface

The Wokwi circuit uses an ESP32 DevKit, three potentiometers, one pushbutton and four LEDs.

**Input components**

| Component | ESP32 GPIO | Function |
|---|---:|---|
| Voltage potentiometer | GPIO 34 | Simulated voltage input |
| Current potentiometer | GPIO 35 | Simulated current input |
| Temperature potentiometer | GPIO 32 | Simulated temperature input |
| Sensor fault button | GPIO 25 | Invalid sensor injection |

**Output components**

| Component | ESP32 GPIO | Function |
|---|---:|---|
| Green LED | GPIO 26 | Normal state indication |
| Yellow LED | GPIO 27 | Warning state indication |
| Red LED | GPIO 14 | Critical fault indication |
| Blue LED | GPIO 13 | Sensor fault indication |

LED configuration:

- Four LEDs provide visual indication of the protection state.
- Each LED uses an individual 220 Ω current-limiting resistor.
- LEDs are active-high outputs.
- All LED cathodes are connected to ESP32 GND.
- LED control is handled by the ESP32 firmware using the current `BatteryStatus`.

**ADC configuration**

- Resolution: 12 bits.
- Raw ADC range: 0–4095.

**Simulated measurement ranges**

| Measurement | Range |
|---|---:|
| Voltage | 0–18 V |
| Current | 0–12 A |
| Temperature | 0–80 °C |

**UART configuration**

- Baud rate: 115200.
- Sampling interval: 500 ms.

The firmware sends telemetry through the ESP32 UART interface to the Wokwi Serial Monitor.

## 6. Build System

BatteryGuard uses separate build systems for the two execution environments.

### Host build

CMake configures the host application and automated tests.

GCC compiles the platform-independent C modules for Linux.

### ESP32 build

PlatformIO builds the ESP32 firmware using the ESP32 platform and Arduino framework.

The PlatformIO source filter includes:

- `src/battery.c`
- `src/protocol.c`
- `firmware/esp32/main.cpp`

It excludes the host application and host sensor simulator.

This ensures that the ESP32 firmware uses the same core implementation without compiling host-specific entry points.

## 7. Project Directory Structure

```text
batteryguard/
|
|-- CMakeLists.txt
|-- platformio.ini
|-- wokwi.toml
|-- diagram.json
|-- README.md
|-- .gitignore
|
|-- include/
|   |-- battery.h
|   |-- sensor.h
|   |-- protocol.h
|
|-- src/
|   |-- battery.c
|   |-- main.c
|   |-- sensor_sim.c
|   |-- protocol.c
|
|-- firmware/
|   |-- esp32/
|       |-- main.cpp
|
|-- tests/
|   |-- test_battery.c
|   |-- test_protocol.c
|
|-- docs/
    |-- architecture.md
    |-- protocol.md
```

Generated build directories such as `build/` and `.pio/` are excluded from version control.

## 8. Engineering Design Decisions

| Decision | Reason |
|---|---|
| Platform-independent protection engine | Avoids duplicated protection logic |
| Common measurement structure | Provides a consistent interface between platforms |
| Sensor validity bitmask | Tracks independent measurement validity |
| Explicit protection states | Makes system behaviour predictable |
| Warning hysteresis | Prevents rapid state switching |
| Fault latching | Prevents immediate clearing of critical faults |
| Consecutive-sample recovery | Requires sustained safe conditions |
| Fixed-size telemetry frame | Provides predictable binary communication |
| CRC-8/ATM | Detects many transmission errors |
| Separate host and ESP32 build systems | Keeps platform-specific code isolated |
| Automated unit tests | Verifies core behaviour independently of hardware |
| ESP32-side LED indication | Provides visual protection-state feedback without coupling hardware-specific output control to the shared core |

## 9. Limitations

BatteryGuard is a software simulation prototype.

- No physical battery or protection circuit has been tested.
- ADC inputs are simulated using potentiometers in Wokwi.
- ADC scaling is illustrative and has not been calibrated against physical sensors.
- Thresholds are not chemistry-specific safety limits.
- The FAULT state does not physically disconnect a battery or load.
- LEDs provide visual indication only and do not perform electrical protection.
- No battery balancing, state-of-charge estimation or state-of-health estimation is implemented.
- The prototype is not a certified battery management system.

## 10. Verification Status

The project has been verified through:

- Host application execution.
- 20 automated protection tests.
- 10 automated protocol tests.
- CTest execution of both test suites.
- Successful ESP32 firmware compilation.
- Wokwi simulation of normal operation, warning conditions, critical faults, sensor fault injection and recovery.
- Visual verification of normal, warning, critical fault, sensor fault and recovery LED indications.
- UART telemetry inspection through the Wokwi Serial Monitor.

The host and ESP32 environments use the same protection and telemetry implementations.