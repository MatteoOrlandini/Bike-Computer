# STORAGE_SPECIFICATION.md

# Storage Specification

## 1. Purpose

The Storage component provides persistent data storage for the Bike Computer.

It is designed to support reliable activity recording, activity recovery, configuration persistence, and future application features.

The storage architecture must prioritize:

- Data integrity.
- Reliability during unexpected power loss.
- Predictable resource consumption.
- Separation between application logic and storage hardware.
- Extensibility for future storage backends.

The initial implementation must support the requirements of the Activity Recorder without introducing unnecessary complexity.

## 2. Scope

The Storage component is responsible for:

- Initializing and managing persistent storage.
- Providing a filesystem abstraction.
- Reading and writing application data.
- Managing storage availability.
- Reporting filesystem errors.
- Supporting safe file operations.
- Providing diagnostics and storage statistics.
- Supporting recovery procedures.

The Storage component is not responsible for:

- Deciding which cycling measurements must be recorded.
- Calculating cycling metrics.
- Managing activity lifecycle.
- Exporting FIT or GPX files.
- Managing network synchronization.
- Rendering storage information in the UI.

These responsibilities belong to other components.

## 3. Hardware Context

The current hardware platform is:

- Waveshare ESP32-S3-Touch-LCD-4.3B.
- ESP32-S3-WROOM-1-N16R8.
- 16 MB flash.
- 8 MB PSRAM.
- microSD card slot.

The actual microSD electrical interface, pin assignments, bus configuration, and driver support must be verified against the board documentation and existing project configuration before implementation.

Do not assume that the microSD interface is already initialized or operational.

Do not modify existing GPIO assignments without reviewing the hardware configuration.

## 4. Architectural Design

The Storage component must provide a hardware-independent interface to application components.

Conceptual architecture:

```text
        Application Components
                  |
                  v
           Storage API
                  |
                  v
         Storage Manager
                  |
                  v
        Filesystem Adapter
                  |
          +-------+-------+
          |               |
          v               v
      microSD         Internal Flash
```

The initial implementation may use only one storage backend.

The architecture should allow additional backends without requiring changes to the Activity Recorder.

### 4.1 Component ownership

| Component | Responsibility |
|---|---|
| Storage API | Public interface for storage operations |
| Storage Manager | Coordinates storage state and operations |
| Filesystem Adapter | Provides filesystem-specific functionality |
| Storage Backend | Communicates with the actual storage hardware |
| Activity Recorder | Defines activity data and recording requirements |
| UI/UX | Displays storage status and errors |

Application components must not directly access filesystem-specific APIs.

## 5. Storage Backend Strategy

### 5.1 microSD

The microSD card is the preferred initial backend for activity recordings because activities may generate substantial amounts of data.

Advantages:

- Removable storage.
- Expandable capacity.
- Easier activity extraction.
- Reduced wear on internal flash.

Before selecting the implementation, verify:

- Supported bus interface.
- Available GPIO pins.
- Card detection capabilities.
- Supported card capacities.
- Filesystem compatibility.
- Driver support in the selected ESP-IDF version.
- Performance under sustained writes.
- Behavior when the card is removed unexpectedly.

### 5.2 Internal flash

Internal flash may be used for:

- Device configuration.
- Small persistent metadata.
- Application state.
- Recovery information.
- Small diagnostic records.

Activity recording on internal flash must not be introduced without evaluating flash endurance, write amplification, and available capacity.

If NVS is used, it should be reserved for suitable small configuration values rather than large activity files.

### 5.3 PSRAM

PSRAM is volatile memory.

It may be used for:

- Temporary buffers.
- File processing.
- Export operations.
- Queue storage, if appropriate.

It must never be treated as persistent storage.

The implementation must verify the actual PSRAM configuration before relying on it.

## 6. Filesystem Selection

The filesystem must be selected after verifying hardware and ESP-IDF support.

Possible candidates include:

- FAT32 for removable microSD storage.
- LittleFS for suitable internal flash use.
- NVS for configuration and small key-value data.

The choice must consider:

- Power-loss behavior.
- Compatibility with desktop operating systems.
- File size limitations.
- Directory support.
- Recovery capabilities.
- Performance.
- Flash endurance.
- Maintenance complexity.

Do not introduce multiple filesystems without a clear requirement.

### 6.1 Filesystem responsibilities

The filesystem adapter must provide a consistent interface for:

- Mounting.
- Unmounting.
- File creation.
- File reading.
- File writing.
- File appending.
- File closing.
- File deletion.
- Directory listing.
- Capacity reporting.
- File metadata inspection.

The Storage API should hide filesystem-specific implementation details.

## 7. Directory Structure

The storage layout should be organized by data type.

A proposed structure is:

```text
/storage/
    activities/
        <activity_id>/
            activity.dat
            metadata.json
            recovery.dat
            exports/
                activity.fit
                activity.gpx/

    config/
        device.json

    diagnostics/
        storage.log

    system/
        storage_state.dat
```

This structure is conceptual.

The final layout must consider filesystem limitations and the cost of creating many small files.

Where appropriate, metadata and recovery information may be stored in the internal activity file instead of separate files.

The implementation must avoid unnecessary duplication of activity data.

## 8. Storage API

The Storage component should expose a small and consistent API.

The following interface is conceptual and must be adapted to the existing project.

```c
esp_err_t storage_init(void);

esp_err_t storage_get_status(
    storage_status_t *status
);

esp_err_t storage_mount(void);

esp_err_t storage_unmount(void);

esp_err_t storage_open(
    const char *path,
    storage_file_t *file,
    storage_open_mode_t mode
);

esp_err_t storage_read(
    storage_file_t *file,
    void *buffer,
    size_t size,
    size_t *bytes_read
);

esp_err_t storage_write(
    storage_file_t *file,
    const void *buffer,
    size_t size,
    size_t *bytes_written
);

esp_err_t storage_flush(
    storage_file_t *file
);

esp_err_t storage_close(
    storage_file_t *file
);

esp_err_t storage_delete(
    const char *path
);

esp_err_t storage_get_capacity(
    uint64_t *total_bytes,
    uint64_t *free_bytes
);
```

These functions are examples, not mandatory names or signatures.

The final API must define:

- File handle ownership.
- Buffer ownership.
- Thread-safety guarantees.
- Blocking behavior.
- Error codes.
- Maximum supported path length.
- Maximum supported file size.
- Operation timeout behavior.

## 9. Initialization and Mounting

Storage initialization must be performed in a controlled sequence.

Conceptual initialization:

```text
START
  |
  v
Initialize storage hardware
  |
  v
Check backend availability
  |
  v
Mount filesystem
  |
  v
Validate directory structure
  |
  v
Check storage integrity
  |
  v
Recover pending operations
  |
  v
Storage ready
```

The application must distinguish between:

- Storage not initialized.
- Storage unavailable.
- Filesystem not mounted.
- Filesystem mounted.
- Filesystem read-only, if supported.
- Filesystem error.
- Storage recovery in progress.

A failed mount must not crash the application.

The device must remain usable when removable storage is unavailable, although recording features may be restricted.

## 10. Safe File Operations

### 10.1 File creation

File creation must avoid accidental overwriting.

The Storage component should provide a safe create-new operation when required.

If a file with the same name already exists, the operation must fail or use an explicitly defined collision-handling strategy.

### 10.2 File writing

Write operations must report:

- Operation result.
- Number of bytes written.
- Storage errors.

A successful write call does not necessarily mean that data has reached non-volatile media.

The implementation must document the semantics of write and flush operations for the selected filesystem and hardware.

### 10.3 Flush and synchronization

The Storage component must provide a mechanism to request that pending file data is synchronized.

The actual durability guarantees depend on the filesystem, driver, and storage hardware.

Do not claim that a flush guarantees physical persistence unless the underlying implementation provides that guarantee.

### 10.4 File closing

Open files must be closed correctly.

The component must handle errors during close and synchronization.

The Activity Recorder must not mark an activity as successfully finalized if the required file operations have failed.

### 10.5 Atomic updates

For small metadata files, use a safe update strategy where supported:

1. Write a temporary file.
2. Flush the temporary file.
3. Validate its contents.
4. Replace the previous file using a filesystem-supported safe operation.
5. Preserve a recoverable copy until the replacement is complete.

Do not assume that rename or replacement operations are atomic on every filesystem.

The exact guarantees must be verified.

## 11. Data Integrity

Storage operations must be designed to detect incomplete or corrupted data.

### 11.1 Checksums

Where appropriate, files or data blocks should include checksums.

Possible approaches include:

- CRC32.
- CRC16.
- Other documented integrity mechanisms.

The selected algorithm must be consistent and documented.

Checksums detect accidental corruption; they do not provide cryptographic authenticity.

### 11.2 Versioning

Persistent data formats must include a version identifier.

The version must allow the application to detect incompatible data structures.

The implementation should define:

- Current format version.
- Supported previous versions.
- Migration strategy.
- Behavior for unsupported future versions.
- Recovery behavior when migration fails.

Never silently reinterpret data from an unknown version.

### 11.3 Partial writes

The implementation must account for:

- Interrupted writes.
- Short writes.
- Failed flush operations.
- Incomplete metadata updates.
- Corrupted final records.

The storage format should make it possible to identify the last valid data block or checkpoint.

### 11.4 Recovery journal

A lightweight journal or checkpoint mechanism should be considered for activity recording.

It may contain:

- Activity identifier.
- Last valid record offset.
- Last persisted timestamp.
- Last committed sequence number.
- Recovery status.
- Integrity information.

The final design must avoid excessive write frequency and unnecessary flash wear.

## 12. Power-Loss and Unexpected Shutdown

Unexpected power loss must be treated as a normal failure scenario.

The Storage component should support recovery after:

- Battery depletion.
- Sudden power disconnection.
- Watchdog reset.
- Software crash.
- Filesystem operation interruption.

After restart, the system must:

1. Initialize the storage backend.
2. Mount the filesystem.
3. Inspect pending storage operations.
4. Identify incomplete files.
5. Validate available data.
6. Restore the last valid state where possible.
7. Report recovery results to the application.

Recovery must not overwrite the original data before a valid recovery result has been established.

If recovery fails, preserve the original data for possible manual analysis.

## 13. microSD Removal and Insertion

The removable storage lifecycle must be explicitly managed.

### 13.1 Card removal

If card detection is available, the system should detect removal.

If card detection is not available, filesystem errors must be used to identify unavailable storage.

When removal occurs during recording:

- Stop unsafe storage operations.
- Preserve already written data.
- Mark the recording as interrupted or degraded.
- Notify the Activity Recorder.
- Notify the UI.
- Avoid repeated writes to an unavailable device.

The application must not report successful recording while data is not being persisted.

### 13.2 Card insertion

After insertion:

- Verify that the card is accessible.
- Mount the filesystem if required.
- Validate the storage structure.
- Check available capacity.
- Report readiness.

Do not automatically resume an interrupted activity unless the Activity Recorder explicitly approves the recovery procedure.

### 13.3 Filesystem errors

Filesystem errors must be handled without uncontrolled retries.

The implementation should define:

- Retry limits.
- Retry delays.
- Error classification.
- Recovery requirements.
- Conditions requiring a remount.

Repeated failures must not create an infinite retry loop.

## 14. Capacity Management

The Storage component must expose:

- Total capacity.
- Used capacity.
- Free capacity.
- Storage availability.
- Optional filesystem health information.

The Activity Recorder may use this information to estimate whether a new activity can be recorded.

The system should provide configurable warning thresholds.

Possible warning levels:

- Low storage.
- Critical storage.
- Storage full.

The actual thresholds must be configurable and documented.

### 14.1 Storage full behavior

When storage becomes full:

- Stop accepting writes that cannot be safely persisted.
- Notify the Activity Recorder.
- Preserve existing completed activities.
- Report the failure to the UI.
- Keep the last valid recording state recoverable.

Never silently overwrite existing activity files.

## 15. Concurrency

Storage access must be thread-safe.

The implementation must define how multiple components access the storage backend.

Possible approaches include:

- A dedicated storage task.
- A mutex protecting filesystem operations.
- A serialized request queue.

The preferred approach should minimize blocking in time-critical tasks.

### 15.1 Forbidden execution contexts

Filesystem operations must not be executed directly from:

- Interrupt handlers.
- GNSS parsing callbacks.
- BLE callbacks.
- LVGL rendering callbacks.
- Other time-critical sensor-processing callbacks.

### 15.2 Long-running operations

Operations such as directory listing, file export, and recovery may take significant time.

They must not block the main application task indefinitely.

The implementation should provide asynchronous operations or bounded execution where necessary.

## 16. Performance and Resource Management

The implementation must consider ESP32-S3 resource limitations.

Measure:

- Read throughput.
- Write throughput.
- Flush latency.
- File-open latency.
- Directory-listing latency.
- Queue occupancy.
- Heap usage.
- PSRAM usage.
- Task stack usage.
- CPU utilization.

### 16.1 Buffering

Use bounded buffers.

Buffer sizes must be selected based on measurements rather than assumptions.

The implementation must define what happens when buffers are full.

### 16.2 Allocation strategy

Avoid frequent dynamic allocation during continuous activity recording.

Prefer predictable allocation and reuse of buffers where appropriate.

Do not allocate memory proportional to total activity duration.

### 16.3 Wear management

If internal flash is used, evaluate:

- Flash write endurance.
- Write frequency.
- Erase block size.
- Wear-leveling support.
- NVS suitability.

Do not use internal flash as a high-frequency activity log without a documented endurance analysis.

## 17. Configuration Storage

Device configuration should be stored separately from activity recordings.

Configuration data may include:

- Display preferences.
- Unit preferences.
- Sensor configuration.
- GNSS configuration.
- Wi-Fi settings.
- Recording preferences.
- User interface settings.

Sensitive values, such as credentials, must not be stored as plain text unless the security implications have been explicitly evaluated.

Configuration updates must use a safe persistence strategy.

Invalid configuration data must not prevent the device from starting.

The application should provide sensible defaults or a recovery mechanism.

## 18. Diagnostics and Logging

The Storage component should expose diagnostic information.

Recommended fields:

- Backend type.
- Backend availability.
- Mount state.
- Filesystem type.
- Total capacity.
- Free capacity.
- Used capacity.
- Last successful operation.
- Last error code.
- Number of read failures.
- Number of write failures.
- Number of recovery operations.
- Number of detected corrupt files.

Diagnostic logging must not generate excessive storage writes.

Avoid writing every storage operation to a persistent log.

## 19. Security and Data Protection

The initial storage implementation should prioritize data integrity and safe access.

Future security requirements may include:

- Encryption at rest.
- Secure configuration storage.
- File integrity authentication.
- Secure deletion.
- Protected firmware configuration.

These features must be designed separately and must not be assumed to exist.

Do not store network credentials or authentication tokens in activity files.

Do not expose sensitive configuration values through diagnostic logs.

## 20. Testing Requirements

### 20.1 Initialization tests

- Successful initialization.
- Missing microSD card.
- Unsupported filesystem.
- Failed mount.
- Invalid directory structure.
- Corrupted metadata.
- Repeated initialization.

### 20.2 File operation tests

- File creation.
- File reading.
- File writing.
- File appending.
- File flushing.
- File closing.
- File deletion.
- Directory listing.
- File collision.
- Invalid paths.
- Maximum file size.

### 20.3 Failure tests

- Card removal during writing.
- Storage full.
- Failed flush.
- Failed close.
- Partial write.
- Corrupted data block.
- Unexpected shutdown.
- Filesystem remount.
- Repeated I/O errors.

### 20.4 Recovery tests

- Recovery after interrupted activity recording.
- Recovery after incomplete metadata update.
- Recovery after interrupted file finalization.
- Validation of the last committed record.
- Preservation of completed activities.
- Unsupported format version.
- Failed recovery without original data destruction.

### 20.5 Performance tests

- Sustained sequential writes.
- Sustained sequential reads.
- Small-file operations.
- Large-file operations.
- Long-duration activity recording.
- Concurrent application requests.
- Queue saturation.
- Memory consumption under sustained load.

Tests must include the actual target hardware and representative microSD cards before declaring the implementation production-ready.

## 21. Implementation Constraints

The Storage agent must:

- Inspect the existing project before proposing changes.
- Verify actual hardware connections and existing drivers.
- Verify the selected ESP-IDF version and available filesystem components.
- Avoid assuming that microSD support is already operational.
- Avoid introducing a filesystem without architectural review.
- Keep filesystem-specific code behind the Storage abstraction.
- Preserve existing configuration and activity data.
- Avoid formatting storage automatically.
- Avoid deleting files automatically.
- Avoid blocking sensor processing or UI rendering.
- Document durability and recovery limitations honestly.

### 21.1 Approval policy

The project uses `approval_each`.

Therefore:

- Every individual file creation requires explicit approval.
- Every individual file modification requires explicit approval.
- Every deletion requires explicit approval.
- Every rename or move requires explicit approval.

Approval for one file does not authorize modifications to other files.

### 21.2 Execution policy

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

## 22. Future Extensions

The architecture should allow future support for:

- Multiple storage backends.
- Internal flash fallback.
- Activity synchronization.
- Encrypted activity storage.
- Storage health monitoring.
- Automatic recovery improvements.
- Data migration tools.
- Activity import.
- Backup and restore.
- Storage diagnostics accessible through the companion application.

These features must not complicate the initial implementation unnecessarily.

## 23. Definition of Done

The initial Storage implementation can be considered complete when:

- The selected storage backend is correctly initialized.
- Filesystem operations are exposed through a stable abstraction.
- Storage errors are detected and reported.
- Activity files can be written and read reliably.
- Unexpected shutdown recovery is implemented and tested.
- Storage capacity is monitored.
- Card removal and insertion are handled safely, where supported.
- Existing data is protected from accidental overwriting.
- Filesystem operations do not block time-critical application tasks.
- Resource consumption is measured and documented.
- Tests cover normal operations, failure scenarios, and recovery.
- All modifications have received the required individual approval.