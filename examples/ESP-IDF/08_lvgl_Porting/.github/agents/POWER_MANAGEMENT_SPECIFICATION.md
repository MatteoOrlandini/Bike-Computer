# POWER_MANAGEMENT_SPECIFICATION.md

# Power Management Specification

## 1. Purpose

The Power Management component is responsible for monitoring and optimizing the power consumption of the Bike Computer.

Its primary objective is to maximize battery life without compromising essential cycling functionality.

The architecture must support different operating conditions, including normal riding, standby, configuration, and charging.

Power management must be coordinated across hardware drivers and application components.

## 2. Scope

The Power Management component is responsible for:

- Monitoring battery information.
- Monitoring charging status, where supported.
- Providing battery status to the application.
- Defining device power states.
- Coordinating power-saving policies.
- Managing inactivity and display timeout policies.
- Controlling optional peripheral power states.
- Providing low-battery warnings.
- Supporting future deep-sleep and standby modes.

It is not responsible for:

- Implementing individual hardware drivers.
- Calculating cycling metrics.
- Managing GNSS position selection.
- Managing activity recording.
- Rendering battery indicators.
- Controlling hardware directly without the Hardware/BSP abstraction.

## 3. Hardware Context

The target platform is:

- Waveshare ESP32-S3-Touch-LCD-4.3B.
- ESP32-S3-WROOM-1-N16R8.
- 16 MB flash.
- 8 MB PSRAM.
- 4.3-inch RGB display.
- Wi-Fi and BLE connectivity.
- GNSS receiver connected through UART.

The following hardware details must be verified before implementation:

- Battery voltage measurement circuit.
- ADC channel and GPIO assignment.
- Voltage divider ratio.
- Battery chemistry and nominal voltage.
- Battery capacity.
- Charging circuit.
- Charging status signals.
- Power enable signals.
- Display backlight control.
- Peripheral power control.
- Actual board power consumption.

Do not assume that the development board includes a battery monitoring circuit.

Do not invent battery voltage thresholds or GPIO assignments.

## 4. Design Principles

The implementation must follow these principles:

1. Essential cycling functionality must take priority over power savings.
2. Power-saving policies must not interrupt active activity recording.
3. Hardware power control must be centralized.
4. Power state transitions must be explicit.
5. Battery measurements must include validity information.
6. Low-battery behavior must be predictable.
7. Peripheral shutdown must respect hardware dependencies.
8. Power consumption must be measured on actual hardware.
9. The device must recover correctly after power interruptions.
10. Power management must remain independent of LVGL.

## 5. Architectural Position

Power Management coordinates system-level power policies.

Conceptual architecture:

```text id="d7fr9s"
          Application Components
                    |
                    v
             Power Manager
                    |
          +---------+---------+
          |         |         |
          v         v         v
       Battery   Power State  Peripheral
       Monitor    Manager     Control
          |         |         |
          +---------+---------+
                    |
                    v
               Hardware/BSP
                    |
                    v
              Physical Device
```

The Power Manager should receive requests from application components and coordinate them according to the current operating state.

Hardware-specific operations must remain in the Hardware/BSP layer or dedicated drivers.

## 6. Power States

The system should support the following conceptual power states:

| State | Description |
|---|---|
| BOOT | Device initialization |
| ACTIVE | Normal operation |
| RIDING | Active cycling activity |
| DISPLAY_DIMMED | Reduced display brightness |
| DISPLAY_OFF | Display disabled while the system remains operational |
| STANDBY | Reduced activity with selected peripherals disabled |
| LOW_POWER | Restricted operation due to low battery |
| CHARGING | Charging detected |
| SHUTDOWN | Controlled shutdown |

The final state model must be adapted to the actual hardware capabilities.

### 6.1 Boot

During boot:

- Initialize essential hardware.
- Initialize battery monitoring, if available.
- Determine whether external power is present.
- Restore persistent application state.
- Initialize required communication interfaces.
- Select the initial power policy.

### 6.2 Active

Normal operation should support:

- Display rendering.
- GNSS acquisition.
- BLE sensor communication.
- Cycling Core calculations.
- Activity recording.
- User interaction.

### 6.3 Riding

During an active activity:

- GNSS must remain operational.
- Connected cycling sensors must remain available.
- Activity recording must continue.
- Time-critical tasks must not be suspended.
- Network activity may be restricted.
- Display brightness may be reduced according to user settings.

An active activity must never be silently stopped by a power-saving transition.

### 6.4 Display dimming

Display dimming is one of the preferred initial power-saving mechanisms.

The implementation should support:

- Configurable brightness.
- Automatic dimming after inactivity.
- Immediate brightness restoration on user interaction.
- Different brightness policies for day and night, if supported.

Dimming must not interfere with touch detection.

### 6.5 Display off

The display may be disabled while the rest of the system continues operating.

Before implementation, verify:

- Display controller behavior.
- Backlight control.
- RGB interface requirements.
- Whether the panel can be disabled independently.
- Whether touch remains operational.

The display-off state must not interrupt activity recording or navigation processing.

### 6.6 Standby

Standby may disable selected nonessential components.

Potential candidates include:

- Wi-Fi.
- Display backlight.
- Unused peripherals.
- Nonessential background services.

GNSS, BLE sensors, and activity recording must remain operational if the device is expected to continue recording an activity.

### 6.7 Low power

Low-power mode is entered when battery capacity becomes critically low.

The system should:

- Notify the user.
- Reduce nonessential consumption.
- Disable optional network operations.
- Reduce display brightness.
- Preserve activity data.
- Attempt safe activity finalization if shutdown becomes necessary.

The actual low-battery thresholds must be determined from verified battery characteristics.

### 6.8 Shutdown

Before controlled shutdown:

- Stop accepting new nonessential operations.
- Notify application components.
- Flush pending activity data where possible.
- Persist required application state.
- Close open storage operations.
- Disable peripherals in a safe order.
- Enter the hardware-supported shutdown state.

The system must not claim successful data persistence if storage synchronization fails.

## 7. Battery Monitoring

### 7.1 Measurement source

Battery monitoring may use:

- ADC voltage measurement.
- A dedicated battery monitoring IC.
- A fuel gauge, if present.

The actual measurement hardware must be identified before implementation.

### 7.2 Battery information

The battery monitor should provide:

| Field | Description |
|---|---|
| Voltage | Measured battery voltage |
| Percentage | Estimated state of charge, if supported |
| Charging | Charging status |
| External power | External supply presence |
| Measurement valid | Measurement validity |
| Measurement timestamp | Time of last update |
| Battery health | Optional future information |

Voltage and state of charge must not be treated as interchangeable measurements.

### 7.3 Voltage measurement

If ADC measurement is used:

- Verify ADC channel configuration.
- Verify voltage divider ratio.
- Apply appropriate calibration.
- Account for measurement noise.
- Avoid excessive sampling frequency.
- Validate readings against a calibrated multimeter.

The battery voltage must never be inferred from an unverified ADC reading.

### 7.4 State of charge

Battery percentage estimation must use a documented method.

Possible approaches include:

- Voltage-based estimation.
- Fuel-gauge IC readings.
- Coulomb counting, if supported by suitable hardware.

Voltage-based estimation may be inaccurate under load and must not be presented as a precise state-of-charge measurement.

### 7.5 Measurement validity

Battery measurements must distinguish between:

- Valid.
- Unavailable.
- Invalid.
- Stale.

A failed measurement must not be interpreted as an empty battery.

## 8. Charging Management

Charging support depends on the actual board power architecture.

The implementation must verify:

- Charger IC.
- Charging current.
- Charging status output.
- Battery protection circuitry.
- External power detection.
- Whether the device can operate while charging.

If charging status is available, the system should distinguish:

- Not charging.
- Charging.
- Fully charged.
- Charging error.
- Charging status unavailable.

Do not infer charging state solely from a temporary increase in battery voltage.

## 9. Peripheral Power Policies

The Power Manager should coordinate the power state of optional peripherals.

Potential peripherals include:

| Peripheral | Possible policy |
|---|---|
| Display backlight | Dim or disable |
| Wi-Fi | Disable when unused |
| BLE | Maintain while sensors are connected |
| GNSS | Maintain during navigation or recording |
| microSD | Keep available during recording |
| Audio output | Disable when unused |
| Unused sensors | Disable where supported |

Actual peripheral power control must be verified against the board schematic.

Disabling a peripheral must not accidentally disable shared hardware resources.

## 10. GNSS Power Management

GNSS power consumption must be considered carefully.

The receiver should remain active when required for:

- Position tracking.
- Speed measurement.
- Navigation.
- Activity recording.

Possible future optimizations include:

- Reduced update frequency when stationary.
- Receiver power reduction when navigation is inactive.
- Controlled GNSS sleep modes.
- Reacquisition strategies.

GNSS power-saving commands must only be implemented after verifying the exact receiver model, firmware, and supported protocol.

Do not send unverified UBX commands to the M10 Mini receiver.

Any change to receiver configuration must preserve the existing UART communication settings unless explicitly approved.

## 11. BLE Power Management

BLE communication should be optimized without compromising sensor reliability.

Possible optimizations include:

- Avoiding unnecessary scanning.
- Maintaining connections only when needed.
- Using suitable connection parameters.
- Avoiding unnecessary characteristic reads.
- Reducing diagnostic traffic.

Connected sensors must not be disconnected automatically during active recording unless required by a documented power policy.

BLE connection parameter changes must be validated against sensor compatibility.

## 12. Wi-Fi Power Management

Wi-Fi should be disabled when it is not required.

Possible operating policies:

- Disabled during normal riding.
- Enabled during synchronization.
- Enabled during configuration.
- Enabled during maintenance.
- Enabled during firmware update.

Wi-Fi activity must not interfere with GNSS, BLE, or activity recording.

Background reconnection attempts must use controlled retry intervals.

## 13. Display Power Management

The display is expected to be a significant contributor to overall power consumption.

The implementation should support:

- Configurable brightness levels.
- Automatic dimming.
- Display timeout.
- Display-off mode.
- Wake on touch.
- Wake on physical button, if available.

The UI must receive display state changes through an application-level interface.

Backlight control must be implemented through the Hardware/BSP abstraction.

## 14. Activity Recording and Low Battery

Activity recording has high priority during cycling.

When battery capacity becomes low:

1. Notify the user.
2. Disable nonessential connectivity operations.
3. Reduce display brightness.
4. Preserve GNSS and BLE operation where possible.
5. Continue recording while the system remains operational.
6. Prepare for safe shutdown if the battery becomes critical.

The system must define a minimum safe operating threshold based on real hardware measurements.

If the battery is too low to guarantee continued operation, the recorder should attempt to persist its current state before shutdown.

Do not wait until the battery is completely depleted before attempting data protection.

## 15. Power Management API

The following API is conceptual.

It must be reviewed against the existing project architecture before implementation.

```c id="m71zid"
esp_err_t power_manager_init(void);

esp_err_t power_manager_get_status(
    power_status_t *status
);

esp_err_t power_manager_set_state(
    power_state_t state
);

esp_err_t power_manager_request_display_dim(void);

esp_err_t power_manager_request_display_off(void);

esp_err_t power_manager_request_standby(void);

esp_err_t power_manager_request_wakeup(void);

esp_err_t battery_monitor_get_status(
    battery_status_t *status
);
```

These names and signatures are examples only.

The final API must define:

- State transition rules.
- Request priorities.
- Error handling.
- Asynchronous behavior.
- Callback execution context.
- Thread-safety guarantees.
- Hardware operation ownership.

Application components must not directly manipulate power-control GPIOs.

## 16. Task and Concurrency Model

Power monitoring and state coordination must not block time-critical tasks.

A conceptual architecture is:

```text id="7hr4gb"
Battery Monitor
       |
       v
Power Manager
       |
       v
Power Event Queue
       |
       v
Power Management Task
       |
       +------ Display Policy
       |
       +------ Connectivity Policy
       |
       +------ Peripheral Policy
       |
       +------ Shutdown Coordination
```

The implementation must define:

- Monitoring interval.
- Event queue capacity.
- State transition timing.
- Request priorities.
- Task stack size.
- Memory consumption.

Interrupt handlers must not perform complex power-management operations.

## 17. Diagnostics

The Power Management component should expose:

- Current power state.
- Battery voltage.
- Estimated battery percentage, if supported.
- Charging state.
- External power state.
- Display brightness.
- Enabled peripherals.
- Last battery measurement.
- Last power transition.
- Last power-management error.

Diagnostics must make it possible to understand why a peripheral was enabled or disabled.

## 18. Testing Requirements

### 18.1 Battery monitoring tests

- Valid battery measurement.
- Invalid ADC reading.
- ADC noise.
- Calibration accuracy.
- Measurement timeout.
- Stale measurement.
- Battery percentage estimation.

### 18.2 Charging tests

- External power connected.
- External power disconnected.
- Charging detected.
- Fully charged.
- Charging status unavailable.
- Operation while charging.

### 18.3 Power state tests

- Boot to active.
- Active to dimmed.
- Dimmed to display off.
- Display off to active.
- Active to standby.
- Standby to active.
- Low-power transition.
- Controlled shutdown.

### 18.4 Activity tests

- Power-saving transition during recording.
- Low battery during recording.
- Display off during recording.
- Wi-Fi disabled during recording.
- GNSS maintained during recording.
- BLE sensors maintained during recording.
- Storage flush during shutdown.

### 18.5 Hardware tests

Measure actual power consumption in:

- Boot.
- Idle.
- Active display.
- Dimmed display.
- Display off.
- GNSS active.
- BLE active.
- Wi-Fi active.
- microSD recording.
- Combined riding operation.
- Standby.

Measurements must be performed on the actual target hardware.

### 18.6 Performance tests

Measure:

- Battery monitoring overhead.
- Task stack usage.
- Heap consumption.
- State transition latency.
- Peripheral shutdown latency.
- Wake-up latency.
- Total current consumption.

## 19. Implementation Constraints

The Power Management agent must:

- Inspect the existing project before proposing changes.
- Verify the board schematic and power architecture.
- Verify battery monitoring hardware before implementing measurements.
- Never invent battery voltage thresholds.
- Never assume a fuel gauge exists.
- Keep hardware-specific operations inside Hardware/BSP or dedicated drivers.
- Avoid disabling peripherals required by active recording.
- Avoid unverified GNSS configuration commands.
- Preserve activity data during low-battery transitions.
- Measure actual power consumption.
- Document all hardware assumptions.

### 19.1 Approval policy

The project uses `approval_each`.

Therefore:

- Every individual file creation requires explicit approval.
- Every individual file modification requires explicit approval.
- Every deletion requires explicit approval.
- Every rename or move requires explicit approval.

Approval for one file does not authorize modifications to other files.

### 19.2 Execution policy

The agent must not execute commands without explicit approval.

This includes:

- Build commands.
- Test commands.
- Formatting commands.
- Flashing commands.
- Hardware interaction.
- GPIO manipulation.
- Power measurements.
- Firmware configuration changes.
- Git operations that modify repository state.

Read-only inspection may be performed within the project's established permissions.

## 20. Future Extensions

The architecture should allow:

- Battery fuel-gauge integration.
- Advanced battery health monitoring.
- Adaptive GNSS power management.
- Dynamic display brightness.
- Advanced standby modes.
- Deep sleep.
- Wake-up scheduling.
- Charging profiles.
- Power consumption analytics.
- Battery life estimation.

These features must be introduced incrementally and validated on actual hardware.

## 21. Definition of Done

The initial Power Management implementation can be considered complete when:

- Battery monitoring works with verified hardware.
- Battery measurement validity is correctly represented.
- Charging status is reported where supported.
- Power states are documented and implemented.
- Display dimming and timeout policies work.
- Optional peripherals can be managed safely.
- Active activity recording is protected.
- Low-battery behavior is predictable.
- Controlled shutdown preserves data where possible.
- Actual power consumption is measured and documented.
- Tests cover power transitions and failure scenarios.
- All modifications have received the required individual approval.