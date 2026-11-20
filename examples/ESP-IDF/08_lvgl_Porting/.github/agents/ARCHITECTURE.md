


# Bike Computer – Software Architecture

**Document status:** Architecture definition  
**Project:** ESP32-S3 Bike Computer  
**Framework:** ESP-IDF 5.3.0  
**Language:** C  
**RTOS:** FreeRTOS  
**Graphics library:** LVGL 8.4.0

---

## 1. Introduction

This document describes the software architecture of the Bike Computer project.

The objective is to develop a modular, reliable, and extensible cycling computer based on the ESP32-S3.

The system is designed to support:

- Real-time cycling metrics.
- GNSS positioning.
- BLE cycling sensors.
- Configurable dashboards.
- Activity recording.
- Offline data storage.
- Wi-Fi connectivity.
- Activity export.
- Strava integration.
- Future navigation and mapping capabilities.
- Future smartphone integration.

The architecture must support incremental development without introducing unnecessary dependencies between components.

## 2. Hardware Platform

### 2.1 Main board

The current hardware platform is:

| Component | Specification |
|---|---|
| Board | Waveshare ESP32-S3-Touch-LCD-4.3B |
| MCU | ESP32-S3-WROOM-1-N16R8 |
| Flash | 16 MB |
| PSRAM | 8 MB |
| Display | 4.3-inch RGB |
| Resolution | 800 × 480 |
| Touch controller | GT911 |
| Wi-Fi | 2.4 GHz |
| Bluetooth | BLE |
| RTC | PCF85063 |
| GPIO expander | CH422G |
| Storage expansion | microSD |

### 2.2 GNSS receiver

The current GNSS receiver is:

- FlyFishRC M10 Mini.
- u-blox M10 generation.

The receiver communicates through UART.

| Signal | ESP32-S3 GPIO |
|---|---:|
| GNSS TX → ESP32 RX | GPIO43 |
| GNSS RX ← ESP32 TX | GPIO44 |

The user has modified the board's RS485 circuitry/path to make these pins available for GNSS UART communication.

The original RS485 configuration must not be assumed to remain applicable.

The following information still requires hardware verification:

- Exact electrical topology.
- UART voltage levels.
- Actual baud rate.
- Receiver output configuration.
- Receiver firmware capabilities.
- Supported configuration commands.

Do not assume support for AssistNow or A-GNSS without verifying the exact receiver firmware and protocol capabilities.

## 3. Software Architecture Overview

The software is organized into logical layers.

<CodeBlock language="mermaid" editable>
flowchart TB
    HW["Hardware and BSP"]
    DRV["Hardware Drivers and Transports"]
    PROTO["Protocol Adapters"]
    HUB["Data Hub"]
    CORE["Cycling Core"]
    ACT["Activity Recorder and Storage"]
    CONN["Connectivity"]
    UI["UI / UX"]

    HW --> DRV
    DRV --> PROTO
    PROTO --> HUB
    HUB --> CORE
    CORE --> ACT
    HUB --> UI
    CORE --> UI
    ACT --> UI
    CONN --> UI
    CONN --> ACT
</CodeBlock>

The diagram represents logical data dependencies, not necessarily a direct function-call chain.

Each component must expose a clearly defined interface.

The UI must never access hardware drivers directly.

The Cycling Core must not depend on LVGL.

The Data Hub must remain independent of presentation logic.

## 4. Architectural Layers

### 4.1 Layer 1 – Hardware and BSP

Responsible for the physical hardware platform.

Responsibilities:

- Board initialization.
- GPIO configuration.
- Display hardware configuration.
- Touch controller hardware configuration.
- UART peripheral configuration.
- I2C peripheral configuration.
- SPI peripheral configuration when required.
- Power management.
- Hardware-specific initialization.

This layer must isolate board-specific details from application logic.

### 4.2 Layer 2 – Hardware Drivers and Transports

Responsible for communication with physical peripherals.

Potential components:

- GNSS UART transport.
- BLE transport.
- Wi-Fi driver integration.
- Display driver.
- Touchscreen driver.
- RTC driver.
- microSD interface.
- GPIO expander driver.

Drivers must provide well-defined interfaces and explicit error handling.

They must not implement cycling-domain logic.

### 4.3 Layer 3 – Protocol Adapters

Responsible for converting raw communication data into structured measurements.

Examples:

- NMEA parser.
- UBX parser.
- BLE Cycling Speed and Cadence parser.
- BLE Cycling Power parser.
- BLE Heart Rate parser.
- BLE Battery Service parser.

Protocol adapters must validate incoming data before publishing it.

Responsibilities include:

- Frame validation.
- Length validation.
- Checksum verification.
- Field extraction.
- Unit conversion where appropriate.
- Protocol-specific error handling.

Protocol adapters must not directly update UI objects.

### 4.4 Layer 4 – Data Hub

The Data Hub is the central measurement-management component.

Its responsibility is to provide a unified representation of measurements from different sources.

It receives measurements from protocol adapters and makes normalized data available to application components.

Responsibilities:

- Measurement normalization.
- Timestamp management.
- Validity tracking.
- Freshness tracking.
- Measurement quality.
- Source identification.
- Source selection.
- Source transition management.
- Thread-safe data access.

The Data Hub must prevent consumers from needing to understand individual sensor protocols.

### 4.5 Layer 5 – Cycling Core

The Cycling Core implements cycling-domain calculations.

Responsibilities:

- Current speed.
- Average speed.
- Maximum speed.
- Distance.
- Elapsed time.
- Moving time.
- Stop detection.
- Auto-pause.
- Activity state.
- Cadence statistics.
- Heart rate statistics.
- Power statistics.
- Altitude and elevation calculations.

The Cycling Core consumes normalized measurements from the Data Hub.

It must remain independent of:

- LVGL.
- UART.
- BLE.
- Wi-Fi.
- Hardware-specific drivers.

### 4.6 Layer 6 – Activity Recorder and Storage

Responsible for recording and preserving activity data.

Responsibilities:

- Activity lifecycle integration.
- Recording samples.
- Data buffering.
- Persistent storage.
- Activity metadata.
- Recovery after unexpected shutdown.
- FIT export.
- GPX export.

The storage implementation must support offline operation.

The activity recording architecture must not require an Internet connection.

### 4.7 Layer 7 – Connectivity

Responsible for external communication.

Potential functionality:

- Wi-Fi management.
- HTTP/HTTPS communication.
- Smartphone integration.
- Activity synchronization.
- Strava integration.
- Firmware update support, if introduced.

Connectivity must be isolated from cycling calculations.

An unavailable network must not prevent the device from recording an activity locally.

### 4.8 Layer 8 – UI/UX

Responsible for presentation and user interaction.

Technology:

- LVGL 8.4.0.

Responsibilities:

- Main dashboard.
- Cycling metrics.
- Screen navigation.
- Sensor status.
- GNSS status.
- Activity controls.
- Settings.
- Navigation interface.
- User feedback.

The UI retrieves application data through defined interfaces.

It must never access GNSS UART, BLE drivers, or storage directly.

## 5. Data Flow

### 5.1 GNSS data flow

<CodeBlock language="mermaid" editable>
flowchart TD
    GPS["M10 Mini GNSS Receiver"]
    UART["UART Transport"]
    PARSER["NMEA / UBX Parser"]
    VALID["Measurement Validation"]
    HUB["Data Hub"]
    CORE["Cycling Core"]
    UI["Dashboard"]
    REC["Activity Recorder"]

    GPS --> UART
    UART --> PARSER
    PARSER --> VALID
    VALID --> HUB
    HUB --> CORE
    HUB --> UI
    CORE --> UI
    CORE --> REC
</CodeBlock>

The GNSS receiver provides positioning and related measurements.

The parser validates incoming protocol messages.

The Data Hub normalizes measurements and manages their validity and freshness.

The Cycling Core uses the measurements for cycling calculations.

The UI presents the resulting information.

The Activity Recorder stores the required activity samples.

### 5.2 BLE sensor data flow

<CodeBlock language="mermaid" editable>
flowchart TD
    SENSOR["BLE Cycling Sensor"]
    BLE["BLE Transport"]
    PARSER["BLE Profile Parser"]
    VALID["Measurement Validation"]
    HUB["Data Hub"]
    CORE["Cycling Core"]
    UI["Dashboard"]

    SENSOR --> BLE
    BLE --> PARSER
    PARSER --> VALID
    VALID --> HUB
    HUB --> CORE
    HUB --> UI
    CORE --> UI
</CodeBlock>

BLE sensor measurements must be normalized before being consumed by the Cycling Core or UI.

### 5.3 UI data flow

The UI must consume prepared application data.

It must not:

- Parse NMEA sentences.
- Parse UBX messages.
- Decode BLE characteristics.
- Calculate trip distance independently.
- Select between GNSS and BLE speed sources.
- Access storage files directly.

All such responsibilities belong to the appropriate application component.

## 6. Data Hub Design

### 6.1 Measurement representation

Every measurement should contain sufficient metadata to determine whether it can safely be consumed.

A conceptual measurement structure is:

```c
typedef struct {
    float value;
    int64_t timestamp_ms;
    bool valid;
    uint8_t source;
    uint8_t quality;
} measurement_t;
```

This is an illustrative contract, not a mandatory implementation.

The final structure must be defined in `DATA_CONTRACTS.md` and must support the actual requirements of the project.

Consider using explicit types for:

- Unit.
- Validity state.
- Source.
- Quality.
- Timestamp semantics.

### 6.2 Measurement sources

Potential sources include:

| Measurement | Primary source | Future alternative |
|---|---|---|
| Position | GNSS UART | Smartphone GPS |
| Speed | BLE wheel sensor, when configured | GNSS |
| Cadence | BLE cadence sensor | None |
| Heart rate | BLE HR sensor | None |
| Power | BLE power meter | None |
| Altitude | GNSS | Barometer |

Source priority must be configurable where appropriate.

The Data Hub must manage source selection independently for each metric.

### 6.3 Source switching

Source switching must avoid:

- Rapid oscillation between sources.
- Duplicate measurements.
- Invalid measurements.
- Sudden artificial jumps.
- Incorrect timestamps.
- Distance double counting.

Use source hysteresis and explicit transition handling where appropriate.

Do not blindly combine measurements from different sources.

## 7. GNSS Architecture

### 7.1 Initial implementation

The initial GNSS implementation uses UART communication with the FlyFishRC M10 Mini.

The software should support protocol adapters that can be extended independently of the UART transport.

Potential protocol support:

- NMEA.
- UBX, if implemented and verified.

### 7.2 GNSS responsibilities

The GNSS component is responsible for:

- UART data reception.
- Frame extraction.
- Protocol parsing.
- Checksum validation.
- Position validation.
- Fix status.
- Speed extraction.
- Altitude extraction.
- Timestamp extraction.
- Receiver status.

The Data Hub is responsible for making the resulting measurements available to the rest of the application.

### 7.3 Future GNSS improvements

Potential improvements:

- Advanced receiver configuration.
- Additional GNSS constellations.
- Improved positioning accuracy.
- Receiver diagnostics.
- A-GNSS support, if verified.
- Smartphone GPS fallback.

All receiver-specific functionality must be verified against the actual M10 Mini firmware.

## 8. BLE Sensors Architecture

The BLE subsystem must support standard Bluetooth SIG cycling services.

Potential services:

| Service | UUID | Purpose |
|---|---|---|
| Cycling Speed and Cadence | `0x1816` | Wheel speed and cadence |
| Cycling Power | `0x1818` | Cycling power |
| Heart Rate | `0x180D` | Heart rate |
| Battery | `0x180F` | Battery level |
| Device Information | `0x180A` | Device information |

The implementation must support:

- Scanning.
- Device discovery.
- Connection.
- Service discovery.
- Characteristic discovery.
- Notifications.
- Disconnection.
- Reconnection.
- Sensor identification.

The architecture should allow multiple sensors to be connected simultaneously where supported by the BLE stack and available resources.

The BLE subsystem must not own cycling calculations.

## 9. Cycling Core Architecture

The Cycling Core must provide a consistent representation of activity metrics.

Potential outputs:

| Metric | Description |
|---|---|
| Current speed | Latest valid speed |
| Average speed | Distance divided by the selected time basis |
| Maximum speed | Highest accepted speed |
| Distance | Accumulated activity distance |
| Elapsed time | Activity duration |
| Moving time | Time spent moving |
| Cadence | Current cadence |
| Average cadence | Average cadence |
| Heart rate | Current heart rate |
| Average heart rate | Average heart rate |
| Power | Current power |
| Average power | Average power |
| Altitude | Current altitude |
| Total ascent | Accumulated elevation gain |

Average speed over elapsed time and average speed over moving time must remain distinct metrics.

The Cycling Core must use a monotonic time source for duration calculations.

## 10. Activity Recording and Storage

Activity recording must be offline-first.

The device must be able to record activities without Wi-Fi or smartphone connectivity.

### 10.1 Recording responsibilities

The Activity Recorder should manage:

- Activity start.
- Activity pause.
- Activity resume.
- Activity finish.
- Sample buffering.
- Periodic storage.
- Activity metadata.
- Data integrity.
- Recovery behavior.

### 10.2 File formats

The target formats are:

- FIT: primary activity export format.
- GPX: complementary route export format.

FIT export must preserve relevant activity information, including timestamps and supported sensor metrics.

GPX export should focus on route and track information.

Do not assume that the internal storage representation must be identical to the export format.

### 10.3 Storage resilience

The implementation should consider:

- Unexpected power loss.
- Partial writes.
- Storage exhaustion.
- Corrupted files.
- Interrupted recording.
- Recovery after reboot.

A failed storage operation must not silently corrupt activity statistics.

## 11. Connectivity Architecture

Connectivity must remain an optional subsystem.

Potential responsibilities:

- Wi-Fi connection management.
- HTTPS requests.
- Smartphone communication.
- Activity upload.
- Synchronization.
- Remote services.

### 11.1 Strava integration

Strava integration should use a secure authentication and upload architecture.

The firmware must not contain confidential server-side OAuth client secrets.

Where appropriate, a companion application or backend should handle sensitive authentication operations.

The device should retain locally recorded activities when synchronization is unavailable.

### 11.2 Smartphone integration

Future smartphone functionality may include:

- GPS assistance.
- Activity synchronization.
- Configuration.
- Notifications.
- Navigation data.

Phone GPS fallback must use an explicitly defined communication protocol.

Do not assume that a standard BLE cycling profile provides smartphone GPS data.

## 12. UI Architecture

The UI is based on LVGL 8.4.0.

The main dashboard should display the most relevant cycling metrics.

Potential screens:

- Main dashboard.
- Activity recording.
- Activity summary.
- Sensor management.
- GNSS status.
- Navigation.
- Settings.
- System information.

### 12.1 UI update strategy

The UI must update independently of measurement acquisition and cycling calculations.

Different metrics may have different update rates.

Avoid unnecessary full-screen redraws and repeated object allocation.

### 12.2 LVGL thread safety

All LVGL operations must follow the established synchronization strategy.

Communication callbacks must not call LVGL directly.

The UI should retrieve application data through thread-safe interfaces.

## 13. FreeRTOS and Task Architecture

The architecture must use FreeRTOS tasks according to clear ownership and execution responsibilities.

Potential task responsibilities include:

- GNSS reception and processing.
- BLE event handling.
- Data Hub management.
- Cycling calculations.
- Activity recording.
- UI updates.
- Connectivity operations.

This is a logical decomposition, not a requirement to create one task for every component.

Avoid unnecessary task proliferation.

Before introducing a new task, evaluate:

- Execution frequency.
- Stack requirements.
- Priority.
- Core affinity.
- Synchronization requirements.
- Memory consumption.
- Scheduling impact.

Task configuration must be based on actual measurements and validation.

### 13.1 Shared data

Shared state must use appropriate synchronization.

Do not assume that `volatile` guarantees thread safety.

Use queues, mutexes, event groups, or other synchronization mechanisms according to the actual access pattern.

Avoid holding locks while performing expensive operations.

## 14. Error Handling Strategy

Each component must expose errors through a consistent mechanism.

Potential error categories:

- Hardware initialization failure.
- Communication failure.
- Protocol parsing failure.
- Invalid measurement.
- Stale measurement.
- Sensor disconnection.
- Storage failure.
- Network failure.
- Resource exhaustion.

Errors must not silently propagate into incorrect measurements.

The system should remain operational when optional subsystems are unavailable.

Examples:

- GNSS loss must not crash the application.
- BLE disconnection must not invalidate unrelated GNSS measurements.
- Wi-Fi failure must not stop local activity recording.
- Storage failure must be reported clearly.

## 15. Memory and Performance Strategy

The ESP32-S3 has limited resources compared with desktop systems.

The architecture must consider:

- Internal RAM.
- PSRAM.
- Flash.
- Task stacks.
- Queues.
- LVGL buffers.
- BLE memory requirements.
- Storage buffers.
- Dynamic allocation.
- Memory fragmentation.

Avoid unnecessary dynamic allocation.

Use fixed-size buffers where appropriate.

Do not assume that all allocations should use PSRAM.

Memory allocation policies must be based on actual hardware and performance requirements.

## 16. Development Phases

The architecture should be implemented incrementally.

### Phase 1 – Foundation

- Stabilize the existing ESP-IDF project.
- Define component boundaries.
- Establish common interfaces.
- Verify board initialization.
- Verify GNSS UART communication.

### Phase 2 – Data and Cycling Logic

- Implement measurement contracts.
- Implement the Data Hub.
- Integrate GNSS measurements.
- Implement Cycling Core.
- Validate speed, distance, and time calculations.

### Phase 3 – BLE Sensors

- Implement BLE sensor discovery.
- Support Heart Rate.
- Support Cycling Speed and Cadence.
- Support Cycling Power.
- Integrate measurements into the Data Hub.

### Phase 4 – Activity Recording

- Implement activity lifecycle.
- Implement sample recording.
- Implement local storage.
- Implement FIT export.
- Implement GPX export.

### Phase 5 – UI Improvements

- Develop the main dashboard.
- Implement configurable metrics.
- Implement sensor management.
- Implement settings.
- Improve visual feedback.

### Phase 6 – Connectivity

- Implement Wi-Fi management.
- Implement smartphone communication.
- Implement activity synchronization.
- Integrate Strava upload through a secure architecture.

### Phase 7 – Navigation

- Define navigation data contracts.
- Implement route handling.
- Develop map presentation.
- Implement turn-by-turn guidance.
- Evaluate map storage and resource requirements.

These phases represent a development strategy, not a fixed delivery schedule.

## 17. Architecture Constraints

The following constraints are mandatory:

1. UI must not access hardware drivers directly.
2. Cycling Core must not depend on LVGL.
3. Protocol parsers must not implement cycling business logic.
4. Data Hub must own measurement normalization and source selection.
5. Activity recording must work without Internet connectivity.
6. Connectivity failures must not prevent local activity recording.
7. Invalid and stale measurements must be handled explicitly.
8. Components must expose well-defined interfaces.
9. Shared data must be accessed safely.
10. Hardware-specific behavior must remain isolated.
11. Receiver configuration commands must be verified before use.
12. No agent may modify files without explicit approval for each file.

## 18. Documentation References

The following documents define additional project details:

| Document | Purpose |
|---|---|
| `DATA_CONTRACTS.md` | Measurement structures and interface contracts |
| `HARDWARE_PROFILE.md` | Verified hardware configuration |
| `ROADMAP.md` | Development priorities and planned features |
| `README_INSTALLATION.md` | Development environment and installation |

The implementation must remain consistent with these documents.

## 19. Definition of Done

An architectural change is complete only when:

- Component responsibilities are clearly defined.
- Dependencies respect the architecture.
- Data flow is documented.
- Interfaces are specified.
- Thread safety has been considered.
- Error handling is defined.
- Memory and performance implications are evaluated.
- Affected documentation is identified.
- Quality review has been completed when required.
- Every file modification has been explicitly approved.

**The architecture must remain modular, testable, reliable, and extensible throughout development.**

