# Bike Computer — GitHub Copilot Instructions

## 1. Project Identity

This repository contains the firmware for an ESP32-S3-based cycling computer.

The long-term objective is to develop a modular, reliable and extensible cycling computer inspired by high-end commercial devices such as the Garmin Edge series.

The firmware must support:
- GNSS positioning and speed measurement.
- Cycling metrics and activity recording.
- A customizable LVGL dashboard.
- BLE cycling sensors.
- Wi-Fi connectivity and activity synchronization.
- Local offline-first data storage.
- Future navigation and smartphone-assisted GPS functionality.

Do not assume that all planned features are already implemented.

## 2. Verified Development Environment

Unless the user explicitly changes these settings, use:

- MCU: ESP32-S3.
- Framework: ESP-IDF 5.3.0.
- Programming language: C.
- RTOS: FreeRTOS.
- UI framework: LVGL 8.4.0.
- IDE: Visual Studio Code with GitHub Copilot.

The current project includes an `08_lvgl_Porting` subproject. Do not assume that this subproject represents the entire repository.

## 3. Hardware Profile

### Main board

- Board: Waveshare ESP32-S3-Touch-LCD-4.3B.
- MCU module: ESP32-S3-WROOM-1-N16R8.
- Flash: 16 MB.
- PSRAM: 8 MB.
- Display: 4.3-inch RGB, 800x480.
- Touch controller: GT911.
- Connectivity: 2.4 GHz Wi-Fi and Bluetooth LE.
- Storage expansion: microSD.
- RTC: PCF85063.
- GPIO expander: CH422G.

### GNSS receiver

- Module: FlyFishRC M10 Mini.
- GNSS generation: u-blox M10.
- UART RX: GPIO43.
- UART TX: GPIO44.

**Important hardware constraint:**

GPIO43 and GPIO44 are being used for GNSS UART communication following a hardware modification performed by the user to the board's RS485 circuitry/path.

Do not restore RS485 functionality, change these pins, or assume the original RS485 circuit is still connected in its standard configuration.

The exact electrical topology of the modification has not been independently verified. Do not make assumptions about transceiver bypassing, voltage levels or signal conditioning.

The actual GNSS baud rate, output configuration and supported protocol features must be verified before changing them.

## 4. Information Reliability Labels

Every technical specification or architectural decision must be treated according to its evidence level.

Use these labels in documentation and technical reports:

- `VERIFIED`: Confirmed directly from source code, configuration or reliable documentation.
- `USER_CONFIRMED`: Explicitly confirmed by the user.
- `ASSUMPTION`: Plausible but not yet verified.
- `PROPOSAL`: Suggested design or implementation.
- `FUTURE`: Planned functionality not currently implemented.

Never present an assumption or proposal as an existing implementation.

When information conflicts, report the conflict and ask for clarification rather than silently choosing one version.

## 5. Architecture Principles

The firmware should evolve towards a modular layered architecture:

1. Hardware and Board Support Package.
2. Hardware drivers and communication transports.
3. Protocol adapters.
4. Unified Data Hub.
5. Cycling Core.
6. Activity Recorder and Storage.
7. Connectivity Services.
8. UI and User Experience.

### Separation of responsibilities

- Hardware drivers must not contain cycling business logic.
- Protocol adapters must normalize incoming data.
- The Data Hub must manage data validity, freshness and source selection.
- The Cycling Core must calculate cycling metrics independently of the UI.
- The Activity Recorder must manage activity persistence.
- Connectivity services must not directly manipulate UI widgets.
- The UI must consume application data through defined interfaces.

Avoid direct dependencies between unrelated layers.

## 6. GNSS and Data Sources

The FlyFishRC M10 Mini connected through UART is the primary positioning source.

A smartphone GPS source may be introduced in the future.

BLE cycling sensors must be supported through standard Bluetooth SIG profiles where applicable:

- Cycling Speed and Cadence Service: `0x1816`.
- Cycling Power Service: `0x1818`.
- Heart Rate Service: `0x180D`.
- Battery Service: `0x180F`.
- Device Information Service: `0x180A`.

Source selection must be performed independently for each metric.

Examples:

- Position: UART GNSS primary; smartphone GPS future fallback.
- Speed: configured BLE wheel sensor preferred; otherwise GNSS.
- Altitude: GNSS initially; barometer may be added later.
- Heart rate, cadence and power: BLE sensors.

Never mix incompatible samples or present stale measurements as current.

Every measurement should carry sufficient metadata to identify:
- Value.
- Unit.
- Timestamp.
- Validity.
- Freshness.
- Source.
- Quality, where available.

Do not implement speculative data fusion or automatic source switching without a defined policy.

## 7. Coding Standards

Use:
- Clear module boundaries.
- Descriptive names.
- Small, focused functions.
- Explicit error handling.
- Consistent ownership of resources.
- Documented public interfaces.
- Thread-safe communication between tasks.

Avoid:
- Unnecessary global variables.
- Blocking operations in time-critical tasks.
- Long operations inside BLE callbacks.
- Direct hardware access from UI code.
- Unchecked task creation and peripheral initialization.
- Silent error recovery that hides failures.

Use ESP-IDF APIs and conventions appropriate for version 5.3.0.

Do not introduce new dependencies without explaining their purpose and obtaining approval.

## 8. Concurrency and Data Integrity

The firmware uses FreeRTOS and may have multiple concurrent execution contexts.

Any shared data must have a clearly defined synchronization strategy.

Use appropriate mechanisms such as:
- FreeRTOS mutexes.
- Queues.
- Event groups.
- Task notifications.
- Immutable snapshots.
- Atomic operations where appropriate.

Do not assume that a shared structure is safe simply because individual fields are small.

Never hold a mutex while performing slow I/O or waiting for external communication.

## 9. UI Requirements

The UI uses LVGL 8.4.0.

The UI must:
- Remain responsive.
- Avoid blocking operations.
- Use thread-safe LVGL access.
- Obtain application data through defined interfaces.
- Support configurable dashboard layouts.
- Clearly distinguish unavailable, invalid and stale measurements.

Do not place business logic or protocol parsing inside LVGL callbacks.

## 10. Development Autonomy and Approval Policy

This project uses supervised autonomy.

The user has selected:

- Autonomy: `supervised`.
- Collaboration: `central`.
- Scope: `project`.
- File approval: `approval_each`.
- Quality review: `major`.

These settings are mandatory.

### File modification approval

**Every individual file modification requires explicit user approval before the modification is performed.**

This includes:
- Creating a file.
- Editing an existing file.
- Deleting a file.
- Renaming or moving a file.
- Modifying configuration files.
- Updating dependencies.

Do not interpret approval for one file as approval for other files.

Before requesting approval, provide:
1. The exact file path.
2. The reason for the change.
3. A concise description of the proposed modification.
4. Any relevant risks or dependencies.

Wait for explicit approval before proceeding.

### Command execution approval

Do not execute commands without explicit user approval.

This includes:
- Building the firmware.
- Running tests.
- Flashing firmware.
- Erasing flash or storage.
- Running scripts.
- Installing or updating dependencies.
- Changing ESP-IDF configuration.
- Performing hardware diagnostics.

You may propose commands and explain their expected effects, but must wait for approval before execution.

### Git operations

Never perform Git operations that change repository state without explicit approval.

This includes:
- Commits.
- Pushes.
- Pulls.
- Branch creation or deletion.
- Merges.
- Resets.
- Rebases.
- Stashing or discarding changes.

Read-only inspection may be proposed separately.

## 11. Agent Collaboration

Use a central coordination model.

The Orchestrator coordinates work and delegates responsibilities to specialized agents when appropriate.

Agents must not assume that another agent has automatically performed a task.

Handoffs are coordinated and explicitly initiated. The user remains in control of the workflow.

Before involving another agent, identify:
- The objective.
- The relevant files.
- Expected output.
- Dependencies.
- Approval requirements.

## 12. Quality and Risk Management

A Quality review is mandatory for major or high-risk changes.

Examples:
- Data Hub architecture changes.
- GNSS source-selection changes.
- BLE lifecycle changes.
- FreeRTOS synchronization changes.
- Activity recording format changes.
- Storage and filesystem changes.
- Memory-intensive LVGL modifications.
- Security and authentication changes.
- Hardware pin or peripheral configuration changes.

The Quality agent must perform a read-only review unless explicitly authorized otherwise.

## 13. Documentation

Keep documentation synchronized with implementation.

Document:
- Architectural decisions.
- Public interfaces.
- Data contracts.
- Hardware assumptions.
- Configuration requirements.
- Known limitations.
- Testing procedures.

Use Architecture Decision Records (ADRs) for significant architectural decisions.

## 14. Default Behavior

When a request is ambiguous:

1. Inspect the relevant context.
2. Identify missing information.
3. Explain assumptions.
4. Propose a solution.
5. Identify affected files.
6. Request approval before modifications or command execution.

Never silently expand the scope of a task.

The user is the final decision-maker for all project changes.