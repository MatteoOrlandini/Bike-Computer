# Data Contracts

## 1. Purpose

This document defines the data contracts shared by the Bike Computer firmware components.

Its purpose is to ensure that all components exchange measurements and status information consistently, with explicit units, timestamps, validity, freshness, and source information.

The contracts cover:

- GNSS measurements.
- BLE sensor measurements.
- Unified Data Hub.
- Cycling Core.
- Activity Recorder.
- Connectivity services.
- UI and dashboard components.

These definitions describe the target architecture. Existing implementations must be inspected before assuming that any contract is already implemented.

---

## 2. General Principles

All data exchanged between components must follow these principles:

1. Every measurement must have an explicit unit.
2. Every measurement must have an associated timestamp.
3. Measurement validity must be explicit.
4. Data freshness must be evaluated independently for each metric.
5. Every measurement must identify its source.
6. Missing data must never be represented by an arbitrary numeric value.
7. Stale measurements must not be presented as current measurements.
8. Components must not modify data owned by other components.
9. Unit conversions must happen at clearly defined boundaries.
10. Data structures must remain independent of LVGL and UI implementation details.

### 2.1 No Magic Values

The following representations must not be used to indicate missing data:

- `0.0` for unavailable speed.
- `0.0` for unavailable altitude.
- `0` for unavailable heart rate.
- `0` for unavailable cadence.
- `0` for unavailable power.
- `0.0` for unavailable GPS coordinates.

A value of zero can be a legitimate measurement.

Use an explicit validity or status field instead.

---

## 3. Common Data Types

The following definitions are conceptual C interfaces. They are proposals for the target architecture, not declarations of existing firmware types.

### 3.1 Measurement Status

```c
typedef enum {
    MEASUREMENT_STATUS_UNAVAILABLE = 0,
    MEASUREMENT_STATUS_VALID,
    MEASUREMENT_STATUS_INVALID,
    MEASUREMENT_STATUS_STALE,
    MEASUREMENT_STATUS_ESTIMATED
} measurement_status_t;
```

Definitions:

| Status | Meaning |
|---|---|
| UNAVAILABLE | No measurement is currently available |
| VALID | A valid and sufficiently fresh measurement is available |
| INVALID | A measurement was received but failed validation |
| STALE | A previously valid measurement is too old |
| ESTIMATED | A value was calculated or estimated rather than directly measured |

An estimated value must not be silently presented as a directly measured value.

### 3.2 Measurement Source

```c
typedef enum {
    DATA_SOURCE_NONE = 0,
    DATA_SOURCE_GNSS_UART,
    DATA_SOURCE_BLE_WHEEL,
    DATA_SOURCE_BLE_CADENCE,
    DATA_SOURCE_BLE_HEART_RATE,
    DATA_SOURCE_BLE_POWER,
    DATA_SOURCE_PHONE_GPS,
    DATA_SOURCE_BAROMETER,
    DATA_SOURCE_INTERNAL
} data_source_t;
```

The source identifies where a measurement originated.

`DATA_SOURCE_INTERNAL` is intended for values calculated by firmware components, such as derived cycling metrics.

Additional sources may be introduced when new hardware or integrations are added.

### 3.3 Measurement Quality

```c
typedef enum {
    DATA_QUALITY_UNKNOWN = 0,
    DATA_QUALITY_LOW,
    DATA_QUALITY_MEDIUM,
    DATA_QUALITY_HIGH
} data_quality_t;
```

Quality describes the confidence or reliability of a measurement.

Quality must not replace validity.

For example, a GNSS fix can be valid but have low quality.

### 3.4 Time Representation

Two different time references must be supported.

**Monotonic time**

Used for:

- Measurement age.
- Timeouts.
- Sensor freshness.
- Task scheduling.
- Elapsed activity time.
- Duration calculations.

Recommended representation:

```c
typedef uint64_t monotonic_time_ms_t;
```

Monotonic time must never move backwards during normal operation.

**UTC time**

Used for:

- Activity timestamps.
- GPX/FIT export.
- Synchronization with external services.
- Recording absolute measurement times.

Recommended representation:

```c
typedef int64_t utc_time_ms_t;
```

UTC time may be unavailable before GNSS time synchronization or another trusted time source becomes available.

Never use UTC timestamps to calculate short-duration intervals when monotonic time is available.

---

## 4. Generic Measurement Contract

A measurement should carry its value and associated metadata.

Conceptual example:

```c
typedef struct {
    measurement_status_t status;
    data_source_t source;
    data_quality_t quality;

    monotonic_time_ms_t measured_at_ms;
    monotonic_time_ms_t received_at_ms;

} measurement_metadata_t;
```

Individual measurements should use type-appropriate value structures.

For example:

```c
typedef struct {
    measurement_metadata_t metadata;
    float value_mps;
} speed_measurement_t;

typedef struct {
    measurement_metadata_t metadata;
    float value_bpm;
} heart_rate_measurement_t;

typedef struct {
    measurement_metadata_t metadata;
    float value_rpm;
} cadence_measurement_t;

typedef struct {
    measurement_metadata_t metadata;
    float value_watts;
} power_measurement_t;
```

This approach provides:

- Explicit units.
- Consistent metadata.
- Better type safety.
- Easier validation.
- Clearer component interfaces.

Avoid using a generic floating-point structure for every metric when dedicated types improve clarity.

---

## 5. GNSS Data Contract

The GNSS component is responsible for receiving and decoding GNSS information.

It must not directly update the UI or Cycling Core internal state.

### 5.1 Position

```c
typedef struct {
    measurement_metadata_t metadata;

    double latitude_deg;
    double longitude_deg;

    float altitude_m;
    float horizontal_accuracy_m;

} position_measurement_t;
```

Requirements:

- Latitude must be expressed in decimal degrees.
- Longitude must be expressed in decimal degrees.
- Altitude must be expressed in meters.
- Coordinates must be validated before publication.
- Invalid coordinates must not be published as valid positions.
- Horizontal accuracy must be marked unavailable if the receiver does not provide a reliable value.

Coordinate ranges:

- Latitude: -90 to +90 degrees.
- Longitude: -180 to +180 degrees.

Altitude must not be assumed valid simply because latitude and longitude are valid.

### 5.2 GNSS Fix Information

```c
typedef enum {
    GNSS_FIX_NONE = 0,
    GNSS_FIX_2D,
    GNSS_FIX_3D,
    GNSS_FIX_TIME_ONLY
} gnss_fix_type_t;

typedef struct {
    gnss_fix_type_t fix_type;

    uint8_t satellites_used;
    float hdop;

    bool position_valid;
    bool altitude_valid;
    bool time_valid;

} gnss_fix_info_t;
```

The GNSS component must distinguish:

- Receiver communication availability.
- Valid satellite fix.
- Valid position.
- Valid altitude.
- Valid UTC time.

A working UART connection does not imply a valid GNSS fix.

### 5.3 GNSS Speed

```c
typedef struct {
    measurement_metadata_t metadata;

    float speed_mps;
    float course_deg;

    bool course_valid;

} gnss_motion_measurement_t;
```

Speed must be normalized to meters per second.

Course must be expressed in degrees and normalized to the range [0, 360).

A stationary receiver may legitimately report zero speed.

### 5.4 GNSS Responsibilities

The GNSS component must:

- Read data from the configured UART.
- Validate incoming messages.
- Decode supported NMEA and/or UBX messages.
- Normalize measurements and units.
- Attach timestamps and quality information.
- Publish measurements through the Data Hub interface.
- Report receiver and fix status.

The GNSS component must not:

- Calculate cycling distance.
- Calculate average speed.
- Update dashboard widgets.
- Select the active speed source.
- Write activity files.

---

## 6. BLE Sensor Data Contract

The BLE Sensors component is responsible for discovering, connecting to, monitoring, and decoding supported BLE sensors.

### 6.1 Supported Sensor Types

```c
typedef enum {
    BLE_SENSOR_UNKNOWN = 0,
    BLE_SENSOR_SPEED_CADENCE,
    BLE_SENSOR_CADENCE,
    BLE_SENSOR_HEART_RATE,
    BLE_SENSOR_CYCLING_POWER
} ble_sensor_type_t;
```

Supported standard Bluetooth services:

| Sensor | Bluetooth Service UUID |
|---|---|
| Heart Rate | `0x180D` |
| Cycling Speed and Cadence | `0x1816` |
| Cycling Power | `0x1818` |
| Battery | `0x180F` |
| Device Information | `0x180A` |

The implementation must verify actual advertised services and characteristics.

Do not assume every sensor exposes all optional characteristics.

### 6.2 BLE Sensor State

```c
typedef enum {
    BLE_CONNECTION_DISCONNECTED = 0,
    BLE_CONNECTION_CONNECTING,
    BLE_CONNECTION_CONNECTED,
    BLE_CONNECTION_DISCOVERING,
    BLE_CONNECTION_READY,
    BLE_CONNECTION_ERROR
} ble_connection_state_t;

typedef struct {
    ble_sensor_type_t type;
    ble_connection_state_t connection_state;

    uint8_t battery_percent;
    bool battery_valid;

    uint8_t address[6];

} ble_sensor_info_t;
```

Connection state and measurement validity are separate concepts.

A connected sensor may not yet have produced a valid measurement.

A disconnected sensor may have previously produced measurements, but those measurements must eventually become stale.

### 6.3 Cadence

```c
typedef struct {
    measurement_metadata_t metadata;
    float cadence_rpm;
} cadence_measurement_t;
```

Unit: revolutions per minute (RPM).

Cadence must be validated according to the capabilities of the sensor and the application configuration.

### 6.4 Heart Rate

```c
typedef struct {
    measurement_metadata_t metadata;
    uint16_t heart_rate_bpm;
} heart_rate_measurement_t;
```

Unit: beats per minute (BPM).

The implementation must correctly decode the Heart Rate Measurement characteristic, including its format flags.

### 6.5 Cycling Power

```c
typedef struct {
    measurement_metadata_t metadata;
    int16_t power_watts;
} power_measurement_t;
```

Unit: watts.

The implementation must correctly decode the Cycling Power Measurement characteristic.

Additional fields, such as cadence-related information or pedal balance, may be added when supported and required.

### 6.6 Wheel Speed

Wheel speed should be derived from wheel sensor measurements when available.

For a standard Cycling Speed and Cadence sensor, the implementation must correctly handle:

- Cumulative wheel revolutions.
- Event time.
- Counter rollover.
- Wheel circumference configuration.
- Invalid or missing event intervals.

Wheel circumference must be configurable.

The BLE component should publish normalized wheel speed and, where useful, raw wheel event information.

The Data Hub and Cycling Core must not need to decode the BLE protocol themselves.

---

## 7. Unified Data Hub Contract

The Data Hub is the central interface for exchanging normalized measurements.

It owns the current measurement state and manages source selection for metrics that can have multiple sources.

### 7.1 Responsibilities

The Data Hub must:

- Receive measurements from hardware and protocol components.
- Validate basic measurement metadata.
- Store the latest measurement for each metric.
- Track measurement freshness.
- Select the active source according to configured policies.
- Expose consistent snapshots to consumers.
- Notify subscribers when relevant data changes.
- Report source transitions.
- Prevent stale data from being treated as current.

The Data Hub must not:

- Decode UART protocols.
- Decode BLE characteristics.
- Calculate cycling distance.
- Calculate activity statistics.
- Contain LVGL dependencies.

### 7.2 Conceptual Metric Identifiers

```c
typedef enum {
    DATA_METRIC_POSITION = 0,
    DATA_METRIC_SPEED,
    DATA_METRIC_ALTITUDE,
    DATA_METRIC_CADENCE,
    DATA_METRIC_HEART_RATE,
    DATA_METRIC_POWER,
    DATA_METRIC_TEMPERATURE,
    DATA_METRIC_BATTERY
} data_metric_id_t;
```

Metric identifiers must remain stable once adopted by the project.

### 7.3 Conceptual API

The following API illustrates the intended responsibilities.

It is not a requirement to adopt these exact function names or signatures.

```c
esp_err_t data_hub_publish_speed(
    const speed_measurement_t *measurement
);

esp_err_t data_hub_publish_position(
    const position_measurement_t *measurement
);

esp_err_t data_hub_publish_heart_rate(
    const heart_rate_measurement_t *measurement
);

esp_err_t data_hub_publish_cadence(
    const cadence_measurement_t *measurement
);

esp_err_t data_hub_publish_power(
    const power_measurement_t *measurement
);

esp_err_t data_hub_get_snapshot(
    data_snapshot_t *snapshot
);
```

The final API may use generic publication functions, typed functions, or a combination of both.

### 7.4 Snapshot Consistency

Consumers should obtain a consistent snapshot rather than reading multiple independently changing global variables.

A snapshot should contain:

- Current position.
- Current speed.
- Current altitude.
- Current cadence.
- Current heart rate.
- Current power.
- GNSS fix information.
- Sensor connection states.
- Active source information.

The Data Hub must guarantee that snapshot access is thread-safe.

Consumers must not receive mutable pointers to internal Data Hub storage.

### 7.5 Freshness

Freshness must be evaluated independently for each metric.

For example:

- GNSS position may require a different timeout from heart rate.
- BLE cadence may have a different update interval from power.
- Battery information may remain useful for much longer than speed.

Freshness thresholds must be configurable or defined in a centralized metric policy.

Do not hardcode a single global timeout for all measurements.

A measurement must transition to STALE when its age exceeds the configured threshold.

The Data Hub must not silently reuse stale values as current values.

---

## 8. Source Selection Contract

Some metrics may be available from multiple sources.

The Data Hub is responsible for selecting the active source.

### 8.1 Initial Source Priorities

| Metric | Preferred Source | Fallback |
|---|---|---|
| Position | GNSS UART | Phone GPS (future) |
| Speed | BLE wheel sensor | GNSS |
| Altitude | GNSS | Barometer (future) |
| Cadence | BLE cadence sensor | Unavailable |
| Heart rate | BLE HR sensor | Unavailable |
| Power | BLE power meter | Unavailable |

These are initial project policies, not assumptions about currently connected hardware.

### 8.2 Selection Rules

The source-selection algorithm must:

1. Consider source availability.
2. Consider measurement validity.
3. Consider measurement freshness.
4. Consider source priority.
5. Avoid unnecessary source switching.
6. Apply hysteresis where appropriate.
7. Report source changes.

A preferred source must not remain active when its measurements become invalid or stale.

A fallback source should be selected only when it provides valid and sufficiently fresh measurements.

### 8.3 Anti-Flapping

Rapid switching between sources must be avoided.

The implementation should consider:

- Minimum source hold time.
- Recovery stability time.
- Configurable source priorities.
- Measurement freshness.
- Source availability transitions.

Thresholds must be selected according to the actual sensor update rates and tested behavior.

### 8.4 No Implicit Fusion

Measurements from different sources must not be averaged or combined automatically.

For example, BLE wheel speed and GNSS speed should not be averaged without a specifically designed and validated fusion algorithm.

Source selection and sensor fusion are different responsibilities.

---

## 9. Cycling Core Contract

The Cycling Core calculates cycling metrics independently of the UI.

It consumes normalized measurements from the Data Hub.

### 9.1 Responsibilities

The Cycling Core is responsible for:

- Distance calculation.
- Elapsed time.
- Moving time.
- Current speed.
- Average speed.
- Maximum speed.
- Current cadence.
- Average cadence, when implemented.
- Current heart rate.
- Average and maximum heart rate, when implemented.
- Current power.
- Average and maximum power, when implemented.
- Activity state management.

### 9.2 Activity State

```c
typedef enum {
    ACTIVITY_STATE_IDLE = 0,
    ACTIVITY_STATE_RECORDING,
    ACTIVITY_STATE_PAUSED,
    ACTIVITY_STATE_STOPPED
} activity_state_t;
```

The state machine must define valid transitions.

For example:

- IDLE -> RECORDING
- RECORDING -> PAUSED
- PAUSED -> RECORDING
- RECORDING -> STOPPED
- PAUSED -> STOPPED

Starting, pausing, resuming, and stopping an activity must have deterministic behavior.

### 9.3 Cycling Statistics

```c
typedef struct {
    float distance_m;

    float current_speed_mps;
    float average_speed_mps;
    float maximum_speed_mps;

    uint64_t elapsed_time_ms;
    uint64_t moving_time_ms;

    activity_state_t activity_state;

} cycling_statistics_t;
```

The actual implementation must also provide validity information for metrics that may be unavailable.

For example, average speed must not be presented as valid when no valid distance or elapsed time is available.

### 9.4 Distance Calculation

Distance calculation must use a clearly defined source policy.

Possible approaches:

- Wheel sensor cumulative revolutions.
- GNSS position deltas.
- A future validated sensor-fusion algorithm.

The implementation must avoid double-counting when the active source changes.

GPS position jumps, invalid fixes, and stale coordinates must not generate artificial distance.

### 9.5 Average Speed

The project must distinguish:

- Average speed over elapsed activity time.
- Average speed over moving time.

The UI must identify which definition is displayed.

### 9.6 Independence from UI

The Cycling Core must not:

- Include LVGL headers.
- Update UI widgets.
- Access display drivers.
- Read BLE characteristics directly.
- Read GNSS UART data directly.

It must expose a stable statistics interface for the UI and Activity Recorder.

---

## 10. Activity Recorder Contract

The Activity Recorder stores activity data independently of connectivity.

The design must follow an offline-first approach.

### 10.1 Responsibilities

The Activity Recorder must:

- Start and stop activity recording.
- Receive normalized data samples.
- Store activity data locally.
- Preserve measurement timestamps.
- Handle interruptions and storage errors.
- Support activity retrieval.
- Prepare recorded activities for FIT and GPX export.

### 10.2 Activity Sample

A conceptual sample may contain:

```c
typedef struct {
    utc_time_ms_t timestamp_utc_ms;
    monotonic_time_ms_t timestamp_monotonic_ms;

    position_measurement_t position;
    speed_measurement_t speed;

    cadence_measurement_t cadence;
    heart_rate_measurement_t heart_rate;
    power_measurement_t power;

} activity_sample_t;
```

The final structure should be optimized for storage requirements and recording frequency.

Do not duplicate unnecessary metadata if it can be represented efficiently without losing information.

### 10.3 Timestamp Requirements

Each recorded sample should preserve:

- Measurement time.
- UTC timestamp, when available.
- Monotonic timestamp.

If UTC is unavailable, the recorder must preserve monotonic timing and handle UTC reconstruction explicitly when a trustworthy time reference becomes available.

Never invent absolute timestamps.

### 10.4 Storage Requirements

The recorder must:

- Handle insufficient storage.
- Detect write failures.
- Avoid corrupting previously recorded activities.
- Preserve data across normal reboots.
- Use a defined file format and version.
- Support recovery from interrupted recordings where feasible.

FIT is the primary target for activity export.

GPX is a complementary format for route and track exchange.

---

## 11. Connectivity Contract

Connectivity components must consume data through stable application interfaces.

They must not directly access internal GNSS, BLE, or Cycling Core structures.

### 11.1 Responsibilities

Connectivity may provide:

- Wi-Fi connection management.
- Phone communication.
- Activity synchronization.
- Strava integration.
- Configuration synchronization.
- Future phone GPS fallback.

### 11.2 Strava Integration

The firmware must not contain a Strava client secret.

Authentication and token management must follow the selected security architecture.

Activity upload must not interfere with local activity recording.

Network failures must not cause recorded activity data to be lost.

### 11.3 Phone GPS

Phone GPS is a future source.

The communication protocol must explicitly define:

- Position format.
- Speed format.
- Timestamp semantics.
- Accuracy information.
- Connection state.
- Data freshness.
- Source identity.

Phone GPS must be treated as a separate source, not as a replacement for the GNSS UART driver.

---

## 12. UI Contract

The UI consumes read-only snapshots from the Data Hub and Cycling Core.

It must not communicate directly with hardware drivers.

### 12.1 UI Responsibilities

The UI is responsible for:

- Displaying current measurements.
- Displaying measurement availability.
- Displaying sensor connection status.
- Displaying GNSS fix status.
- Displaying activity statistics.
- Displaying stale or unavailable data appropriately.
- Handling user configuration.

### 12.2 Display Rules

The UI must distinguish:

- Valid measurement.
- Estimated measurement.
- Unavailable measurement.
- Stale measurement.
- Invalid measurement.

Do not display a stale measurement as if it were live.

For unavailable data, use an explicit visual representation such as `--` rather than displaying zero.

### 12.3 LVGL Isolation

LVGL-specific structures and APIs must remain inside the UI layer.

The Data Hub and Cycling Core must not depend on LVGL.

UI updates must respect the project's LVGL synchronization mechanism.

---

## 13. Concurrency and Ownership

The firmware uses FreeRTOS, so concurrent access must be explicitly managed.

### 13.1 Ownership

Each component owns its internal state.

Other components must access that state through defined interfaces.

Examples:

- GNSS owns GNSS receiver state.
- BLE Sensors owns BLE connection and decoding state.
- Data Hub owns normalized measurement snapshots and source selection.
- Cycling Core owns cycling calculations.
- Activity Recorder owns recording and storage state.
- UI owns presentation state.

### 13.2 Thread Safety

Shared data must be protected using an appropriate synchronization mechanism.

Possible mechanisms include:

- FreeRTOS mutexes.
- FreeRTOS queues.
- Event groups.
- Task notifications.
- Carefully designed lock-free structures where justified.

The selected mechanism must match the access pattern.

### 13.3 Snapshot Access

Snapshot retrieval must not expose partially updated measurements.

A consumer must either receive a consistent snapshot or an explicit error.

### 13.4 Callback Restrictions

Callbacks executed in BLE or other time-sensitive contexts must remain short.

They must not:

- Perform blocking storage operations.
- Execute lengthy calculations.
- Update LVGL directly.
- Wait indefinitely for another task.

Where necessary, callbacks should forward data to a dedicated task using queues or notifications.

### 13.5 Pointer Lifetime

Interfaces must document pointer ownership and lifetime.

Do not return pointers to temporary buffers or internal mutable structures that may change immediately after the function returns.

---

## 14. Validation and Error Handling

Every component must validate incoming data before publishing or consuming it.

Validation should include:

- Numeric range checks.
- Finite floating-point checks.
- Timestamp consistency.
- Measurement freshness.
- Protocol-level validation.
- Source availability.
- Counter rollover handling where applicable.

For floating-point values, reject NaN and infinity when the metric requires a finite number.

Errors should be reported through the project's standard ESP-IDF error-handling conventions.

Invalid measurements must not overwrite valid measurements without an explicit state transition.

The system should expose diagnostics sufficient to identify:

- Missing GNSS data.
- Invalid GNSS fixes.
- BLE disconnections.
- Stale measurements.
- Source transitions.
- Data Hub publication failures.
- Storage failures.

---

## 15. Unit Conventions

The internal firmware representation must use consistent SI units wherever practical.

| Metric | Internal Unit |
|---|---|
| Distance | meters (m) |
| Speed | meters per second (m/s) |
| Altitude | meters (m) |
| Latitude | decimal degrees |
| Longitude | decimal degrees |
| Course | degrees |
| Cadence | revolutions per minute (RPM) |
| Heart rate | beats per minute (BPM) |
| Power | watts (W) |
| Temperature | degrees Celsius (°C) |
| Time intervals | milliseconds (ms) |
| Battery level | percent (%) |

Conversions for presentation must happen in the UI layer.

For example:

- m/s to km/h.
- m/s to mph.
- meters to kilometers.
- meters to feet.

The underlying data must remain in its canonical unit.

---

## 16. Configuration Contract

Configuration values that affect data interpretation must have a single authoritative owner.

Examples:

- Wheel circumference.
- Metric or imperial display preference.
- GNSS update configuration.
- Sensor source priorities.
- Freshness thresholds.
- Moving-speed threshold.
- Recording frequency.

Configuration must be validated before being applied.

Changes to configuration must not silently corrupt an active recording or produce inconsistent statistics.

The implementation must define when configuration changes take effect.

---

## 17. Contract Versioning

Data contracts are shared interfaces and must be treated as architectural dependencies.

When changing a contract:

1. Identify all affected components.
2. Identify all producers and consumers.
3. Determine whether the change is backward compatible.
4. Update the relevant interface documentation.
5. Update affected tests.
6. Review memory and performance implications.
7. Obtain the required approval before implementation.

Avoid unnecessary interface changes.

Prefer additive changes when backward compatibility is important.

---

## 18. Testing Requirements

Every contract must have corresponding tests.

### 18.1 Measurement Tests

Test:

- Valid measurements.
- Invalid measurements.
- Unavailable measurements.
- Stale measurements.
- Estimated measurements.
- Timestamp handling.
- Numeric range validation.

### 18.2 Data Hub Tests

Test:

- Publication from multiple sources.
- Snapshot consistency.
- Freshness transitions.
- Source selection.
- Source fallback.
- Source recovery.
- Anti-flapping behavior.
- Concurrent publication and retrieval.

### 18.3 Cycling Core Tests

Test:

- Distance accumulation.
- GNSS position jumps.
- Wheel counter rollover.
- Source switching.
- Elapsed time.
- Moving time.
- Average speed.
- Maximum speed.
- Activity state transitions.

### 18.4 Activity Recorder Tests

Test:

- Start and stop.
- Pause and resume.
- Missing UTC time.
- Storage errors.
- Interrupted recordings.
- Sample consistency.
- Export compatibility.

### 18.5 Integration Tests

Verify that:

- GNSS measurements reach the Data Hub.
- BLE measurements reach the Data Hub.
- Cycling Core consumes normalized measurements.
- UI consumes snapshots without accessing hardware.
- Activity Recorder stores consistent samples.

---

## 19. Implementation Rules for AI Agents

All agents working on the Bike Computer project must respect these contracts.

Before modifying an interface:

1. Read the existing implementation.
2. Identify the current producers and consumers.
3. Compare the implementation with this document.
4. Identify required changes.
5. Explain compatibility risks.
6. Propose the smallest safe modification.
7. Wait for explicit approval before modifying files.

Agents must not:

- Assume conceptual structures already exist.
- Introduce duplicate sources of truth.
- Bypass the Data Hub.
- Add direct hardware access to the UI.
- Introduce implicit unit conversions.
- Treat missing data as zero.
- Modify multiple components without analyzing their dependencies.

The Orchestrator coordinates interface changes and ensures that all affected agents use the same contract definitions.

---

## 20. Final Architectural Rule

The Data Hub is the single normalized measurement interface of the Bike Computer.

Hardware components provide measurements.

The Data Hub validates, tracks, and selects them.

The Cycling Core calculates cycling metrics.

The Activity Recorder stores activity data.

The Connectivity layer synchronizes information.

The UI presents the resulting state.

No component should bypass these responsibilities without an explicitly documented architectural decision.