# GNSS Specification

## 1. Purpose

This document defines the requirements, architecture, interfaces, and operational behavior of the GNSS subsystem for the Bike Computer.

The initial GNSS hardware is:

- Receiver: FlyFishRC M10 Mini.
- GNSS generation: u-blox M10.
- Host MCU: ESP32-S3.
- Communication interface: UART.
- UART RX: GPIO43.
- UART TX: GPIO44.
- Firmware framework: ESP-IDF 5.3.0.

The GNSS subsystem must provide reliable positioning, speed, altitude, course, satellite information, and time data to the Unified Data Hub.

This document describes the target implementation. Actual receiver configuration, firmware capabilities, baud rate, and supported protocols must be verified against the physical module before implementation.

---

## 2. Hardware Configuration

### 2.1 Receiver

The initial receiver is the FlyFishRC M10 Mini, based on the u-blox M10 generation.

The exact receiver variant, firmware version, and enabled features must be identified before sending configuration commands.

Do not assume that all u-blox M10 features are available on this specific module.

### 2.2 UART Connection

Current project configuration:

| Signal | ESP32-S3 GPIO |
|---|---:|
| GNSS TX -> MCU RX | GPIO43 |
| MCU TX -> GNSS RX | GPIO44 |

The user has modified the Waveshare board's RS485 circuitry/path to make these GPIOs available for GNSS UART communication.

Important constraints:

- Do not restore RS485 functionality.
- Do not assume the original RS485 signal topology.
- Do not assume the electrical levels without verification.
- Do not modify GPIO43 or GPIO44 assignments without explicit approval.
- Verify the physical connection before changing UART configuration.

The UART connection must use compatible logic levels.

### 2.3 UART Configuration

The existing firmware may use:

- UART2.
- TX GPIO44.
- RX GPIO43.
- Baud rate: 115200.

These settings must be verified against the actual hardware and receiver configuration.

The GNSS agent must not assume that 115200 baud is the receiver's current baud rate.

Before changing the configuration, inspect:

1. Existing UART initialization.
2. Existing receiver output.
3. Current receiver baud rate.
4. Enabled output protocols.
5. UART electrical configuration.

### 2.4 Hardware Ownership

The GNSS component owns the UART interface used by the receiver.

No other component may directly access the GNSS UART.

---

## 3. Software Architecture

The GNSS subsystem should be divided into the following logical layers:

1. UART transport.
2. Protocol framing and parsing.
3. Message decoding.
4. Measurement validation.
5. GNSS state management.
6. Data Hub publication.

Conceptual architecture:

```text
          FlyFishRC M10 Mini
                  |
                  | UART
                  |
          GNSS UART Transport
                  |
                  v
          Protocol Dispatcher
             /         \
            /           \
         NMEA            UBX
            \           /
             \         /
                  |
                  v
           Measurement Decoder
                  |
                  v
             Validation
                  |
                  v
            GNSS State
                  |
                  v
              Data Hub
```

The implementation may combine some of these layers if doing so improves maintainability without introducing tight coupling.

---

## 4. UART Transport

### 4.1 Responsibilities

The UART transport layer must:

- Initialize the configured UART peripheral.
- Configure RX and TX pins.
- Configure baud rate.
- Receive incoming bytes.
- Handle UART events and errors.
- Detect buffer overflow conditions.
- Provide a continuous byte stream to the protocol parser.

It must not decode NMEA or UBX messages directly.

### 4.2 ESP-IDF Integration

Use the ESP-IDF UART driver.

The implementation should consider:

- RX buffer sizing.
- UART event queue sizing.
- Receive timeouts.
- Buffer overflow handling.
- UART framing errors.
- Parity errors.
- Task stack usage.

Buffer sizes and task priorities must be selected according to the actual message rate and system load.

### 4.3 Continuous Reception

GNSS reception must not depend on the UI refresh rate.

The receiver must be read continuously and independently of LVGL.

The UART task must not perform lengthy calculations or storage operations.

### 4.4 Error Handling

The transport must expose relevant errors, including:

- UART initialization failure.
- Receive timeout.
- FIFO overflow.
- RX buffer overflow.
- Framing errors.
- Unexpected disconnection or lack of incoming data.

A UART error must not automatically be interpreted as loss of GNSS fix.

Communication status and positioning status must remain separate.

---

## 5. Supported Protocols

The GNSS subsystem should support NMEA and, where justified by the receiver configuration, UBX.

### 5.1 NMEA

NMEA is the initial protocol to support.

The parser should be designed to handle common GNSS sentences, including:

| Sentence | Purpose |
|---|---|
| GGA | Position, altitude, fix quality, satellites |
| RMC | Position, speed, course, UTC date/time |
| GSA | Fix mode and satellite information |
| GSV | Satellites in view |
| VTG | Course and ground speed |

Not every sentence is required for the initial implementation.

The implementation should first determine which messages the receiver actually outputs.

### 5.2 UBX

UBX is a binary protocol used by u-blox receivers.

Support may be added for messages that provide useful information not sufficiently available through NMEA.

Potential message families include:

- NAV-PVT.
- NAV-STATUS.
- NAV-SAT.
- MON-VER.

The exact messages and fields must be verified against the receiver's firmware and protocol documentation.

Do not implement speculative message decoding.

### 5.3 Protocol Dispatcher

The parser should identify incoming messages and route them to the appropriate decoder.

Requirements:

- Support fragmented messages across UART reads.
- Support multiple messages in a single UART read.
- Recover from corrupted messages.
- Validate message lengths.
- Validate checksums.
- Avoid unbounded memory usage.
- Avoid blocking indefinitely while waiting for a complete message.

Unknown messages must be ignored safely.

---

## 6. NMEA Parsing Requirements

### 6.1 General Parsing

The NMEA parser must:

- Identify sentence type.
- Validate the sentence structure.
- Validate checksum.
- Handle optional fields.
- Handle empty fields.
- Handle different talker identifiers.
- Handle malformed input.
- Avoid buffer overflows.

The parser must not assume that every sentence contains every optional field.

### 6.2 Coordinate Conversion

NMEA coordinates are commonly represented in degrees and minutes.

Convert them to decimal degrees.

For latitude:

```text
decimal_degrees = degrees + minutes / 60
```

For longitude:

```text
decimal_degrees = degrees + minutes / 60
```

Apply the hemisphere:

- N: positive latitude.
- S: negative latitude.
- E: positive longitude.
- W: negative longitude.

Validate the resulting coordinates before publication.

### 6.3 Speed Conversion

NMEA speed values may be expressed in knots.

The internal representation must use meters per second.

```text
speed_mps = speed_knots * 0.514444
```

Conversion to km/h must happen in the UI layer.

### 6.4 Course

Course over ground must be represented in degrees.

Normalize valid values to the range [0, 360).

Course must be marked unavailable when the receiver does not provide a valid value.

Do not interpret a missing course as zero degrees.

### 6.5 UTC Time

UTC time must be decoded independently from local system time.

The parser must handle date and time fields correctly.

UTC timestamps must not be considered trustworthy merely because a sentence contains a time field.

The GNSS fix and time validity information must be considered.

---

## 7. UBX Parsing Requirements

### 7.1 Frame Structure

The UBX parser must recognize the standard frame structure:

```text
SYNC CHAR 1
SYNC CHAR 2
CLASS
ID
LENGTH LSB
LENGTH MSB
PAYLOAD
CK_A
CK_B
```

The synchronization bytes are:

```text
0xB5 0x62
```

### 7.2 Checksum

Validate the UBX checksum using the algorithm defined by the u-blox protocol specification.

Reject frames with:

- Invalid synchronization.
- Invalid length.
- Invalid checksum.
- Unsupported message structure.

### 7.3 NAV-PVT

If supported by the actual receiver firmware, NAV-PVT may provide:

- UTC date and time.
- Fix type.
- Fix validity flags.
- Latitude.
- Longitude.
- Ellipsoid altitude.
- Mean sea level altitude.
- Horizontal accuracy.
- Vertical accuracy.
- Ground speed.
- Heading.
- Number of satellites.

The decoder must respect the exact field sizes, signedness, scaling factors, and validity flags defined by the applicable u-blox protocol version.

Do not assume that a NAV-PVT message is available simply because the receiver belongs to the M10 generation.

### 7.4 Unknown UBX Messages

Unknown messages must be ignored safely.

The parser must not interpret unknown payloads as known structures.

---

## 8. Measurement Model

The GNSS subsystem must expose normalized measurements.

### 8.1 Position

Internal representation:

- Latitude: decimal degrees.
- Longitude: decimal degrees.
- Horizontal accuracy: meters, if available.

Requirements:

- Validate coordinate ranges.
- Validate fix status.
- Preserve measurement timestamps.
- Do not publish invalid coordinates as valid positions.

### 8.2 Altitude

Internal representation:

- Meters.

The subsystem should distinguish altitude references when available:

- Ellipsoid height.
- Mean sea level height.

Do not silently mix these altitude references.

The selected altitude reference must be documented and consistent throughout the application.

### 8.3 Speed

Internal representation:

- Meters per second.

Speed must be marked unavailable if the receiver does not provide a valid measurement.

A zero speed value is valid.

### 8.4 Course

Internal representation:

- Degrees.

Course may be unreliable at very low speeds.

The subsystem should use a documented validity policy rather than presenting every receiver-reported course as reliable.

### 8.5 Satellite Information

Expose:

- Fix type.
- Satellites used.
- HDOP, when available.
- Position validity.
- Altitude validity.
- UTC time validity.

Do not confuse satellites in view with satellites used in the navigation solution.

---

## 9. GNSS Fix State

The GNSS subsystem must maintain a clear operational state.

Conceptual definition:

```c
typedef enum {
    GNSS_STATE_UNINITIALIZED = 0,
    GNSS_STATE_INITIALIZING,
    GNSS_STATE_NO_DATA,
    GNSS_STATE_SEARCHING,
    GNSS_STATE_FIX_2D,
    GNSS_STATE_FIX_3D,
    GNSS_STATE_DEGRADED,
    GNSS_STATE_ERROR
} gnss_state_t;
```

The exact state machine must distinguish:

- UART communication unavailable.
- Receiver communicating without a valid fix.
- Valid 2D fix.
- Valid 3D fix.
- Degraded positioning.
- Receiver or parser error.

A valid UART stream does not imply a valid position.

A previous fix must become stale after the configured timeout.

---

## 10. Measurement Freshness and Validation

The GNSS subsystem must attach measurement timestamps.

Use monotonic timestamps for freshness calculations.

Each measurement must be validated independently.

Examples:

- Position may be valid while altitude is unavailable.
- Speed may be valid while course is unavailable.
- UTC time may be valid while position is unavailable.

The GNSS subsystem must not force all fields to share the same validity state.

### 10.1 Freshness

Freshness thresholds must account for:

- Receiver update frequency.
- Message output configuration.
- UART transmission rate.
- Parser scheduling.
- Temporary communication interruptions.

Thresholds must be configurable or centralized.

Do not introduce arbitrary timeout values without considering the actual receiver output rate.

### 10.2 Invalid Measurements

Reject measurements containing:

- Invalid coordinates.
- NaN or infinity.
- Impossible protocol field values.
- Invalid fix status.
- Corrupted or incomplete messages.

Do not overwrite valid data with malformed data.

Invalid input may update diagnostic counters and communication status.

---

## 11. Receiver Configuration

Receiver configuration must be handled carefully.

### 11.1 Initial Configuration Policy

Before changing receiver configuration:

1. Inspect the current firmware.
2. Identify the receiver variant.
3. Determine the current baud rate.
4. Determine enabled protocols.
5. Determine the current output messages.
6. Verify the applicable receiver documentation.

The first implementation should preferably work with the receiver's existing output configuration.

### 11.2 Configuration Changes

Changes to receiver configuration may include:

- UART baud rate.
- NMEA output messages.
- UBX output messages.
- GNSS constellations.
- Navigation update rate.
- Power-saving configuration.

Such changes must be justified by actual requirements.

Do not change multiple receiver settings simultaneously without a defined verification procedure.

### 11.3 Configuration Persistence

If configuration persistence is supported, its behavior must be verified against the exact receiver model and firmware.

Do not assume that configuration changes persist after power cycling.

---

## 12. AssistNow and A-GNSS

Assisted GNSS may improve time-to-first-fix under suitable conditions.

However, support must not be assumed.

Before implementing AssistNow or another A-GNSS mechanism, verify:

1. Exact receiver model.
2. Receiver firmware version.
3. Supported assistance protocol.
4. Required message types.
5. Required credentials or service registration.
6. Data validity period.
7. Storage requirements.
8. Network requirements.
9. Licensing and service conditions.

Do not send undocumented or speculative UBX commands.

A-GNSS must be treated as a separate enhancement, not a requirement for basic GNSS functionality.

The receiver must remain usable without assistance data.

---

## 13. Data Hub Integration

The GNSS subsystem must publish normalized measurements through the Data Hub.

It must not expose UART buffers or parser internals to other components.

Measurements to publish:

- Position.
- Speed.
- Altitude.
- Course.
- GNSS fix information.
- UTC time information.

Each published measurement must contain:

- Value.
- Unit.
- Status.
- Source.
- Quality, when available.
- Measurement timestamp.
- Reception timestamp.

The source identifier for UART GNSS measurements should be `DATA_SOURCE_GNSS_UART`.

The GNSS subsystem must not select the final application speed or position source.

Source selection belongs to the Data Hub.

---

## 14. FreeRTOS Task Design

GNSS reception must be independent of UI rendering.

A dedicated task or an equivalent non-blocking architecture should handle incoming UART data.

The implementation should consider:

- Task priority.
- Stack size.
- UART event processing.
- Parser execution time.
- Buffer management.
- Inter-task communication.
- Error recovery.

The final task configuration must be based on actual runtime measurements.

Do not increase stack sizes arbitrarily.

Use stack high-water-mark diagnostics when validating task memory requirements.

The GNSS task must not call LVGL functions.

---

## 15. Diagnostics

The GNSS subsystem should expose diagnostic information useful for debugging.

Suggested diagnostics:

- UART bytes received.
- NMEA messages received.
- UBX messages received.
- Invalid checksum count.
- Malformed message count.
- UART overflow count.
- Last valid message timestamp.
- Last valid position timestamp.
- Current baud rate.
- Current fix state.
- Satellites used.
- Last parser error.

Diagnostics should be accessible through logging or a dedicated diagnostic interface.

Avoid excessive logging in high-frequency receive paths.

---

## 16. Performance and Reliability

The GNSS subsystem must:

- Process incoming data without blocking unrelated tasks.
- Avoid unnecessary dynamic memory allocation.
- Handle fragmented messages.
- Recover from corrupted input.
- Prevent buffer overflows.
- Maintain stable operation during long activities.
- Avoid unnecessary receiver reconfiguration.
- Avoid excessive CPU usage.

The implementation must be tested with:

- Continuous GNSS output.
- No-fix conditions.
- Weak signal conditions.
- Invalid NMEA sentences.
- Invalid UBX frames.
- UART interruptions.
- Receiver restart.
- Long-duration operation.

---

## 17. Testing Requirements

### 17.1 Parser Unit Tests

Test:

- Valid NMEA sentences.
- Invalid NMEA checksums.
- Empty optional fields.
- Different talker identifiers.
- Valid UBX frames.
- Invalid UBX checksums.
- Incomplete frames.
- Multiple frames in one buffer.
- Fragmented frames.
- Unknown message types.
- Buffer boundary conditions.

### 17.2 Measurement Tests

Test:

- Coordinate conversion.
- Hemisphere handling.
- Speed conversion.
- Course normalization.
- Altitude validity.
- Fix type decoding.
- Satellite count handling.
- UTC time decoding.
- Stale measurement transitions.

### 17.3 Integration Tests

Verify:

- UART reception.
- Protocol parsing.
- Measurement publication.
- Data Hub integration.
- GNSS state transitions.
- Recovery after receiver restart.

### 17.4 Hardware Tests

Hardware tests must be performed only after explicit approval.

They should verify:

- Physical UART connectivity.
- Electrical compatibility.
- Actual baud rate.
- Receiver output configuration.
- Position acquisition.
- Speed reporting.
- Long-duration stability.

---

## 18. Implementation Rules for AI Agents

Before modifying the GNSS subsystem, agents must:

1. Inspect the existing UART implementation.
2. Inspect GPIO configuration.
3. Identify existing parser code.
4. Identify current task architecture.
5. Determine the actual receiver output format.
6. Compare existing behavior with this specification.
7. Identify dependencies on the Data Hub.
8. Propose the smallest safe implementation.

Agents must not:

- Change GPIO43 or GPIO44 assignments without approval.
- Restore RS485 functionality.
- Assume the UART baud rate.
- Assume UBX output is enabled.
- Send undocumented receiver commands.
- Assume AssistNow support.
- Modify receiver configuration without approval.
- Update LVGL directly from the GNSS task.
- Implement source selection inside the GNSS driver.

---

## 19. Future Enhancements

Potential future enhancements include:

- Improved GNSS quality estimation.
- Additional u-blox navigation messages.
- Receiver configuration management.
- A-GNSS support, if verified.
- Phone GPS fallback.
- GNSS diagnostics dashboard.
- Position filtering.
- Improved course estimation.
- Low-power GNSS operation.

These enhancements must not compromise the reliability of basic positioning.

---

## 20. Final Design Rule

The GNSS subsystem is responsible for converting receiver communication into validated, normalized GNSS measurements.

It owns the UART transport, protocol parsing, decoding, and receiver state.

It publishes measurements to the Data Hub.

It does not calculate cycling statistics, select application-level measurement sources, write activity files, or update the UI.

The GNSS subsystem must remain independent, testable, and replaceable without requiring changes to the Cycling Core or UI architecture.