# BLE Sensors Specification

## 1. Purpose

This document defines the architecture, requirements, interfaces, and operational behavior of the Bluetooth Low Energy (BLE) Sensors subsystem.

The subsystem is responsible for discovering, connecting to, monitoring, and decoding external cycling sensors.

The initial hardware platform is:

- MCU: ESP32-S3.
- Framework: ESP-IDF 5.3.0.
- Bluetooth stack: ESP-IDF Bluetooth stack.
- BLE host stack: NimBLE, subject to verification of the existing project configuration.

The target sensors include:

- Speed sensors.
- Cadence sensors.
- Combined speed and cadence sensors.
- Heart rate monitors.
- Cycling power meters.

No additional BLE sensors are currently assumed to be available.

This document defines the target architecture, not an assertion that all sensor types are already implemented.

---

## 2. Design Principles

The BLE subsystem must follow these principles:

1. Use standard Bluetooth SIG GATT services whenever possible.
2. Separate BLE transport from sensor protocol decoding.
3. Support multiple sensor types.
4. Keep sensor connection state independent from measurement validity.
5. Normalize measurements before publishing them.
6. Track measurement timestamps and freshness.
7. Support reconnection after temporary disconnections.
8. Avoid blocking BLE callbacks.
9. Keep BLE implementation details outside the Cycling Core and UI.
10. Allow additional sensor types to be introduced without redesigning the entire subsystem.

---

## 3. Supported Sensor Types

The initial target sensor types are:

| Sensor | Standard Service UUID |
|---|---|
| Heart Rate | `0x180D` |
| Cycling Speed and Cadence | `0x1816` |
| Cycling Power | `0x1818` |
| Battery Service | `0x180F` |
| Device Information | `0x180A` |

The subsystem must identify sensors by their supported services and characteristics rather than relying exclusively on device names.

Device names may be missing, duplicated, or changed by manufacturers.

### 3.1 Speed Sensors

Speed can be obtained from:

- A dedicated wheel speed sensor.
- A combined speed and cadence sensor.

The standard Cycling Speed and Cadence service may provide wheel revolution data.

### 3.2 Cadence Sensors

Cadence can be obtained from:

- A dedicated cadence sensor.
- A combined speed and cadence sensor.

The subsystem must support sensors that provide only one of these measurements.

### 3.3 Heart Rate Sensors

Heart rate sensors must use the standard Heart Rate Service.

The implementation must support the Heart Rate Measurement characteristic format defined by the Bluetooth specification.

### 3.4 Power Meters

Power meters must use the standard Cycling Power Service.

The implementation should initially focus on instantaneous power.

Additional power-related information may be supported later.

---

## 4. Software Architecture

The BLE subsystem should be organized into the following logical layers:

1. BLE host and transport.
2. Device discovery.
3. Connection management.
4. GATT service discovery.
5. Characteristic subscription.
6. Sensor-specific protocol decoders.
7. Measurement normalization.
8. Data Hub publication.

Conceptual architecture:

```text
                  BLE Sensors
                       |
                       v
                 BLE Host Stack
                       |
                       v
                 Device Scanner
                       |
                       v
              Connection Manager
                       |
                       v
                GATT Discovery
                       |
                       v
              Characteristic Client
                       |
          +------------+------------+
          |            |            |
          v            v            v
        CSC           HRM          Power
       Decoder       Decoder       Decoder
          |            |            |
          +------------+------------+
                       |
                       v
                Measurement
                 Normalization
                       |
                       v
                    Data Hub
```

The implementation may use shared infrastructure for all sensor types.

Sensor-specific decoding must remain separate from BLE connection management.

---

## 5. Bluetooth Stack Integration

### 5.1 ESP-IDF Integration

The implementation must use the Bluetooth stack already configured by the project.

Before modifying the BLE subsystem, inspect:

- ESP-IDF version.
- Bluetooth controller configuration.
- NimBLE configuration.
- Existing BLE initialization.
- Existing callbacks.
- Existing connection handling.
- Existing task configuration.

Do not initialize a second Bluetooth host stack.

Do not introduce Bluedroid if the project already uses NimBLE, or vice versa, without an explicit architectural decision.

### 5.2 Initialization

The BLE subsystem should provide a single initialization entry point.

Conceptual interface:

```c
esp_err_t ble_sensors_init(void);
```

Initialization must:

1. Initialize the Bluetooth controller if required.
2. Initialize the selected host stack.
3. Register callbacks.
4. Initialize connection-management state.
5. Start the host task or event-processing mechanism.
6. Report initialization status.

The exact sequence must follow the selected ESP-IDF Bluetooth stack requirements.

### 5.3 Resource Management

The implementation must consider:

- Maximum simultaneous connections.
- BLE connection intervals.
- Host stack memory.
- GATT operation timeouts.
- Discovery timeouts.
- Notification rates.
- Reconnection behavior.

The maximum number of simultaneous sensors must be configurable or centrally defined.

Do not assume that the ESP32-S3 can maintain an unlimited number of connections.

---

## 6. Device Discovery

### 6.1 Scanning

The BLE subsystem must support scanning for nearby devices.

Scanning should be performed according to application state and power requirements.

The scanner should:

- Discover nearby BLE devices.
- Read advertised service UUIDs when available.
- Capture device addresses.
- Capture device names when available.
- Track RSSI.
- Identify potentially supported sensors.

### 6.2 Device Identification

Device identification should prioritize:

1. Advertised service UUIDs.
2. GATT service discovery.
3. Supported characteristics.
4. Manufacturer-specific information, when useful.
5. Device name as supplementary information.

A device must not be classified as a valid sensor solely because its name contains words such as "Heart Rate" or "Cadence".

### 6.3 Discovery Results

Conceptual structure:

```c
typedef struct {
    uint8_t address[6];

    char name[32];

    int8_t rssi;

    bool has_heart_rate_service;
    bool has_csc_service;
    bool has_cycling_power_service;

} ble_discovery_result_t;
```

The final structure should reflect the actual BLE stack and project requirements.

Discovery results must not expose internal BLE stack pointers.

---

## 7. Sensor Pairing and Configuration

### 7.1 Pairing Concept

The application must allow users to associate discovered sensors with logical sensor roles.

Examples:

- Primary speed sensor.
- Primary cadence sensor.
- Primary heart rate sensor.
- Primary power meter.

Pairing must be treated separately from a temporary BLE connection.

A sensor may be paired but currently disconnected.

### 7.2 Persistent Configuration

The application should persist the selected sensor identity.

Configuration should include, where applicable:

- Sensor role.
- BLE address.
- Address type.
- Device name.
- Sensor capabilities.
- Wheel circumference.
- Connection preference.

BLE privacy and address randomization must be considered.

Do not assume that every sensor uses a permanent public address.

If a sensor uses a resolvable private address, the implementation must account for the relevant bonding and identity-resolution mechanisms supported by the chosen stack.

### 7.3 User Interface

The UI should eventually support:

- Starting a sensor scan.
- Viewing discovered sensors.
- Selecting a sensor.
- Assigning a sensor role.
- Removing a configured sensor.
- Viewing connection state.
- Viewing battery level when available.

The BLE subsystem must not implement UI widgets.

It must expose the necessary discovery and configuration events through application interfaces.

---

## 8. Connection Management

### 8.1 Connection State

Conceptual definition:

```c
typedef enum {
    BLE_SENSOR_DISCONNECTED = 0,
    BLE_SENSOR_CONNECTING,
    BLE_SENSOR_CONNECTED,
    BLE_SENSOR_DISCOVERING,
    BLE_SENSOR_SUBSCRIBING,
    BLE_SENSOR_READY,
    BLE_SENSOR_ERROR
} ble_sensor_connection_state_t;
```

A sensor is READY only when the required services and characteristics have been discovered and the required subscriptions have been established.

A connected BLE device is not necessarily a usable sensor.

### 8.2 Connection Lifecycle

The connection manager must handle:

1. Connection request.
2. Connection establishment.
3. GATT service discovery.
4. Characteristic discovery.
5. Notification subscription.
6. Measurement reception.
7. Disconnection.
8. Reconnection.

Every transition must be reflected in the sensor state.

### 8.3 Reconnection

The subsystem should attempt reconnection after unexpected disconnections.

Reconnection must use a controlled retry strategy.

The strategy should consider:

- Retry interval.
- Increasing delay after repeated failures.
- Maximum retry frequency.
- User-requested disconnection.
- Application power state.
- Sensor availability.

Do not continuously reconnect in a tight loop.

### 8.4 Connection Failures

Possible errors include:

- Device unavailable.
- Connection timeout.
- GATT discovery failure.
- Unsupported service.
- Missing characteristic.
- Subscription failure.
- Authentication or security failure.
- Unexpected disconnection.

Errors must be exposed through diagnostics and sensor status.

---

## 9. Standard GATT Services

### 9.1 Heart Rate Service

Service UUID:

```text
0x180D
```

Characteristic:

```text
Heart Rate Measurement: 0x2A37
```

Optional characteristics:

```text
Body Sensor Location: 0x2A38
Heart Rate Control Point: 0x2A39
```

The implementation must decode the Heart Rate Measurement flags correctly.

The characteristic may encode heart rate using:

- 8-bit value.
- 16-bit value.

Additional fields may include:

- Energy Expended.
- RR-Interval values.

The decoder must use the flags to determine which fields are present.

Do not assume a fixed characteristic length.

The initial implementation must publish heart rate in BPM.

RR-Interval support may be added later if required.

### 9.2 Cycling Speed and Cadence Service

Service UUID:

```text
0x1816
```

Characteristic:

```text
CSC Measurement: 0x2A5B
```

Optional characteristics:

```text
CSC Feature: 0x2A5C
Sensor Location: 0x2A5D
SC Control Point: 0x2A55
```

The CSC Measurement characteristic uses flags to indicate whether wheel and/or crank data are present.

The decoder must correctly handle:

- Cumulative wheel revolutions.
- Last wheel event time.
- Cumulative crank revolutions.
- Last crank event time.
- Optional field combinations.

Event times use the units defined by the Bluetooth specification.

The implementation must correctly handle 16-bit event-time rollover and 32-bit revolution-counter rollover.

### 9.3 Cycling Power Service

Service UUID:

```text
0x1818
```

Characteristic:

```text
Cycling Power Measurement: 0x2A63
```

Optional characteristics include:

```text
Cycling Power Feature: 0x2A65
Sensor Location: 0x2A5D
Cycling Power Control Point: 0x2A66
```

The decoder must:

- Read the flags.
- Decode instantaneous power.
- Handle optional fields according to the flags.
- Validate packet length.
- Handle signed power values correctly.

Instantaneous power is expressed in watts.

Additional features such as pedal balance, accumulated torque, and crank revolution data may be supported later.

### 9.4 Battery Service

Service UUID:

```text
0x180F
```

Characteristic:

```text
Battery Level: 0x2A19
```

Battery level is normally expressed as a percentage.

The value must be validated before publication.

Battery information is optional and must not be required for a sensor to become READY.

### 9.5 Device Information Service

Service UUID:

```text
0x180A
```

Optional characteristics may expose:

- Manufacturer Name.
- Model Number.
- Serial Number.
- Firmware Revision.
- Hardware Revision.
- Software Revision.

Only retrieve fields that are useful and permitted by the sensor.

Serial numbers and other identifying information must not be logged unnecessarily.

---

## 10. Sensor Measurement Contracts

All decoded measurements must be normalized before publication.

### 10.1 Speed

Internal unit:

- Meters per second.

For wheel-based sensors, speed should be calculated using wheel circumference and the difference between cumulative revolution counts and event times.

Conceptual formula:

```text
delta_distance_m =
    delta_revolutions * wheel_circumference_m

delta_time_s =
    delta_event_time_s

speed_mps =
    delta_distance_m / delta_time_s
```

The implementation must correctly handle counter rollover.

The first received event must initialize the measurement baseline rather than producing an artificial speed.

Zero or invalid time intervals must not produce a valid speed measurement.

Wheel circumference must be configurable.

### 10.2 Cadence

Internal unit:

- Revolutions per minute (RPM).

Cadence may be derived from crank revolution counts and event times.

The implementation must correctly handle:

- Counter rollover.
- Event-time rollover.
- Missing crank events.
- Invalid intervals.

A sensor providing wheel data only must not be assumed to provide cadence.

### 10.3 Heart Rate

Internal unit:

- Beats per minute (BPM).

Heart rate must be decoded according to the Heart Rate Measurement flags.

A missing notification must eventually make the measurement stale.

The sensor connection may remain active while the heart rate measurement is stale.

### 10.4 Power

Internal unit:

- Watts.

Instantaneous power must be decoded from the Cycling Power Measurement characteristic.

The implementation must respect the signed integer representation.

Do not assume that every power meter provides cadence.

### 10.5 Measurement Metadata

Every published measurement must include:

- Measurement value.
- Unit.
- Status.
- Source.
- Measurement timestamp.
- Reception timestamp.
- Quality, when available.

For BLE measurements, use `DATA_SOURCE_BLE_WHEEL`, `DATA_SOURCE_BLE_CADENCE`, `DATA_SOURCE_BLE_HEART_RATE`, or `DATA_SOURCE_BLE_POWER`, as appropriate.

The exact source identifier should reflect the origin of each metric, not merely the connection's device type.

---

## 11. Data Hub Integration

The BLE subsystem must publish normalized measurements to the Unified Data Hub.

It must not expose raw GATT notifications to the Cycling Core or UI.

Measurements to publish:

- Wheel speed.
- Cadence.
- Heart rate.
- Power.
- Battery level.
- Sensor status.

The Data Hub is responsible for selecting the active application-level source when multiple sources exist.

For example:

- BLE wheel speed versus GNSS speed.
- Future phone GPS versus UART GNSS position.

The BLE subsystem must not implement global source selection.

### 11.1 Publication Frequency

Measurements should be published when new valid data arrives or when relevant status changes.

Avoid unnecessary publication of unchanged values unless required for freshness tracking or diagnostics.

The implementation must not assume that every sensor sends notifications at the same rate.

### 11.2 Stale Measurements

The BLE subsystem or Data Hub must detect missing updates.

Freshness thresholds must account for sensor update rates and metric type.

A sensor that remains connected but stops sending notifications must not continue presenting its last measurement as current.

---

## 12. Data Hub and Connection Status Separation

The following concepts must remain independent:

- BLE radio availability.
- Device connection state.
- GATT discovery state.
- Notification subscription state.
- Measurement availability.
- Measurement freshness.

Examples:

A sensor can be connected and READY but have no valid measurement yet.

A sensor can be connected and READY while its last measurement is stale.

A sensor can be disconnected while its previous measurement remains temporarily available but is marked stale according to its freshness policy.

The UI must display these states accurately.

---

## 13. Concurrency and Callback Rules

BLE callbacks must remain short and non-blocking.

Callbacks must not:

- Perform storage operations.
- Execute lengthy calculations.
- Update LVGL directly.
- Wait indefinitely for another task.
- Perform synchronous network operations.

Callbacks should validate basic notification boundaries and forward data to an appropriate processing context.

The decoder may run in a dedicated task or another suitable context.

The selected architecture must prevent:

- Concurrent modification of sensor state.
- Use-after-free errors.
- Access to invalid GATT handles.
- Race conditions during disconnection.
- Publication of measurements from obsolete connections.

Connection and notification events must be processed safely even when a sensor disconnects during an operation.

---

## 14. Power Management

BLE scanning and connections consume energy.

The implementation should support different operational modes.

Potential modes:

- Normal activity mode.
- Sensor discovery mode.
- Idle mode.
- Low-power mode.

Scanning should not remain continuously active when unnecessary.

Existing sensor connections should be maintained according to application requirements.

Power-saving changes must not compromise sensor reliability or cause unnecessary disconnections.

---

## 15. Diagnostics

The BLE subsystem should expose diagnostic information.

Suggested diagnostics:

- BLE initialization status.
- Number of discovered devices.
- Number of configured sensors.
- Number of connected sensors.
- Current connection states.
- Connection failures.
- Reconnection attempts.
- GATT discovery failures.
- Notification count per sensor.
- Invalid packet count.
- Last measurement timestamp.
- Last disconnection reason.
- Battery level when available.

Diagnostics must avoid excessive logging in high-frequency notification callbacks.

---

## 16. Testing Requirements

### 16.1 Parser Unit Tests

Test:

- Valid Heart Rate Measurement packets.
- 8-bit heart rate values.
- 16-bit heart rate values.
- Optional RR-Interval fields.
- Valid CSC Measurement packets.
- Wheel-only packets.
- Crank-only packets.
- Combined wheel and crank packets.
- Valid Cycling Power Measurement packets.
- Optional power fields.
- Invalid packet lengths.
- Unknown flag combinations.
- Counter rollover.
- Event-time rollover.

### 16.2 Connection Tests

Test:

- Device discovery.
- Connection establishment.
- Service discovery.
- Characteristic discovery.
- Notification subscription.
- Unexpected disconnection.
- Reconnection.
- User-requested disconnection.
- Multiple simultaneous sensors.
- Unsupported devices.

### 16.3 Measurement Tests

Test:

- Speed calculation.
- Cadence calculation.
- Wheel circumference configuration.
- Invalid event intervals.
- Stale measurement transitions.
- Source metadata.
- Timestamp handling.
- Battery information.

### 16.4 Integration Tests

Verify:

- BLE measurements reach the Data Hub.
- Data Hub snapshots contain correct metadata.
- Cycling Core consumes normalized values.
- UI reports actual connection and measurement states.
- Sensor disconnection does not corrupt activity recording.

### 16.5 Hardware Tests

Hardware tests must be performed only after explicit approval.

Use actual sensors to verify:

- Discovery.
- Connection stability.
- Measurement accuracy.
- Reconnection behavior.
- Simultaneous sensor operation.
- Long-duration activity recording.

---

## 17. Implementation Rules for AI Agents

Before modifying the BLE subsystem, agents must:

1. Inspect the existing Bluetooth stack configuration.
2. Identify the current BLE host stack.
3. Inspect existing connection and discovery code.
4. Identify existing callbacks and tasks.
5. Inspect existing sensor decoders.
6. Identify Data Hub interfaces.
7. Compare current behavior with this specification.
8. Propose the smallest safe implementation.

Agents must not:

- Introduce a second Bluetooth host stack.
- Assume all devices use public BLE addresses.
- Assume all sensors support the same characteristics.
- Block inside BLE callbacks.
- Update LVGL directly from BLE callbacks.
- Bypass the Data Hub.
- Modify unrelated UART or GNSS configuration.
- Introduce unverified manufacturer-specific protocols without a documented requirement.

---

## 18. Future Enhancements

Potential future enhancements include:

- Multiple sensors of the same type.
- Sensor priority configuration.
- Advanced power meter metrics.
- RR-Interval recording.
- Sensor firmware information.
- Improved battery monitoring.
- Sensor calibration.
- Manufacturer-specific extensions.
- Advanced BLE diagnostics.

These features must be added without compromising standard GATT compatibility.

---

## 19. Final Design Rule

The BLE Sensors subsystem is responsible for managing BLE sensor connections and converting standard GATT measurements into normalized application data.

It owns discovery, connection management, GATT operations, protocol decoding, and sensor state.

It publishes measurements and sensor status to the Data Hub.

It does not calculate cycling statistics, select global data sources, write activity files, or update the UI.

The BLE subsystem must remain modular, testable, and independent of the Cycling Core and UI architecture.