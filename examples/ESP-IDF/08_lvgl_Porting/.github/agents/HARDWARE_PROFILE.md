# HARDWARE_PROFILE.md

# Bike Computer Hardware Profile

## 1. Purpose

This document describes the target hardware platform of the Bike Computer project.

It is the reference document for all AI agents and developers working on hardware-related features.

Its purpose is to:

- Document confirmed hardware components.
- Record known pin assignments.
- Identify hardware modifications.
- Distinguish verified information from assumptions.
- Prevent incorrect hardware configuration.
- Identify details that must be verified before implementation.

This document must be updated whenever hardware changes are confirmed.

## 2. Hardware Platform Overview

### 2.1 Main development board

| Property | Value |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-4.3B |
| MCU | ESP32-S3 |
| Module | ESP32-S3-WROOM-1-N16R8 |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Display | 4.3-inch RGB |
| Display resolution | 800 × 480 |
| Touch controller | GT911 |
| Wi-Fi | 2.4 GHz |
| Bluetooth | BLE |
| Storage expansion | microSD slot |
| RTC | PCF85063 |
| GPIO expander | CH422G |

The exact board revision must be verified before relying on revision-specific pin assignments or electrical characteristics.

### 2.2 Main processing unit

The ESP32-S3 is responsible for:

- Application execution.
- FreeRTOS task scheduling.
- GNSS communication.
- BLE sensor communication.
- Wi-Fi connectivity.
- Display management.
- Touch input processing.
- Activity recording.
- Data processing and storage coordination.

The module provides 16 MB flash and 8 MB PSRAM.

Memory allocation must account for the requirements of LVGL, networking, BLE, GNSS, and activity recording.

Do not assume that all PSRAM is available to application code.

## 3. Display and Touch

### 3.1 Display

| Property | Value |
|---|---|
| Type | RGB display |
| Diagonal | 4.3 inches |
| Resolution | 800 × 480 |
| Interface | RGB |
| Backlight | Hardware implementation must be verified |

The display is intended to provide the main user interface.

The UI implementation uses LVGL 8.4.0.

Display-specific configuration must remain inside the display driver or Hardware/BSP layer.

### 3.2 Touch controller

| Property | Value |
|---|---|
| Controller | GT911 |
| Type | Capacitive touch |
| Interface | Hardware connection must be verified |

The touch controller is used for:

- Dashboard interaction.
- Menu navigation.
- Configuration.
- Activity controls.
- Map interaction in future versions.

The exact I2C address, GPIO assignments, interrupt behavior, and reset sequence must be verified against the board revision and existing driver.

## 4. GNSS Hardware

### 4.1 Receiver

| Property | Value |
|---|---|
| Module | FlyFishRC M10 Mini |
| GNSS generation | u-blox M10 family |
| Connection | UART |
| Intended role | Primary position source |

The exact receiver firmware and supported features must be verified.

Do not assume that every feature available in the u-blox M10 product family is supported by this specific module.

### 4.2 UART connection

The confirmed application-level pin assignment is:

| Signal | ESP32-S3 GPIO |
|---|---:|
| GNSS RX (ESP32 receives) | GPIO43 |
| GNSS TX (ESP32 transmits) | GPIO44 |

The connection is intended to use a UART peripheral.

Existing firmware may configure:

- UART2.
- TX GPIO44.
- RX GPIO43.
- Baud rate 115200.

These are existing software configuration details and must not be interpreted as independently verified receiver settings.

The actual receiver baud rate must be checked before changing UART configuration.

### 4.3 RS485 hardware modification

The board's RS485 circuitry/path has been modified to make GPIO43 and GPIO44 available for GNSS UART communication.

Important constraints:

- GPIO43 and GPIO44 must be treated as GNSS-related pins.
- Do not restore or assume the original RS485 functionality.
- Do not configure these pins for another peripheral without explicit hardware verification.
- Do not assume the exact electrical topology of the modification.
- Do not assume signal voltage levels or transceiver bypass details.
- Inspect the modified board and verify the electrical connection before making hardware-level changes.

The hardware modification takes precedence over assumptions based on the original board design.

### 4.4 GNSS configuration details to verify

The following information is not yet confirmed:

- Actual UART baud rate.
- NMEA output configuration.
- UBX output configuration.
- Supported GNSS constellations.
- Navigation update rate.
- Enabled messages.
- Antenna characteristics.
- PPS availability and connection.
- AssistNow or A-GNSS support.
- Receiver firmware version.
- Receiver power-control capabilities.

No undocumented receiver configuration commands may be sent.

In particular, do not implement AssistNow or A-GNSS without verifying exact module support, protocol requirements, and licensing constraints.

## 5. Connectivity Hardware

### 5.1 Wi-Fi

The ESP32-S3 provides 2.4 GHz Wi-Fi.

Intended uses:

- Network synchronization.
- Firmware updates.
- Configuration.
- Data export.
- Future companion-device communication.

The exact antenna implementation and RF performance must be validated on the assembled device.

### 5.2 Bluetooth Low Energy

BLE is provided by the ESP32-S3.

Intended uses:

- Heart-rate sensors.
- Speed sensors.
- Cadence sensors.
- Power meters.
- Future BLE peripherals.

The firmware must use standard BLE services whenever supported by the sensor.

Relevant standard services include:

| Service | UUID |
|---|---|
| Heart Rate | 0x180D |
| Battery | 0x180F |
| Device Information | 0x180A |
| Cycling Speed and Cadence | 0x1816 |
| Cycling Power | 0x1818 |

BLE must be managed through the connectivity and sensor abstraction layers.

### 5.3 Phone connectivity

Future phone integration may provide:

- GPS position fallback.
- Internet connectivity.
- Activity synchronization.
- Configuration.
- Notifications.

The phone connection protocol has not yet been finalized.

Do not assume that phone GPS sharing is available through a standard BLE service.

## 6. Storage Hardware

### 6.1 Flash

The module provides 16 MB of flash.

Flash is used for:

- Firmware.
- Configuration.
- Persistent application data.
- NVS.
- Other partitioned resources.

The actual partition table must be inspected before defining storage requirements.

Do not assume that all flash space is available for application data.

### 6.2 PSRAM

The module provides 8 MB of PSRAM.

Potential uses include:

- LVGL buffers.
- Large temporary buffers.
- Map rendering.
- Data processing.
- Other memory-intensive features.

Allocation must consider memory access characteristics and compatibility with the relevant ESP-IDF APIs.

### 6.3 microSD

The development board includes a microSD slot.

Intended uses:

- Activity recording.
- FIT file storage.
- GPX export.
- Map data.
- Future offline navigation resources.

The following details must be verified:

- SPI or SDMMC interface.
- GPIO assignments.
- Card-detect support.
- Bus frequency.
- Power requirements.
- Filesystem configuration.
- Existing driver implementation.

Do not invent microSD pin assignments.

The storage implementation must protect activity data against unexpected removal or power loss.

## 7. Real-Time Clock

The board includes a PCF85063 RTC.

Potential uses:

- Maintaining time across resets.
- Providing a time reference when GNSS is unavailable.
- Timestamping events.
- Supporting activity metadata.

The following details must be verified:

- I2C address.
- Bus assignment.
- GPIO connections.
- RTC backup power arrangement.
- Driver implementation.
- Time synchronization behavior.

GNSS time may be used to synchronize the RTC when a valid GNSS time source is available.

## 8. GPIO Expander

The board includes a CH422G GPIO expander.

Its exact connection and configuration must be verified from the board documentation and existing firmware.

Potential responsibilities depend on the board design and may include control signals that cannot be assigned directly to ESP32-S3 GPIOs.

Agents must:

- Inspect the existing CH422G driver.
- Verify the I2C connection.
- Identify the signals controlled by the expander.
- Avoid duplicating ownership of its outputs.
- Preserve existing initialization and configuration requirements.

Do not assume its output mapping or I2C address.

## 9. Pin Assignment Register

Only confirmed GNSS UART assignments are listed here.

| GPIO | Function | Status |
|---|---|---|
| GPIO43 | GNSS UART RX | Confirmed |
| GPIO44 | GNSS UART TX | Confirmed |

All other peripheral pin assignments must be retrieved from the correct board revision documentation or verified in the existing project.

A complete pin register should eventually include:

- GPIO number.
- Peripheral function.
- Direction.
- Active level.
- Electrical characteristics.
- Hardware owner.
- Software owner.
- Initialization requirements.
- Conflicts and restrictions.

Never infer pin assignments from a different Waveshare board revision or a similar ESP32-S3 product.

## 10. Hardware Ownership

Hardware resources must have clear ownership.

The following ownership model is recommended:

| Resource | Software owner |
|---|---|
| UART GNSS | GNSS driver |
| RGB display | Display driver / Hardware BSP |
| GT911 touch | Touch driver |
| BLE controller | Connectivity layer |
| Wi-Fi controller | Connectivity layer |
| microSD interface | Storage driver |
| PCF85063 RTC | RTC driver |
| CH422G | GPIO expander driver |

The exact ownership must be reconciled with the existing project before implementation.

Application components must not directly access hardware resources owned by another component.

## 11. Electrical and Hardware Safety

All hardware-related changes must follow these rules:

1. Never assume GPIO voltage tolerance.
2. Never connect an unverified signal to an ESP32-S3 GPIO.
3. Verify UART signal levels before connecting external modules.
4. Verify power supply requirements before enabling peripherals.
5. Do not modify the board's RS485 circuitry/path without explicit approval.
6. Do not change GPIO assignments without checking resource conflicts.
7. Do not send undocumented commands to external hardware.
8. Do not perform physical hardware operations without explicit approval.

The board schematic and the actual modified board must both be considered.

## 12. Hardware Information Status

The following table distinguishes confirmed information from items requiring further investigation.

| Item | Status |
|---|---|
| Waveshare board model | Confirmed |
| ESP32-S3-WROOM-1-N16R8 | Confirmed |
| Flash capacity | Confirmed: 16 MB |
| PSRAM capacity | Confirmed: 8 MB |
| Display resolution | Confirmed: 800 × 480 |
| Touch controller | Confirmed: GT911 |
| GNSS module | Confirmed: FlyFishRC M10 Mini |
| GNSS UART RX | Confirmed: GPIO43 |
| GNSS UART TX | Confirmed: GPIO44 |
| RS485 hardware/path modification | Confirmed |
| GNSS electrical topology | To verify |
| GNSS signal voltage levels | To verify |
| GNSS baud rate | To verify |
| GNSS output protocol | To verify |
| Battery monitoring hardware | To verify |
| Battery chemistry and capacity | To verify |
| Charging circuit | To verify |
| Backlight control pin | To verify |
| microSD interface and pins | To verify |
| RTC bus and address | To verify |
| CH422G mapping | To verify |
| GT911 bus and address | To verify |
| Complete GPIO mapping | To verify |
| Board revision | To verify |

## 13. Hardware Verification Checklist

Before implementing a feature that depends on hardware, the responsible agent should:

- [ ] Identify the exact board revision.
- [ ] Inspect the official board schematic.
- [ ] Inspect the existing hardware initialization code.
- [ ] Verify the relevant GPIO assignments.
- [ ] Check peripheral ownership.
- [ ] Verify voltage levels and electrical compatibility.
- [ ] Identify possible resource conflicts.
- [ ] Document unresolved assumptions.
- [ ] Obtain approval before modifying files.
- [ ] Obtain approval before executing commands or interacting with hardware.

A feature must not be considered hardware-ready while critical electrical details remain unknown.

## 14. Hardware Documentation Sources

The following sources should be used when verifying hardware information:

1. Official Waveshare documentation for ESP32-S3-Touch-LCD-4.3B.
2. The schematic corresponding to the exact board revision.
3. ESP32-S3-WROOM-1-N16R8 datasheet.
4. Official Espressif ESP32-S3 technical documentation.
5. GT911 controller documentation.
6. PCF85063 documentation.
7. CH422G documentation.
8. Documentation for the exact FlyFishRC M10 Mini receiver and its firmware.

The actual board and its modifications must be checked against the documentation.

If documentation conflicts with the physical hardware, investigate the discrepancy rather than assuming either source is correct.

## 15. Rules for AI Agents

All project agents must follow this hardware profile.

### Hardware Agent

Responsible for:

- Maintaining the hardware inventory.
- Verifying pin assignments.
- Reviewing hardware initialization.
- Identifying electrical risks.
- Documenting board modifications.

### GNSS Agent

Must:

- Use GPIO43 and GPIO44 for the documented GNSS UART connection.
- Respect the existing RS485 hardware/path modification.
- Verify receiver configuration before changing it.
- Avoid undocumented UBX commands.

### Connectivity Agent

Must:

- Use ESP32-S3 Wi-Fi and BLE capabilities.
- Avoid conflicts with existing peripheral configuration.
- Coordinate access through the connectivity abstraction.

### Storage Agent

Must:

- Verify the microSD interface.
- Inspect the partition table.
- Avoid assumptions about available storage capacity.

### Power Management Agent

Must:

- Verify the battery and charging hardware.
- Avoid inventing GPIO assignments or voltage thresholds.
- Verify display and peripheral power-control mechanisms.

### Firmware Agent

Must:

- Respect hardware resource ownership.
- Avoid direct hardware access from application-level modules.
- Preserve existing working hardware configuration unless a change is approved.

## 16. Approval and Execution Policy

The project uses supervised autonomy, central coordination, and project-level agent scope.

The file approval policy is `approval_each`.

The Quality review policy is `major`.

Therefore:

- Every file creation requires explicit approval.
- Every file modification requires explicit approval.
- Every deletion requires explicit approval.
- Every rename or move requires explicit approval.
- Approval for one file does not authorize changes to other files.

Agents must not execute commands without explicit approval.

This includes:

- Build commands.
- Test commands.
- Formatting commands.
- Flashing commands.
- Hardware interaction.
- GPIO manipulation.
- Firmware configuration changes.
- Git operations that modify repository state.

Read-only inspection may be performed within the established project permissions.

## 17. Definition of Done

This hardware profile is considered complete when:

- All confirmed hardware components are documented.
- Confirmed pin assignments are recorded.
- The GNSS UART connection is documented.
- The RS485 hardware/path modification is documented.
- Unverified electrical details are explicitly identified.
- All agents follow the hardware ownership rules.
- Hardware-dependent implementation decisions are based on verified information.
- Any future hardware changes are reflected in this document.