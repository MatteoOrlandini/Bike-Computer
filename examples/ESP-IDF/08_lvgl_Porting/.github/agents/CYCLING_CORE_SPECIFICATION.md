# Cycling Core Specification

## 1. Purpose

This document defines the architecture, responsibilities, algorithms, and interfaces of the Cycling Core subsystem.

The Cycling Core transforms normalized measurements from the Unified Data Hub into cycling metrics and activity statistics.

It is responsible for:

- Current speed.
- Average speed.
- Maximum speed.
- Distance.
- Elapsed time.
- Moving time.
- Cadence statistics.
- Heart rate statistics.
- Power statistics.
- Activity state management.
- Automatic pause and resume.
- Activity-related events.

The Cycling Core must remain independent of hardware drivers, BLE protocols, GNSS protocols, storage implementation, and LVGL.

---

## 2. Design Principles

The Cycling Core must follow these principles:

1. Consume normalized measurements only.
2. Never access hardware directly.
3. Never depend on LVGL.
4. Use monotonic time for duration calculations.
5. Distinguish unavailable measurements from valid zero values.
6. Avoid accumulating distance from stale or invalid data.
7. Prevent double-counting during source transitions.
8. Keep activity statistics consistent.
9. Make calculation policies explicit and configurable.
10. Keep calculations deterministic and testable.

The Cycling Core must not depend on whether a measurement comes from GNSS, BLE, or a future external source.

---

## 3. Architecture

Conceptual architecture:

```text
              Unified Data Hub
                     |
                     v
                Data Snapshot
                     |
                     v
                Cycling Core
                     |
          +----------+----------+
          |          |          |
          v          v          v
       Speed      Distance     Time
       Engine      Engine     Engine
          |          |          |
          +----------+----------+
                     |
                     v
              Statistics Engine
                     |
                     v
              Activity Manager
                     |
          +----------+----------+
          |                     |
          v                     v
    Activity Recorder          UI
```

The implementation may use fewer source files if responsibilities remain clearly separated.

---

## 4. Input Data

The Cycling Core consumes a consistent Data Hub snapshot.

Relevant input measurements include:

- Position.
- Speed.
- Altitude.
- Cadence.
- Heart rate.
- Power.
- Measurement validity.
- Measurement timestamps.
- Active source information.

The Cycling Core must evaluate the status and freshness of each metric independently.

A valid speed measurement does not imply a valid position measurement.

A valid heart rate measurement does not imply that the activity is recording.

---

## 5. Activity State Machine

### 5.1 States

```c
typedef enum {
    ACTIVITY_STATE_IDLE = 0,
    ACTIVITY_STATE_RECORDING,
    ACTIVITY_STATE_PAUSED,
    ACTIVITY_STATE_STOPPED
} activity_state_t;
```

### 5.2 State Definitions

| State | Description |
|---|---|
| IDLE | No active activity |
| RECORDING | Activity is running and accumulating statistics |
| PAUSED | Activity is temporarily paused |
| STOPPED | Activity has been stopped and is ready for finalization |

### 5.3 Valid Transitions

```text
              start
        IDLE ---------> RECORDING
                           |
                           | pause
                           v
                        PAUSED
                           |
                           | resume
                           v
                       RECORDING
                           |
                           | stop
                           v
                        STOPPED
```

Additional supported transitions:

- PAUSED -> STOPPED.
- STOPPED -> IDLE after activity finalization.
- STOPPED -> RECORDING only if explicitly designed as a new activity or continuation workflow.

Invalid transitions must return a clear error.

### 5.4 State Transition Rules

Starting an activity must initialize activity statistics.

Pausing an activity must stop elapsed recording accumulation according to the selected pause policy.

Resuming must continue the same activity without resetting previous statistics.

Stopping must finalize statistics and notify the Activity Recorder.

Resetting statistics must require an explicit application-level operation.

---

## 6. Time Management

### 6.1 Monotonic Time

All duration calculations must use monotonic time.

Do not use system wall-clock time for elapsed activity duration.

### 6.2 Elapsed Time

Elapsed time represents the duration during which the activity has been actively recording, excluding paused intervals.

Conceptually:

```text
elapsed_time =
    sum(recording_intervals)
```

The implementation must avoid double-counting time when pausing and resuming.

### 6.3 Moving Time

Moving time represents the portion of elapsed recording time during which the cyclist is considered to be moving.

It must be calculated using an explicit moving-state policy.

### 6.4 Moving Time and Elapsed Time

The following relationship must always hold:

```text
0 <= moving_time <= elapsed_time
```

### 6.5 Time Precision

Internally, durations should use milliseconds or a higher-resolution monotonic representation where appropriate.

Convert to seconds or formatted time only when exposing values to the UI or exporting activities.

---

## 7. Speed Calculation

### 7.1 Current Speed

Current speed is obtained from the active speed measurement selected by the Data Hub.

Possible sources:

- BLE wheel sensor.
- GNSS receiver.

The Cycling Core must not independently select between these sources.

The value must be expressed internally in meters per second.

### 7.2 Speed Validity

Current speed is valid only when:

- The measurement status is acceptable.
- The measurement is fresh.
- The value is finite.
- The value passes configured plausibility checks.

A valid zero speed must be preserved.

An unavailable speed must not be represented as zero.

### 7.3 Maximum Speed

Maximum speed is the highest valid speed observed during the activity.

The implementation should consider filtering isolated spikes.

The filtering policy must be explicit and tested.

Invalid measurements must never increase maximum speed.

### 7.4 Average Speed

Two different average speeds must be supported.

**Average speed over elapsed recording time:**

```text
average_speed =
    total_distance / elapsed_time
```

**Average speed over moving time:**

```text
moving_average_speed =
    total_distance / moving_time
```

Both values must be expressed in meters per second internally.

If the denominator is zero, the corresponding average is unavailable.

The UI must identify which average speed is displayed.

---

## 8. Distance Calculation

Distance is one of the most important Cycling Core responsibilities.

The implementation must avoid artificial distance caused by GPS noise, invalid fixes, source transitions, and counter rollover.

### 8.1 Preferred Distance Sources

Initial policy:

1. BLE wheel sensor, when valid and configured.
2. GNSS position, when the wheel sensor is unavailable.
3. Future validated sensor-fusion implementation.

The actual active source is selected by the Data Hub.

The Cycling Core must receive enough metadata to identify source transitions.

### 8.2 Wheel-Based Distance

For wheel sensors, distance can be derived from cumulative revolutions.

```text
distance_m =
    delta_revolutions * wheel_circumference_m
```

The implementation must:

- Handle counter rollover.
- Handle missing events.
- Avoid counting the initial counter value as distance.
- Validate implausible revolution changes.
- Avoid counting the same event twice.

If the Data Hub publishes normalized speed instead of cumulative wheel events, the Cycling Core must use a consistent integration policy.

### 8.3 GNSS-Based Distance

When GNSS is the active distance source, distance can be calculated from consecutive valid positions.

The implementation should use a suitable geodesic distance calculation, such as the Haversine formula.

Conceptually:

```text
delta_distance =
    haversine(previous_position, current_position)
```

Only valid, fresh, and temporally consistent positions may be used.

### 8.4 GNSS Noise Filtering

The implementation must consider:

- Horizontal accuracy.
- Position update interval.
- Minimum movement threshold.
- Maximum plausible speed.
- Position jumps.
- Invalid fixes.
- Temporary loss of signal.

Small position variations caused by stationary GNSS noise must not continuously increase distance.

Filtering thresholds must be configurable or derived from measurement quality.

Do not introduce arbitrary constants without documenting their rationale.

### 8.5 Source Transitions

When changing between wheel-based and GNSS-based distance:

- Do not count the same physical movement twice.
- Reset or reinitialize source-specific calculation baselines.
- Preserve accumulated activity distance.
- Avoid artificial distance jumps.
- Handle the transition as an explicit event.

The first measurement from a newly active source must establish a baseline where necessary.

It must not automatically contribute a distance increment.

### 8.6 Distance Integrity

Distance must be monotonic during a normal recording session.

The following must always hold:

```text
distance_m >= 0
```

A temporary sensor disconnection must not reset accumulated distance.

---

## 9. Automatic Pause and Resume

Automatic pause is an optional application feature.

It must be configurable by the user.

### 9.1 Configuration

Conceptual configuration:

```c
typedef struct {
    bool enabled;

    float pause_speed_threshold_mps;
    uint32_t pause_delay_ms;
    uint32_t resume_delay_ms;

} auto_pause_config_t;
```

The actual configuration may include additional filtering and hysteresis parameters.

### 9.2 Pause Conditions

Automatic pause may be triggered when:

- The activity is recording.
- A valid speed measurement is available.
- Speed remains below the configured threshold.
- The condition persists for the configured pause delay.

### 9.3 Resume Conditions

Automatic resume may be triggered when:

- The activity is paused automatically.
- A valid speed measurement is available.
- Speed remains above the configured resume threshold.
- The condition persists for the configured resume delay.

Use separate pause and resume thresholds where necessary to avoid rapid state changes.

### 9.4 Invalid Speed

Invalid or stale speed must not automatically be interpreted as zero.

If speed becomes unavailable, the automatic pause logic must follow an explicitly defined policy.

For example:

- Maintain the current activity state temporarily.
- Wait for valid speed again.
- Apply a configurable timeout before taking further action.

The system must not pause immediately because the GNSS signal was lost.

### 9.5 Manual Pause Priority

Manual pause must take priority over automatic resume.

Automatic logic must never resume an activity that the user explicitly paused.

The implementation must distinguish manual pause from automatic pause.

### 9.6 Automatic Pause Events

The Cycling Core should generate events indicating:

- Automatic pause.
- Automatic resume.
- Manual pause.
- Manual resume.

These events may be used by the UI and Activity Recorder.

---

## 10. Cadence Statistics

Cadence is received through the Data Hub.

The Cycling Core may calculate:

- Current cadence.
- Average cadence.
- Maximum cadence.

Cadence must be validated independently of speed.

A cadence measurement may remain valid when the bicycle is stationary.

Average cadence must have a clearly defined calculation policy.

Possible policies include:

- Average over all valid recording time.
- Average over time during which valid cadence is available.
- Average while moving.

The selected policy must be documented and consistently applied.

Missing cadence data must not be treated as zero RPM.

---

## 11. Heart Rate Statistics

The Cycling Core may calculate:

- Current heart rate.
- Average heart rate.
- Maximum heart rate.

Heart rate is expressed in BPM.

Only valid and fresh measurements should contribute to statistics.

Missing heart rate data must not be included as zero.

The average calculation must account for measurement availability.

If time-weighted averaging is implemented, the weighting interval must be explicitly defined.

---

## 12. Power Statistics

The Cycling Core may calculate:

- Current power.
- Average power.
- Maximum power.

Power is expressed in watts.

Only valid and fresh measurements should contribute to statistics.

Zero watts is a valid measurement.

Missing power data must not be treated as zero.

Future enhancements may include:

- Normalized Power.
- Intensity Factor.
- Training Stress Score.
- Power zones.
- Left/right balance.

These metrics must be introduced only with documented algorithms and validation tests.

---

## 13. Altitude and Elevation

The Cycling Core may use altitude measurements from the Data Hub.

Potential statistics include:

- Current altitude.
- Minimum altitude.
- Maximum altitude.
- Total ascent.
- Total descent.

Altitude measurements must have a consistent reference.

Ellipsoid altitude and mean sea level altitude must not be mixed without conversion.

### 13.1 Elevation Gain

Elevation gain should not be calculated by simply summing every positive altitude difference.

GNSS altitude noise can produce significant errors.

A future elevation algorithm should consider:

- Measurement quality.
- Filtering.
- Minimum altitude change.
- Vertical accuracy.
- Barometric sensor availability.

The algorithm must be tested using representative cycling data.

---

## 14. Statistics Data Structure

Conceptual structure:

```c
typedef struct {
    float distance_m;

    float current_speed_mps;
    float average_speed_mps;
    float moving_average_speed_mps;
    float maximum_speed_mps;

    float current_cadence_rpm;
    float average_cadence_rpm;
    float maximum_cadence_rpm;

    float current_heart_rate_bpm;
    float average_heart_rate_bpm;
    float maximum_heart_rate_bpm;

    float current_power_watts;
    float average_power_watts;
    float maximum_power_watts;

    uint64_t elapsed_time_ms;
    uint64_t moving_time_ms;

    activity_state_t activity_state;

} cycling_statistics_t;
```

This is an illustrative structure.

The final implementation must include explicit validity metadata for each metric that may be unavailable.

Do not assume that all fields are valid whenever the structure is initialized.

---

## 15. Activity Events

The Cycling Core should expose application-level events.

Potential events:

```c
typedef enum {
    CYCLING_EVENT_ACTIVITY_STARTED = 0,
    CYCLING_EVENT_ACTIVITY_PAUSED,
    CYCLING_EVENT_ACTIVITY_RESUMED,
    CYCLING_EVENT_ACTIVITY_STOPPED,
    CYCLING_EVENT_AUTO_PAUSED,
    CYCLING_EVENT_AUTO_RESUMED,
    CYCLING_EVENT_DISTANCE_SOURCE_CHANGED,
    CYCLING_EVENT_SENSOR_UNAVAILABLE
} cycling_event_type_t;
```

Events must contain sufficient information for consumers to understand the transition.

Event delivery must be thread-safe.

Consumers must not modify Cycling Core internal state.

---

## 16. Activity Recorder Integration

The Cycling Core must provide normalized statistics and relevant measurement data to the Activity Recorder.

The Activity Recorder owns persistent storage.

The Cycling Core must not directly manage files.

The interface should support:

- Activity start.
- Activity pause.
- Activity resume.
- Activity stop.
- Measurement samples.
- Activity statistics snapshots.
- Source transition events.

Recording must not depend on UI refresh frequency.

The Activity Recorder must be able to operate independently of display rendering.

---

## 17. UI Integration

The UI consumes read-only Cycling Core snapshots.

It must not calculate cycling statistics independently.

The UI may format values for display.

Examples:

- m/s to km/h.
- Meters to kilometers.
- Milliseconds to HH:MM:SS.

The UI must distinguish:

- Valid values.
- Estimated values.
- Unavailable values.
- Stale values.

The UI must not display unavailable values as zero.

---

## 18. Performance and Reliability

The Cycling Core must:

- Avoid unnecessary dynamic memory allocation.
- Use deterministic calculations.
- Avoid blocking operations.
- Handle missing measurements.
- Handle source transitions.
- Avoid distance discontinuities.
- Preserve accumulated statistics during sensor interruptions.
- Support long-duration activities.
- Use appropriate numeric precision.

64-bit accumulators should be used for long-duration counters and time values where appropriate.

Floating-point values must be checked for NaN and infinity before use.

---

## 19. Testing Requirements

### 19.1 Time Tests

Test:

- Activity start.
- Activity pause.
- Activity resume.
- Activity stop.
- Elapsed time.
- Moving time.
- Repeated pause and resume.
- Invalid state transitions.

### 19.2 Speed Tests

Test:

- Valid speed.
- Zero speed.
- Missing speed.
- Stale speed.
- Maximum speed.
- Invalid spikes.
- Source transitions.

### 19.3 Distance Tests

Test:

- Wheel-based distance.
- GNSS-based distance.
- Haversine calculations.
- Stationary GPS noise.
- GPS position jumps.
- Wheel counter rollover.
- Missing measurements.
- Source transitions.
- No double-counting.

### 19.4 Automatic Pause Tests

Test:

- Pause threshold.
- Pause delay.
- Resume threshold.
- Resume delay.
- Hysteresis.
- Invalid speed.
- Stale speed.
- Manual pause priority.
- GNSS signal loss.

### 19.5 Statistics Tests

Test:

- Average speed.
- Moving average speed.
- Maximum speed.
- Average cadence.
- Maximum cadence.
- Average heart rate.
- Maximum heart rate.
- Average power.
- Maximum power.
- Missing data handling.

### 19.6 Long-Duration Tests

Test:

- Long activities.
- Large distance accumulators.
- Time counter behavior.
- Repeated sensor disconnections.
- Source switching.
- Repeated automatic pause and resume.

---

## 20. Implementation Rules for AI Agents

Before modifying the Cycling Core, agents must:

1. Inspect existing trip-computer implementation.
2. Identify current calculations.
3. Inspect existing synchronization mechanisms.
4. Identify Data Hub interfaces.
5. Identify Activity Recorder interfaces.
6. Compare existing behavior with this specification.
7. Identify incorrect or incomplete calculations.
8. Propose a minimal and testable implementation.

Agents must not:

- Access GNSS UART directly.
- Access BLE characteristics directly.
- Update LVGL directly.
- Write activity files directly.
- Duplicate source-selection logic.
- Treat missing measurements as zero.
- Change activity statistics without updating tests.
- Introduce undocumented calculation policies.

---

## 21. Future Enhancements

Potential future features include:

- Advanced speed filtering.
- Improved GNSS distance estimation.
- Wheel and GNSS sensor fusion.
- Elevation gain algorithms.
- Power-based training metrics.
- Cycling performance zones.
- Automatic lap detection.
- Manual and automatic lap management.
- Climbing metrics.
- Training summaries.

Each enhancement must have a documented algorithm and corresponding tests.

---

## 22. Final Design Rule

The Cycling Core converts normalized measurements into reliable cycling statistics.

It owns calculation policies, activity state, distance accumulation, speed statistics, time statistics, and activity-related events.

It does not own hardware communication, BLE decoding, GNSS parsing, persistent storage, or UI rendering.

The Cycling Core must remain deterministic, independently testable, and completely decoupled from LVGL.