# ACTIVITY_RECORDER_SPECIFICATION.md

# Activity Recorder Specification

## 1. Purpose

The Activity Recorder is responsible for recording, storing, recovering, and exporting cycling activities.

It must support reliable offline recording, even when network connectivity is unavailable.

The recorder must prioritize data integrity and recovery over performance optimizations.

The initial implementation must be designed for a single active cycling activity.

## 2. Scope

The Activity Recorder is responsible for:

- Starting, pausing, resuming, and stopping activity recordings.
- Receiving measurements from the Data Hub and activity information from Cycling Core.
- Persisting measurements and activity events.
- Maintaining activity metadata.
- Recovering incomplete recordings after unexpected shutdowns.
- Managing stored activities.
- Exporting recorded activities into supported file formats.

The Activity Recorder is not responsible for:

- Reading GNSS hardware directly.
- Communicating with BLE sensors.
- Calculating cycling metrics.
- Rendering activity information in the UI.
- Managing network connectivity.
- Implementing Strava authentication or uploads.

These responsibilities belong to other components.

## 3. Architectural Position

The Activity Recorder sits between Cycling Core, the Data Hub, and persistent storage.

Conceptual data flow:

```text
GNSS / BLE Sensors
        |
        v
     Data Hub
        |
        v
   Cycling Core
        |
        v
 Activity Recorder
        |
        v
 Persistent Storage
        |
        v
 Export Pipeline
        |
        +------ FIT
        |
        +------ GPX
```

### 3.1 Component ownership

| Component | Responsibility |
|---|---|
| Data Hub | Provides normalized measurements and metadata |
| Cycling Core | Maintains activity state and calculates cycling metrics |
| Activity Recorder | Records activity data and manages persistent storage |
| Storage Backend | Provides access to persistent storage |
| Export Pipeline | Converts recorded activities into external formats |
| UI/UX | Requests operations and displays recording status |

The UI must never access activity files directly.

The Activity Recorder must not depend on LVGL.

## 4. Design Principles

The implementation must follow these principles:

1. **Offline first:** recording must work without Wi-Fi or Internet access.
2. **Data integrity:** incomplete writes must not silently corrupt existing activities.
3. **Crash recovery:** unexpected power loss must not automatically destroy an activity.
4. **Separation of concerns:** recording, calculation, storage, and export must remain independent.
5. **Explicit data validity:** missing or invalid measurements must not be represented as valid values.
6. **Low resource consumption:** storage operations must not block real-time sensor processing.
7. **Versioned formats:** internal storage formats must support future migrations.
8. **No silent data loss:** queue overflow, storage errors, and incomplete writes must be detectable.
9. **Explicit deletion:** recorded activities must never be deleted automatically without an approved retention policy.

## 5. Activity Lifecycle

The recorder must support the following lifecycle:

```text
IDLE
 |
 | start
 v
RECORDING
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
FINALIZING
 |
 | successful finalization
 v
COMPLETED
```

An interrupted recording may instead enter a recovery state during the next startup.

### 5.1 Start

When an activity starts:

- Generate a unique activity identifier.
- Create the activity metadata.
- Initialize recording timestamps.
- Create the internal activity file.
- Initialize recording statistics.
- Start accepting measurement samples.
- Persist an initial activity-start event.

The recorder must report success only after the initial activity state has been safely persisted.

### 5.2 Pause

When an activity is paused:

- Stop recording normal activity samples.
- Persist a pause event.
- Preserve accumulated activity data.
- Maintain the activity session state.

The recorder must distinguish between paused time and active recording time.

### 5.3 Resume

When an activity resumes:

- Persist a resume event.
- Continue recording using the existing activity identifier.
- Preserve previous measurements and events.
- Continue elapsed-time accounting correctly.

### 5.4 Stop

When an activity stops:

- Stop accepting new activity samples.
- Flush pending buffers.
- Persist the stop event.
- Finalize metadata and statistics.
- Verify the resulting internal file.
- Mark the activity as completed only after successful finalization.

If finalization fails, the activity must remain recoverable.

### 5.5 Automatic pause

Automatic pause is primarily a Cycling Core responsibility.

The recorder must receive explicit activity-state transitions rather than independently deciding whether the cyclist is moving.

The implementation must distinguish:

- Manual pause.
- Automatic pause.
- Automatic resume.
- Manual stop.

These events must remain identifiable in the recorded activity.

## 6. Timestamp Management

Accurate timing is essential for distance, speed, elapsed time, and sensor synchronization.

### 6.1 Monotonic timestamps

The recorder must use a monotonic time source for measuring elapsed durations.

Monotonic timestamps must not be affected by:

- GNSS time synchronization.
- RTC corrections.
- Manual clock adjustments.
- Time zone changes.

The monotonic clock is the reference for calculating recording intervals.

### 6.2 UTC timestamps

UTC timestamps should be stored when a reliable time source is available.

Possible sources include:

- GNSS time.
- RTC.
- Network time synchronization.

The implementation must track whether UTC time is valid and its source.

### 6.3 Time synchronization

If UTC becomes available or changes during an activity:

- Preserve the original monotonic timeline.
- Record the time synchronization event.
- Do not rewrite previous monotonic timestamps.
- Do not silently shift the activity timeline.

The recorder must never fabricate UTC timestamps when the clock is not valid.

### 6.4 Timestamp representation

Each sample should contain:

- Monotonic timestamp.
- Optional UTC timestamp.
- UTC validity information.

The exact internal representation must be selected during implementation.

## 7. Recorded Data Model

The internal data model must support multiple measurement types and future extensions.

### 7.1 Activity metadata

Each activity should contain:

| Field | Description |
|---|---|
| Activity ID | Unique identifier |
| Schema version | Internal format version |
| Activity type | Initially cycling |
| Start timestamp | Activity start time |
| End timestamp | Activity end time, if available |
| Duration | Total elapsed duration |
| Moving duration | Time spent moving |
| Distance | Accumulated distance |
| Average speed | Average speed according to documented calculation |
| Maximum speed | Maximum valid speed |
| Average heart rate | Average valid heart rate |
| Maximum heart rate | Maximum valid heart rate |
| Average cadence | Average valid cadence |
| Maximum cadence | Maximum valid cadence |
| Average power | Average valid power |
| Maximum power | Maximum valid power |
| Elevation gain | Accumulated elevation gain |
| Recording status | Recording, paused, completed, or recoverable |

Optional metrics must remain absent or explicitly invalid when unavailable.

### 7.2 Measurement samples

A measurement sample may contain:

| Field | Description |
|---|---|
| Timestamp | Monotonic timestamp |
| UTC timestamp | Optional UTC time |
| Position | Latitude and longitude |
| Altitude | Altitude in meters |
| Speed | Speed in m/s |
| Distance | Accumulated distance in meters |
| Heart rate | Beats per minute |
| Cadence | Revolutions per minute |
| Power | Watts |
| Temperature | Degrees Celsius, if available |
| Source metadata | Origin of the measurement |
| Quality metadata | Measurement quality |
| Validity metadata | Validity of individual fields |

The model must support partial samples.

For example, a sample may contain valid GNSS position and speed but no heart rate or power.

### 7.3 Per-field validity

Validity must be tracked independently for each measurement.

The following states should be supported:

- Valid.
- Invalid.
- Unavailable.
- Stale.
- Estimated, if estimation is explicitly supported.

A missing measurement must not be replaced with zero.

For example:

- Missing heart rate is not 0 bpm.
- Missing power is not 0 W.
- Invalid position must not be exported as coordinates `(0, 0)`.

### 7.4 Source and quality metadata

Where available, record:

- Measurement source.
- Measurement timestamp.
- Data freshness.
- Quality indicator.
- Estimated or directly measured status.

This information is important for debugging and future analysis.

## 8. Recording Policy

The recorder must not blindly duplicate every update received from the Data Hub.

### 8.1 Sampling strategy

The implementation should support:

- Recording on measurement updates.
- Periodic snapshots.
- Event-driven records.
- Configurable recording intervals.

The initial sampling policy must balance:

- Data resolution.
- Storage consumption.
- CPU usage.
- Write frequency.
- Battery consumption.

The selected sampling policy must be documented and configurable where appropriate.

### 8.2 Duplicate and stale measurements

The recorder must avoid storing repeated measurements as new fresh samples when the underlying measurement has not changed.

Stale values may be retained for diagnostic purposes if explicitly marked as stale, but must not be interpreted as new measurements.

### 8.3 Measurement aggregation

The recorder must preserve enough information to reconstruct the activity timeline.

Any aggregation or downsampling must be explicitly designed.

Do not discard important sensor events or position changes merely to reduce storage consumption.

### 8.4 Distance and speed

The recorder should store both:

- Valid speed measurements.
- Accumulated distance provided by Cycling Core.

The recorder must not independently calculate distance unless a separate, explicitly approved requirement is introduced.

## 9. Activity Events and Laps

The internal format must support activity events.

Initial event types:

- Activity started.
- Activity paused.
- Activity resumed.
- Activity stopped.
- Automatic pause.
- Automatic resume.
- Lap started.
- Lap completed.
- GPS fix acquired.
- GPS fix lost.
- Sensor connected.
- Sensor disconnected.
- Time synchronized.
- Storage warning.
- Recording recovery.

Each event should contain:

- Event type.
- Monotonic timestamp.
- Optional UTC timestamp.
- Optional event parameters.

Lap support should include:

- Lap number.
- Start timestamp.
- End timestamp.
- Distance.
- Duration.
- Average speed.
- Optional average heart rate, cadence, and power.

## 10. Persistent Storage

### 10.1 Storage abstraction

Persistent storage must be accessed through a dedicated storage abstraction.

The Activity Recorder must not depend directly on a specific filesystem implementation.

Possible storage backends include:

- microSD card.
- Internal flash.
- Other persistent storage supported by the hardware.

The actual storage hardware, wiring, partition layout, and filesystem support must be verified before implementation.

The presence of a microSD slot does not guarantee that the storage driver is already configured.

### 10.2 Internal format

The recorder should use a versioned internal format.

The internal format must preserve information that may not be supported by FIT or GPX.

It should support:

- Activity metadata.
- Measurement samples.
- Activity events.
- Laps.
- Timestamp information.
- Data validity.
- Source metadata.
- Recovery information.

The exact binary or textual representation must be selected after evaluating storage and performance constraints.

Do not commit to a binary layout before documenting its compatibility and migration strategy.

### 10.3 Write strategy

The implementation should use buffered writes.

Storage operations must not run inside:

- GNSS parsing callbacks.
- BLE callbacks.
- Interrupt handlers.
- LVGL rendering callbacks.

A dedicated recorder task or equivalent asynchronous mechanism should handle storage operations.

### 10.4 Data integrity

The storage implementation should support:

- Checksums or equivalent integrity verification.
- Versioned headers.
- Append-oriented recording.
- Periodic persistence checkpoints.
- Recovery of incomplete activities.
- Validation during activity finalization.

Where supported by the filesystem, use temporary files and atomic finalization operations.

Avoid rewriting the complete activity file after every sample.

### 10.5 Power-loss recovery

The system must be designed for unexpected shutdowns.

After reboot, the recorder should:

1. Inspect the activity storage.
2. Identify incomplete recordings.
3. Validate the last known consistent checkpoint.
4. Recover valid samples where possible.
5. Mark the activity as interrupted or recoverable.
6. Make the recovered activity visible to the user.

Recovery must not automatically mark an interrupted activity as successfully completed.

If recovery is impossible, preserve the original file and report the problem.

### 10.6 Storage errors

The recorder must detect and handle:

- Storage unavailable.
- Card removed.
- Write failure.
- Read failure.
- Storage full.
- Corrupted activity file.
- Filesystem errors.

When an error occurs:

- Stop or suspend unsafe write operations.
- Preserve already persisted data.
- Report the error to the application.
- Avoid reporting successful recording when data is no longer being saved.
- Allow recovery where possible.

The UI must clearly communicate whether recording is active and whether data is being persisted successfully.

### 10.7 Storage capacity

The implementation should monitor available storage capacity.

A configurable warning threshold should be considered.

The system must not silently delete old activities to make room for new recordings.

Any automatic cleanup policy requires a separate explicit approval.

## 11. File Naming and Activity Identification

Activity identifiers must be unique.

File naming should:

- Avoid personal information.
- Avoid relying exclusively on the current system clock.
- Support multiple activities starting within the same second.
- Avoid unsafe characters.
- Prevent accidental overwriting.

A trusted UTC timestamp may be used as part of the filename.

When UTC is unavailable, use a unique identifier or another collision-resistant mechanism.

The internal activity ID must remain stable even if the exported filename changes.

## 12. Export Formats

The Activity Recorder must support exporting activities independently from the internal recording mechanism.

The initial export targets are:

- FIT: primary activity export format.
- GPX: complementary route and track export format.

The internal representation must remain the authoritative source of recorded data.

### 12.1 FIT export

FIT is the preferred format for compatibility with cycling platforms and activity analysis tools.

The exporter should support, where data is available:

- Activity messages.
- Session messages.
- Lap messages.
- Record messages.
- Event messages.
- Position.
- Altitude.
- Speed.
- Distance.
- Heart rate.
- Cadence.
- Power.

The implementation must follow the relevant FIT protocol specification.

Pay particular attention to:

- File headers.
- Message definitions.
- Field numbers.
- Data types.
- Units.
- Scaling factors.
- Timestamp representation.
- Endianness.
- CRC calculations.

Do not assume that internal units match FIT field units.

The exporter must correctly handle unavailable measurements rather than writing misleading zero values.

The FIT implementation must be validated using compatible FIT analysis tools or reference implementations.

### 12.2 GPX export

GPX should be used for route and track interoperability.

The exporter should support:

- Track metadata.
- Track segments.
- Track points.
- Latitude and longitude.
- Elevation.
- Timestamps.

Where supported, extensions may be used for additional measurements such as:

- Heart rate.
- Cadence.
- Power.

Do not assume that all GPX consumers support these extensions.

The exported GPX file must remain valid even when optional measurements are missing.

### 12.3 Export validation

Before exposing an exported file as complete:

- Verify that the file was written successfully.
- Verify its structure.
- Validate checksums where applicable.
- Confirm that mandatory metadata is present.
- Ensure that unavailable values are represented correctly.

A failed export must not modify or damage the original internal activity.

## 13. Connectivity and External Platforms

The Activity Recorder must not implement network communication with external platforms.

Strava integration belongs to the Connectivity component or a dedicated integration layer.

The expected future workflow is:

```text
Activity Recorder
        |
        v
Internal Activity
        |
        v
FIT Export
        |
        v
Connectivity Component
        |
        v
Companion App / Backend
        |
        v
External Platform
```

Authentication, OAuth tokens, upload retries, and network error handling must remain outside the Activity Recorder.

No client secrets must be stored in firmware.

## 14. Concurrency and Task Model

The recorder must be thread-safe.

The implementation should use a dedicated task or equivalent asynchronous processing mechanism.

A conceptual architecture is:

```text
Data Hub
   |
   v
Recorder API
   |
   v
Bounded Queue
   |
   v
Recorder Task
   |
   v
Storage Backend
```

### 14.1 Queue management

The queue must have a documented capacity and overflow policy.

Queue overflow must never result in silent data loss.

Possible strategies include:

- Applying backpressure where safe.
- Prioritizing critical activity events.
- Reporting dropped samples.
- Recording a data-loss diagnostic event when possible.

The selected strategy must not block time-critical sensor processing indefinitely.

### 14.2 Memory management

The implementation must account for the limited memory resources of the ESP32-S3.

Avoid:

- Large unbounded queues.
- Frequent heap allocations.
- Storing the complete activity in RAM.
- Large temporary buffers.
- Uncontrolled file caching.

Use PSRAM only after verifying its availability and suitability for the intended allocation.

The recorder must support activities longer than the available RAM capacity by streaming data to persistent storage.

## 15. Proposed API

The following API is conceptual. It is not an instruction to create or modify source files.

The final interface must be reviewed against the existing project architecture.

Possible operations:

```c
esp_err_t activity_recorder_init(void);

esp_err_t activity_recorder_start(
    const activity_metadata_t *metadata
);

esp_err_t activity_recorder_pause(
    activity_pause_reason_t reason
);

esp_err_t activity_recorder_resume(void);

esp_err_t activity_recorder_stop(void);

esp_err_t activity_recorder_submit_sample(
    const activity_sample_t *sample
);

esp_err_t activity_recorder_submit_event(
    const activity_event_t *event
);

esp_err_t activity_recorder_get_status(
    activity_recorder_status_t *status
);

esp_err_t activity_recorder_export(
    const char *activity_id,
    activity_export_format_t format
);
```

These names and signatures are examples only.

The implementation must define:

- Ownership of input structures.
- Whether operations are synchronous or asynchronous.
- Queue behavior.
- Error reporting.
- Thread-safety guarantees.
- Buffer lifetime.
- Completion notification mechanism.

The UI must receive status updates through an application-level interface, not by accessing recorder internals.

## 16. Recorder Status and Diagnostics

The recorder should expose a status structure containing:

- Current lifecycle state.
- Active activity ID.
- Recording duration.
- Number of recorded samples.
- Number of recorded events.
- Storage availability.
- Pending queue size.
- Last successful write timestamp.
- Last storage error.
- Number of dropped samples, if any.
- Recovery status.

Diagnostics must make it possible to identify whether an activity was:

- Recorded successfully.
- Interrupted unexpectedly.
- Recovered after reboot.
- Partially lost.
- Exported successfully.
- Exported unsuccessfully.

## 17. Testing Requirements

Testing must cover the following areas.

### 17.1 Lifecycle tests

- Start a new activity.
- Pause an activity.
- Resume an activity.
- Stop an activity.
- Attempt invalid state transitions.
- Verify event ordering.
- Verify duration calculations.

### 17.2 Measurement tests

- Valid GNSS measurements.
- Missing GNSS measurements.
- Invalid measurements.
- Stale measurements.
- BLE sensor disconnection.
- Sensor reconnection.
- Partial samples.
- Duplicate timestamps.
- Timestamp discontinuities.

### 17.3 Storage tests

- Successful writes.
- Storage unavailable.
- Storage full.
- Card removal during recording.
- Write failure.
- Read failure.
- Corrupted file.
- Incomplete file.
- Long-duration recording.

### 17.4 Recovery tests

- Unexpected shutdown during recording.
- Shutdown during buffer flush.
- Shutdown during finalization.
- Recovery of an incomplete activity.
- Recovery with corrupted trailing data.
- Verification that previously completed activities remain intact.

### 17.5 Export tests

- FIT export with all supported measurements.
- FIT export with missing measurements.
- GPX export with valid position data.
- GPX export with missing altitude.
- GPX export with optional extensions.
- Invalid internal activity.
- Export interrupted by storage failure.
- Validation with external FIT and GPX tools.

### 17.6 Performance tests

Measure:

- Recorder task stack usage.
- Queue occupancy.
- Write latency.
- CPU utilization.
- Heap usage.
- PSRAM usage, if applicable.
- Storage throughput.
- Sample loss under sustained load.

Testing must include long-duration recordings.

## 18. Implementation Constraints

The Activity Recorder agent must:

- Inspect the existing project before proposing implementation changes.
- Reuse existing infrastructure when appropriate.
- Avoid assuming that microSD support is already implemented.
- Avoid introducing a new filesystem without architectural review.
- Keep the recorder independent of LVGL.
- Keep storage operations out of sensor callbacks.
- Preserve the distinction between invalid, unavailable, stale, and zero-valued measurements.
- Avoid changing Cycling Core responsibilities.
- Avoid implementing Strava or other network integrations.
- Avoid deleting or overwriting existing activity data without explicit approval.

### 18.1 Approval policy

The project uses `approval_each`.

Therefore:

- Every individual file modification requires explicit user approval.
- Creating a file requires approval.
- Editing a file requires approval.
- Deleting a file requires approval.
- Renaming or moving a file requires approval.

Approval for one file does not authorize changes to other files.

### 18.2 Execution policy

The agent must not execute commands without explicit approval.

This includes:

- Build commands.
- Test commands.
- Formatting commands.
- Flashing commands.
- Hardware interaction.
- Filesystem formatting.
- Storage deletion.
- Git operations that modify repository state.

Read-only inspection may be performed within the project's established permissions.

## 19. Future Extensions

The architecture should leave room for:

- Multiple activity types.
- Advanced lap management.
- Structured sensor diagnostics.
- Barometric altitude.
- Temperature sensors.
- Advanced power metrics.
- Training load calculations.
- Activity synchronization.
- Companion application integration.
- Additional export formats.
- Cloud backup.
- Activity import.

These features must not complicate the initial implementation unnecessarily.

## 20. Definition of Done

The initial Activity Recorder implementation can be considered complete when:

- Activities can be started, paused, resumed, and stopped.
- Valid measurements are recorded with correct timestamps.
- Missing measurements are represented correctly.
- Recording works without network connectivity.
- Storage errors are detected and reported.
- Incomplete recordings can be recovered where possible.
- Completed activities remain intact after reboot.
- FIT export produces valid files.
- GPX export produces valid files.
- Storage operations do not block sensor processing or UI rendering.
- Resource usage is measured and documented.
- Tests cover lifecycle, storage, recovery, and export.
- All modifications have received the required individual approval.