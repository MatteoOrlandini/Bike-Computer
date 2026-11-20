# CONNECTIVITY_SPECIFICATION.md

# Connectivity Specification

## 1. Purpose

The Connectivity component manages external communication for the Bike Computer.

It provides a unified architecture for network connectivity, smartphone communication, activity synchronization, and future integrations with external services.

The component must support offline operation and must never make core cycling functionality dependent on an Internet connection.

The initial implementation should focus on a reliable Wi-Fi foundation while keeping the architecture ready for future smartphone integration.

## 2. Scope

The Connectivity component is responsible for:

- Wi-Fi initialization and management.
- Network connection and disconnection handling.
- Network status monitoring.
- Communication with a companion application.
- Activity synchronization.
- Data transfer to external services through an appropriate integration layer.
- Network error handling.
- Connection diagnostics.

The Connectivity component is not responsible for:

- GNSS hardware communication.
- BLE cycling sensor communication.
- Cycling metric calculations.
- Activity recording.
- Direct filesystem management.
- UI rendering.
- Storing Strava client secrets or other backend credentials.

These responsibilities belong to other components.

## 3. Architectural Position

Connectivity is an independent application service.

Conceptual architecture:

```text
              Bike Computer
                    |
                    v
             Connectivity
                    |
          +---------+---------+
          |         |         |
          v         v         v
         Wi-Fi   Companion  Sync Manager
                   Link         |
                                v
                         External Services
                                |
                                v
                              Strava
```

The Connectivity component must communicate with other components through defined interfaces.

The UI must not directly manage Wi-Fi drivers or network sockets.

The Activity Recorder must not depend on Connectivity.

## 4. Design Principles

The implementation must follow these principles:

1. Offline-first operation.
2. Network operations must not block real-time cycling tasks.
3. All network operations must be asynchronous or appropriately bounded.
4. Network failures must not interrupt activity recording.
5. Reconnection must be handled automatically where appropriate.
6. Data transfers must be retryable.
7. Sensitive information must be protected.
8. Network protocols must be documented and versioned.
9. The device must remain usable without a paired smartphone.
10. External service integrations must remain independent from core firmware functionality.

## 5. Wi-Fi Management

### 5.1 Hardware capabilities

The target ESP32-S3 platform supports 2.4 GHz Wi-Fi.

The implementation must verify the actual board configuration and ESP-IDF Wi-Fi driver setup.

Do not assume that Wi-Fi is already initialized in the existing project.

### 5.2 Wi-Fi responsibilities

The Wi-Fi manager should provide:

- Initialization.
- Deinitialization.
- Connection to a configured network.
- Disconnection.
- Connection status monitoring.
- IP address retrieval.
- Signal strength monitoring, where available.
- Reconnection handling.
- Error reporting.

### 5.3 Connection states

The following states should be supported:

| State | Description |
|---|---|
| DISABLED | Wi-Fi is disabled |
| INITIALIZING | Driver initialization is in progress |
| DISCONNECTED | No active connection |
| CONNECTING | Connection attempt in progress |
| CONNECTED | Connected to an access point |
| OBTAINING_IP | Waiting for network configuration |
| READY | Network connection is usable |
| ERROR | An unrecoverable or reported error occurred |

A Wi-Fi association does not necessarily mean that Internet connectivity is available.

The implementation must distinguish between:

- Connected to an access point.
- Valid local IP configuration.
- Local network availability.
- Internet availability.

Internet availability checks should be optional and must not be required for normal device operation.

### 5.4 Connection configuration

The system should support configurable Wi-Fi credentials.

Credentials must not be hardcoded into source files.

Sensitive configuration must be stored using an appropriate protected storage mechanism.

The implementation must not expose passwords through logs or diagnostic interfaces.

### 5.5 Reconnection strategy

The Wi-Fi manager should support automatic reconnection.

The strategy should include:

- Retry limits or controlled retry behavior.
- Increasing retry intervals where appropriate.
- Handling access point disappearance.
- Handling authentication failures.
- Handling DHCP failures.
- Resetting retry state after successful connection.

Avoid aggressive reconnection loops that consume excessive power or CPU time.

## 6. Network Architecture

Network communication must be isolated from cycling-critical tasks.

The proposed architecture is:

```text
Application Components
         |
         v
Connectivity API
         |
         v
Network Manager
         |
         +----------------+
         |                |
         v                v
    Wi-Fi Driver     Protocol Layer
                           |
                  +--------+--------+
                  |                 |
                  v                 v
                 HTTP          Companion Protocol
```

The Connectivity API must hide implementation details such as:

- ESP-IDF network interfaces.
- TCP/IP sockets.
- HTTP clients.
- TLS configuration.
- Connection retry mechanisms.

Application components must not access these resources directly unless explicitly approved by the architecture.

## 7. Companion Application

A companion smartphone application is a future architectural objective.

It may provide:

- Device configuration.
- Activity synchronization.
- GPS assistance.
- Firmware update management.
- Sensor configuration.
- Route transfer.
- Device diagnostics.
- Activity upload to external platforms.

The firmware must not assume that the companion application already exists.

The communication protocol must be designed and versioned before implementation.

### 7.1 Communication transport

Possible transports include:

- Wi-Fi.
- BLE.
- A combination of Wi-Fi and BLE.

The final transport must be selected according to the use case.

For example:

| Use case | Candidate transport |
|---|---|
| Device discovery | BLE |
| Initial configuration | BLE or Wi-Fi |
| GPS assistance | BLE or Wi-Fi |
| Activity transfer | Wi-Fi |
| Large file transfer | Wi-Fi |
| Basic status information | BLE |

These are architectural proposals, not fixed implementation decisions.

### 7.2 Device identification

The device should expose a stable identifier.

The identifier must not unnecessarily expose personal information.

The companion application should be able to distinguish multiple Bike Computers.

### 7.3 Protocol versioning

The communication protocol must include a version identifier.

The protocol should support:

- Version negotiation.
- Unsupported-version handling.
- Message validation.
- Error responses.
- Request identifiers.
- Timeouts.
- Connection interruption handling.

Unknown message types must be rejected or safely ignored according to the protocol specification.

## 8. Smartphone GPS Assistance

Smartphone GPS is a planned future feature.

It must provide an alternative position source when the onboard GNSS receiver is unavailable or has poor reception.

The smartphone must send position data to the Bike Computer through the companion communication protocol.

### 8.1 Data requirements

A GPS assistance message should contain:

- Latitude.
- Longitude.
- Altitude, if available.
- Speed, if available.
- Heading, if available.
- Horizontal accuracy.
- Vertical accuracy, if available.
- Smartphone measurement timestamp.
- Message reception timestamp.
- Source identifier.

The protocol must distinguish between measurement time and reception time.

### 8.2 Position validation

The Bike Computer must validate received position data.

Validation should include:

- Coordinate range.
- Timestamp validity.
- Data freshness.
- Accuracy information.
- Message integrity.
- Plausibility checks.

Invalid or stale positions must not be forwarded as valid current measurements.

### 8.3 Source selection

Position source selection belongs to the Data Hub.

Connectivity must only provide smartphone position measurements with their associated metadata.

The Data Hub decides whether smartphone GPS should be used as the active position source.

### 8.4 Synchronization

The smartphone and Bike Computer may have different clocks.

The communication protocol must avoid assuming perfectly synchronized clocks.

Measurement timestamps and reception timestamps must be preserved separately.

The implementation must define how timestamp differences and clock uncertainty are handled.

## 9. Activity Synchronization

Activity synchronization is a future feature.

The Activity Recorder remains the authoritative owner of recorded activity data.

Connectivity is responsible for transferring activities when a suitable connection is available.

### 9.1 Synchronization workflow

Conceptual workflow:

```text
Activity Recorder
        |
        v
Completed Activity
        |
        v
Connectivity Sync Manager
        |
        v
Check Connection
        |
        v
Transfer Activity
        |
        v
Verify Transfer
        |
        v
Mark Sync Status
```

### 9.2 Synchronization states

Each activity may have a synchronization state:

- NOT_SYNCED.
- QUEUED.
- SYNCING.
- SYNCED.
- FAILED.
- RETRY_PENDING.

Synchronization status must not modify the original activity recording.

### 9.3 Transfer requirements

The synchronization system should support:

- Activity identification.
- File size verification.
- Transfer progress.
- Transfer interruption detection.
- Retry after connection loss.
- Duplicate detection.
- Transfer integrity verification.

The destination must not receive an activity marked as successfully transferred before verification is complete.

### 9.4 Offline queue

Activities should remain queued while connectivity is unavailable.

The synchronization manager must not require continuous Internet access.

Queued activities must survive device reboot.

The synchronization queue should be persistent or reconstructable from stored activity metadata.

### 9.5 Data deletion

Successful synchronization must not automatically delete the original activity.

Local activity deletion must remain an explicit operation governed by the Storage and Activity Recorder policies.

## 10. HTTP and REST Communication

The Connectivity component may use HTTP/HTTPS for communication with companion services and backends.

The implementation must use HTTPS for communication over untrusted networks.

### 10.1 HTTP client requirements

The HTTP client should support:

- GET.
- POST.
- PUT.
- DELETE, only where explicitly required.
- Configurable timeouts.
- Response validation.
- Error classification.
- Bounded request and response sizes.
- TLS certificate verification.

### 10.2 Error handling

The implementation must distinguish between:

- DNS resolution failures.
- Connection timeouts.
- Connection refused.
- TLS validation failures.
- HTTP client errors.
- HTTP server errors.
- Invalid responses.
- Malformed payloads.
- Interrupted transfers.

Network errors must not crash the application.

### 10.3 Retry strategy

Retries should be used only where appropriate.

The implementation must consider:

- Whether the operation is idempotent.
- Whether the request may have reached the server.
- Whether a duplicate operation could cause side effects.
- Whether the server supports request identifiers or idempotency keys.

Do not blindly retry non-idempotent operations.

## 11. Strava Integration

Strava integration is a planned future feature.

The firmware must not contain private client secrets.

OAuth authentication and activity uploads should preferably be handled by a companion application or backend.

### 11.1 Proposed workflow

```text
Bike Computer
      |
      v
Activity Recorder
      |
      v
FIT Export
      |
      v
Companion Application
      |
      v
OAuth Authentication
      |
      v
Strava API
      |
      v
Upload Result
      |
      v
Bike Computer Sync Status
```

### 11.2 Responsibilities

The Bike Computer should provide:

- Completed activity data.
- FIT export.
- Activity identification.
- Transfer status.

The companion application or backend should handle:

- OAuth authentication.
- Token storage.
- Token refresh.
- API requests.
- Upload retries.
- API error handling.
- External service rate limits.

### 11.3 Security requirements

Never embed private OAuth client secrets in firmware.

Never log access tokens or refresh tokens.

Use secure communication channels.

Authentication and authorization must be handled by the component responsible for the external service integration.

### 11.4 Upload status

The device may receive status updates such as:

- Upload pending.
- Upload in progress.
- Upload completed.
- Upload failed.
- Authentication required.

The Bike Computer must not assume that a successful local transfer means that the external platform has accepted the activity.

## 12. Network Security

Security must be considered from the beginning.

The implementation should support:

- HTTPS with certificate verification.
- Secure credential storage.
- Input validation.
- Bounded message sizes.
- Protocol version validation.
- Authentication for privileged operations.
- Protection against unauthorized configuration changes.

Avoid:

- Hardcoded passwords.
- Hardcoded private keys.
- Hardcoded OAuth secrets.
- Logging credentials.
- Disabling TLS certificate validation in production.
- Accepting arbitrary commands from unauthenticated clients.

The threat model must be reviewed before exposing device services to external networks.

## 13. Power Management

Connectivity can consume significant energy.

The implementation should consider:

- Wi-Fi enabled only when needed.
- Connection timeout.
- Retry intervals.
- Disabling unused network services.
- Avoiding continuous Internet polling.
- Limiting background transfers during critical operation.

The power management strategy must not interfere with:

- GNSS acquisition.
- BLE sensor operation.
- Activity recording.
- Display updates.

The system should support different connectivity policies for:

- Normal riding.
- Activity synchronization.
- Configuration.
- Maintenance.

These policies must be designed and tested before implementation.

## 14. Concurrency and Task Model

Network operations must run independently from time-critical tasks.

The preferred architecture is:

```text
Application Requests
        |
        v
Connectivity API
        |
        v
Bounded Request Queue
        |
        v
Connectivity Task
        |
        v
Network Operations
        |
        v
Result / Event Queue
        |
        v
Application Components
```

The implementation must define:

- Queue capacity.
- Request ownership.
- Timeout behavior.
- Cancellation behavior.
- Callback execution context.
- Result delivery.
- Memory limits.

Do not perform DNS resolution, TLS handshakes, HTTP requests, or file transfers inside sensor callbacks or LVGL rendering callbacks.

## 15. Proposed API

The following interface is conceptual.

It must be reviewed against the existing project architecture before implementation.

```c
esp_err_t connectivity_init(void);

esp_err_t connectivity_start(void);

esp_err_t connectivity_stop(void);

esp_err_t connectivity_wifi_connect(
    const char *ssid,
    const char *password
);

esp_err_t connectivity_wifi_disconnect(void);

esp_err_t connectivity_get_status(
    connectivity_status_t *status
);

esp_err_t connectivity_submit_request(
    const connectivity_request_t *request
);

esp_err_t connectivity_sync_activity(
    const char *activity_id
);

esp_err_t connectivity_cancel_operation(
    uint32_t operation_id
);
```

These names and signatures are examples only.

The final API must define:

- Synchronous versus asynchronous behavior.
- Request ownership.
- Buffer lifetime.
- Error handling.
- Operation identifiers.
- Timeout behavior.
- Result notification.
- Thread-safety guarantees.

## 16. Diagnostics

The Connectivity component should expose diagnostic information including:

- Wi-Fi state.
- SSID, where appropriate.
- IP address.
- Signal strength.
- Connection duration.
- Last connection error.
- Reconnection count.
- Pending synchronization operations.
- Active transfer.
- Last successful synchronization.
- Last network operation result.

Sensitive information must not be exposed through diagnostic logs.

Passwords, tokens, and private keys must never be logged.

## 17. Testing Requirements

### 17.1 Wi-Fi tests

- Successful connection.
- Invalid credentials.
- Access point unavailable.
- Access point disappears.
- DHCP failure.
- Reconnection.
- Manual disconnection.
- Wi-Fi disabled.
- Repeated connection attempts.

### 17.2 Network tests

- DNS failure.
- Connection timeout.
- Connection refused.
- HTTP error responses.
- Invalid response payloads.
- TLS certificate validation failure.
- Interrupted connection.
- Server unavailable.

### 17.3 Synchronization tests

- Activity transfer.
- Interrupted transfer.
- Retry after reboot.
- Duplicate transfer prevention.
- Invalid activity file.
- Transfer integrity verification.
- Multiple queued activities.
- Storage unavailable during transfer.

### 17.4 Companion communication tests

- Device discovery.
- Protocol version negotiation.
- Invalid message.
- Unsupported message type.
- Message timeout.
- Connection loss.
- Reconnection.
- GPS assistance with valid data.
- GPS assistance with stale data.
- Invalid coordinates.
- Clock differences.

### 17.5 Security tests

- Invalid authentication.
- Unauthorized configuration request.
- Malformed network message.
- Oversized payload.
- Invalid TLS certificate.
- Credential exposure in logs.

### 17.6 Performance tests

Measure:

- Wi-Fi initialization time.
- Connection time.
- Network throughput.
- CPU utilization.
- Heap usage.
- PSRAM usage.
- Task stack usage.
- Power consumption.
- Impact on GNSS and BLE processing.
- Impact on activity recording.

## 18. Implementation Constraints

The Connectivity agent must:

- Inspect the existing project before proposing changes.
- Verify the ESP-IDF networking configuration.
- Reuse existing network components where appropriate.
- Keep network operations independent from cycling-critical tasks.
- Avoid blocking GNSS, BLE, or LVGL processing.
- Preserve offline operation.
- Keep external-service credentials out of firmware.
- Avoid implementing an undocumented companion protocol.
- Avoid assuming that the companion application already exists.
- Avoid coupling activity recording to network availability.
- Document protocol and security decisions.

### 18.1 Approval policy

The project uses `approval_each`.

Therefore:

- Every individual file creation requires explicit approval.
- Every individual file modification requires explicit approval.
- Every deletion requires explicit approval.
- Every rename or move requires explicit approval.

Approval for one file does not authorize changes to other files.

### 18.2 Execution policy

The agent must not execute commands without explicit approval.

This includes:

- Build commands.
- Test commands.
- Formatting commands.
- Flashing commands.
- Hardware interaction.
- Network configuration changes.
- Git operations that modify repository state.

Read-only inspection may be performed within the project's established permissions.

## 19. Future Extensions

The architecture should support:

- Companion smartphone application.
- Smartphone GPS assistance.
- BLE-based device configuration.
- Wi-Fi activity transfer.
- Route import.
- Firmware updates.
- Strava integration.
- Cloud synchronization.
- Remote diagnostics.
- Weather information.
- Map and route downloads.

These features must be introduced incrementally.

## 20. Definition of Done

The initial Connectivity implementation can be considered complete when:

- Wi-Fi initialization and connection management work reliably.
- Connection states are exposed through a stable API.
- Network failures are handled safely.
- Reconnection behavior is documented and tested.
- Network operations do not block cycling-critical tasks.
- The device remains fully usable offline.
- Sensitive configuration is protected.
- Diagnostics are available.
- Resource and power consumption are measured.
- Tests cover connection failures and recovery.
- All modifications have received the required individual approval.