# NAVIGATION_SPECIFICATION.md

# Navigation Specification

## 1. Purpose

The Navigation component provides route management and turn-by-turn navigation capabilities for the Bike Computer.

The long-term objective is to provide a cycling navigation experience comparable to dedicated cycling computers.

The component must support offline navigation and must remain independent of the UI framework, GNSS hardware, and external network services.

The initial architecture must prioritize route handling and position-based navigation while allowing future integration of offline maps and advanced routing features.

## 2. Scope

The Navigation component is responsible for:

- Importing cycling routes.
- Parsing and validating GPX files.
- Managing stored routes.
- Maintaining the active navigation session.
- Matching the current position to a route.
- Calculating distance to route points and upcoming turns.
- Detecting deviations from the planned route.
- Generating navigation instructions.
- Providing navigation data to the UI.
- Supporting offline navigation.

The Navigation component is not responsible for:

- Reading GNSS hardware directly.
- Selecting the active GNSS source.
- Managing BLE sensors.
- Calculating cycling performance metrics.
- Managing persistent storage directly.
- Rendering maps or navigation graphics.
- Downloading map data directly.
- Managing network connections.

These responsibilities belong to other components.

## 3. Architectural Position

Navigation consumes normalized position information from the Data Hub and route-related information from the Storage and Connectivity components.

Conceptual architecture:

```text
              GNSS / Phone GPS
                      |
                      v
                   Data Hub
                      |
                      v
                Navigation Core
                      |
          +-----------+-----------+
          |           |           |
          v           v           v
       Route       Position    Guidance
      Manager      Matching    Engine
          |           |           |
          +-----------+-----------+
                      |
                      v
                 Navigation API
                      |
                      v
                    UI/UX
```

The Navigation Core must not depend on LVGL.

The UI must not parse GPX files or calculate route deviations.

## 4. Design Principles

The implementation must follow these principles:

1. Offline-first navigation.
2. Separation between route data and navigation state.
3. Navigation calculations must be independent of LVGL.
4. Invalid or stale position data must not generate misleading guidance.
5. Route deviations must be detected reliably.
6. Navigation must tolerate temporary GNSS signal loss.
7. Route data must be versioned and validated.
8. Large route files must not require loading unnecessary data into RAM.
9. Map rendering must remain independent from navigation calculations.
10. Navigation must never interfere with activity recording or sensor processing.

## 5. Navigation Features and Development Phases

Navigation should be developed incrementally.

### Phase 1: Route Management

- GPX import.
- GPX validation.
- Route metadata extraction.
- Route storage.
- Route listing.
- Route selection.
- Route deletion with explicit user approval.

### Phase 2: Basic Navigation

- Current position display.
- Distance to route.
- Distance to destination.
- Route deviation detection.
- Next route point.
- Basic directional guidance.

### Phase 3: Turn-by-Turn Navigation

- Turn detection.
- Turn classification.
- Distance to next turn.
- Navigation instruction generation.
- Advance warning.
- Off-route notifications.

### Phase 4: Map Display

- Offline map rendering.
- Route overlay.
- Current position marker.
- Map orientation.
- Zoom and pan.
- Map caching.

### Phase 5: Advanced Navigation

- Multiple route alternatives.
- Automatic rerouting.
- Elevation profile.
- Route surface information.
- Climb information.
- Route planning.
- Online route services.

Later phases must not be assumed to be part of the initial implementation.

## 6. Route Data Model

A route represents a planned cycling path.

The internal route model should support:

- Route identifier.
- Route name.
- Description.
- Source file.
- Import timestamp.
- Total distance.
- Elevation gain, if available.
- Elevation loss, if available.
- Starting position.
- Destination position.
- Route points.
- Waypoints.
- Track segments.
- Optional turn information.
- Optional surface information.

### 6.1 Route points

Each route point should contain:

| Field | Description |
|---|---|
| Latitude | Geographic latitude |
| Longitude | Geographic longitude |
| Elevation | Optional elevation in meters |
| Timestamp | Optional original GPX timestamp |
| Distance from start | Cumulative route distance |
| Point index | Position in the route |
| Flags | Optional route metadata |

The implementation should avoid storing redundant information unless required for performance.

### 6.2 Route segments

A route may contain multiple segments.

Segment boundaries must be preserved during import.

The Navigation Core must not automatically assume that disconnected GPX track segments form one continuous navigable path.

### 6.3 Waypoints

Waypoints may represent:

- Start location.
- Destination.
- Intermediate stops.
- Points of interest.
- User-defined markers.

Waypoint information must remain distinct from the route geometry.

## 7. GPX Import

GPX is the initial route import format.

The importer should support commonly used GPX structures.

At minimum:

- GPX track (`trk`).
- Track segments (`trkseg`).
- Track points (`trkpt`).
- Routes (`rte`).
- Route points (`rtept`).
- Waypoints (`wpt`).
- Elevation (`ele`).
- Metadata.
- Track and route names.

The parser must validate:

- XML structure.
- GPX version.
- Coordinate ranges.
- Missing coordinates.
- Empty tracks.
- Invalid numeric values.
- Excessively large files.
- Malformed XML.

Invalid data must not cause a crash or uncontrolled memory allocation.

### 7.1 GPX compatibility

GPX files may contain optional extensions.

Unknown extensions should be ignored safely unless explicitly supported.

The importer must not assume that every GPX file contains elevation or timestamps.

### 7.2 Import result

After parsing, the importer should provide:

- Import status.
- Route metadata.
- Number of points.
- Number of segments.
- Calculated route distance.
- Elevation availability.
- Validation warnings.
- Error information.

A route must not become available for navigation until validation is complete.

## 8. Route Storage

The Navigation component must use the Storage abstraction.

It must not access filesystem-specific APIs directly.

A proposed directory structure is:

```text
/storage/
    routes/
        <route_id>/
            route.gpx
            route_metadata.dat
            route_index.dat
```

This structure is conceptual.

The final format must consider:

- Storage capacity.
- File size.
- Random access requirements.
- Recovery behavior.
- Route loading time.
- Memory consumption.

Original imported GPX files should be preserved where possible.

Any generated route index must be reproducible from the original route data.

Deleting routes requires explicit user approval.

## 9. Route Geometry Processing

Route geometry must be processed before or during navigation.

Possible preprocessing operations include:

- Coordinate validation.
- Distance calculation.
- Segment indexing.
- Bounding-box calculation.
- Point simplification for display.
- Spatial indexing.
- Elevation profile generation.

### 9.1 Distance calculation

Geographic distances must be calculated using an appropriate geodesic approximation.

The implementation may use the Haversine formula for short-range distance calculations.

The calculation must account for the curvature of the Earth.

All internal distance values should use meters.

### 9.2 Route simplification

Route simplification may be used to reduce memory consumption and improve rendering performance.

However:

- Original route geometry must remain available.
- Simplification must preserve route shape.
- Navigation-critical points must not be discarded.
- Turn information must not be lost.

The simplification algorithm and tolerance must be documented.

### 9.3 Large routes

The implementation must support routes containing many thousands of points.

Do not assume that the entire route can always be loaded into internal RAM.

Consider:

- Chunked loading.
- Simplified display geometry.
- Spatial indexing.
- Storage-backed route access.
- Bounded caches.

The final strategy must be validated against available RAM and PSRAM.

## 10. Position Matching

Position matching determines where the current GNSS position lies relative to the planned route.

The Navigation Core must consume position measurements from the Data Hub.

Each position measurement must include validity and freshness information.

### 10.1 Route projection

The current position should be projected onto the nearest plausible route segment.

The result should include:

- Matched route segment.
- Projected position.
- Distance from route.
- Distance along route.
- Position matching confidence.

The implementation must not blindly select the geographically nearest point when route geometry contains:

- Parallel roads.
- Overlapping route sections.
- Crossings.
- Hairpin turns.
- Nearby return paths.

### 10.2 Matching continuity

Position matching should consider previous navigation state.

The algorithm should prefer plausible forward or backward progression along the route.

It must still support legitimate movement in either direction when the user backtracks.

### 10.3 Position accuracy

GNSS accuracy must influence matching confidence.

When position accuracy is poor:

- Reduce confidence.
- Avoid unnecessary route deviation warnings.
- Preserve the previous valid navigation state where appropriate.
- Report reduced positioning confidence.

Do not treat an inaccurate position as precise.

### 10.4 GNSS signal loss

When position becomes unavailable:

- Preserve the active route.
- Preserve the last valid navigation state.
- Suspend position-dependent calculations.
- Mark navigation guidance as temporarily unavailable.
- Resume matching when valid position data returns.

The component must not fabricate position updates.

## 11. Route Deviation Detection

The Navigation Core must detect when the cyclist moves away from the planned route.

Deviation detection should consider:

- Distance from route.
- Position accuracy.
- Position freshness.
- Consecutive measurements.
- Route geometry.
- Current movement direction.

### 11.1 Hysteresis

Use hysteresis to avoid repeated transitions between on-route and off-route states.

A single inaccurate position must not immediately trigger a definitive deviation warning.

The thresholds must be configurable and validated during testing.

### 11.2 Navigation states

Suggested states:

| State | Description |
|---|---|
| NO_ROUTE | No route selected |
| ROUTE_LOADED | Route loaded but navigation not started |
| NAVIGATING | Active navigation |
| POSITION_UNAVAILABLE | Position temporarily unavailable |
| LOW_CONFIDENCE | Position matching is uncertain |
| OFF_ROUTE_CANDIDATE | Possible deviation detected |
| OFF_ROUTE | Confirmed route deviation |
| ARRIVED | Destination reached |
| COMPLETED | Navigation session ended |

The state machine must define valid transitions.

## 12. Turn-by-Turn Navigation

Turn-by-turn guidance provides instructions for upcoming route changes.

The initial implementation may generate basic instructions from route geometry.

Advanced road-network-aware instructions require additional route metadata or map data.

### 12.1 Turn detection

Turn detection may use:

- Direction changes between route segments.
- Heading changes.
- Segment geometry.
- Turn angle.
- Distance between route points.

The implementation must avoid generating false turns due to GPS noise or small route variations.

### 12.2 Turn classification

Suggested turn types:

- STRAIGHT.
- SLIGHT_LEFT.
- LEFT.
- SHARP_LEFT.
- U_TURN_LEFT.
- SLIGHT_RIGHT.
- RIGHT.
- SHARP_RIGHT.
- U_TURN_RIGHT.
- ROUNDABOUT.
- DESTINATION.

These categories are conceptual.

The final classification rules must be documented and tested.

### 12.3 Turn instruction data

A navigation instruction should contain:

- Instruction identifier.
- Turn type.
- Distance to turn.
- Route position.
- Optional street or path name.
- Optional roundabout exit number.
- Instruction validity.
- Confidence.

### 12.4 Advance warnings

The system should provide advance warnings before an upcoming turn.

Warning distances should adapt to cycling speed and device configuration.

Avoid announcing the same instruction repeatedly unless the navigation state requires it.

### 12.5 Limitations

Geometry-based turn detection cannot reliably identify:

- Road names.
- Road priorities.
- Roundabout exit numbers.
- Cycleway access restrictions.
- Traffic restrictions.
- Whether a turn is physically accessible.

These features require suitable map or route metadata.

The UI must not present inferred information as authoritative road information.

## 13. Distance and Progress Calculations

The Navigation Core should provide:

- Distance to destination.
- Distance to next turn.
- Distance from route start.
- Remaining route distance.
- Route completion percentage.
- Current route segment.
- Estimated arrival time, if supported.

All distance values must use meters internally.

Display unit conversion belongs to the UI or a shared unit-management service.

### 13.1 Progress tracking

Progress should be calculated using the matched position along the route.

Do not calculate progress solely from the nearest route point index.

The algorithm must support:

- GPS noise.
- Temporary signal loss.
- Backtracking.
- Route crossings.
- Parallel road sections.

### 13.2 Estimated arrival time

Estimated arrival time may be introduced later.

It must be based on documented assumptions and reliable measurements.

The system must not display misleading precision when the estimate is unreliable.

## 14. Map Data and Rendering

Map rendering is separate from navigation calculations.

The Navigation Core provides route geometry and navigation state.

The UI/Map Renderer is responsible for visual presentation.

### 14.1 Offline maps

Offline maps are a future objective.

The implementation must evaluate suitable map data formats and rendering approaches before selecting a solution.

Possible considerations include:

- Vector tiles.
- Raster tiles.
- Offline map packages.
- Region-based map storage.

The solution must account for:

- Storage capacity.
- RAM and PSRAM usage.
- Rendering performance.
- Geographic coverage.
- Licensing.
- Map update strategy.

Do not assume that an Internet-based map service will be available while cycling.

### 14.2 Map renderer responsibilities

The renderer should support:

- Current position marker.
- Route line.
- Upcoming route section.
- Destination marker.
- Waypoints.
- Zoom level.
- Map orientation.
- Optional elevation or route overlays.

The renderer must not calculate route deviation or turn instructions.

### 14.3 Display constraints

The target display is an 800x480 RGB panel.

Map rendering must be optimized for the ESP32-S3 platform.

The implementation must consider:

- Framebuffer memory.
- PSRAM availability.
- Partial rendering.
- Tile caching.
- Refresh rate.
- LVGL integration.
- Memory bandwidth.

Map rendering must not interfere with activity recording or sensor processing.

## 15. Navigation API

The following API is conceptual and must be reviewed before implementation.

```c
esp_err_t navigation_init(void);

esp_err_t navigation_load_route(
    const char *route_id
);

esp_err_t navigation_start(void);

esp_err_t navigation_pause(void);

esp_err_t navigation_resume(void);

esp_err_t navigation_stop(void);

esp_err_t navigation_get_status(
    navigation_status_t *status
);

esp_err_t navigation_get_guidance(
    navigation_guidance_t *guidance
);

esp_err_t navigation_get_route_info(
    navigation_route_info_t *info
);

esp_err_t navigation_import_gpx(
    const char *file_path
);
```

These function names and signatures are examples only.

The final interface must define:

- Input ownership.
- Output ownership.
- Synchronous or asynchronous behavior.
- Route-loading behavior.
- Error handling.
- Thread safety.
- Position update mechanism.
- Event notification mechanism.

The Navigation Core must not expose internal data structures directly to LVGL.

## 16. UI Integration

The UI should receive navigation state through a stable application interface.

Possible navigation screens include:

- Route selection.
- Route overview.
- Map view.
- Turn-by-turn guidance.
- Route elevation profile.
- Navigation summary.

The UI should display:

- Current navigation status.
- Distance to next turn.
- Turn direction.
- Distance to destination.
- Route progress.
- Off-route warning.
- Position accuracy or availability.

The UI must distinguish between:

- Valid guidance.
- Uncertain guidance.
- Temporarily unavailable guidance.

Navigation updates must not require continuous full-screen refreshes.

## 17. Concurrency and Task Model

Navigation processing must be independent from UI rendering.

A conceptual architecture is:

```text
Data Hub
   |
   v
Position Update Queue
   |
   v
Navigation Task
   |
   +------ Route Matching
   |
   +------ Deviation Detection
   |
   +------ Turn Guidance
   |
   v
Navigation State
   |
   v
UI Update Queue
```

The implementation must define:

- Position update frequency.
- Queue capacity.
- Processing priority.
- Maximum execution time.
- Memory usage.
- Thread synchronization.

Large GPX parsing operations must not block the UI or GNSS processing.

## 18. Diagnostics

The Navigation component should expose:

- Selected route ID.
- Route point count.
- Current navigation state.
- Current matched segment.
- Distance from route.
- Distance to destination.
- Distance to next turn.
- Position matching confidence.
- Last valid position timestamp.
- Last navigation error.
- Route import errors.
- Route deviation count.

Diagnostics should support troubleshooting without requiring persistent logging of every position update.

## 19. Testing Requirements

### 19.1 GPX tests

- Valid GPX track.
- Valid GPX route.
- Multiple track segments.
- Missing elevation.
- Missing timestamps.
- Empty GPX file.
- Malformed XML.
- Invalid coordinates.
- Very large GPX file.
- Unsupported GPX extensions.

### 19.2 Geometry tests

- Distance calculations.
- Route segment projection.
- Route crossings.
- Parallel routes.
- Hairpin turns.
- Overlapping route sections.
- Route simplification.
- Segment boundaries.

### 19.3 Position matching tests

- Accurate GNSS position.
- Noisy GNSS position.
- Low-accuracy position.
- Stale position.
- Position loss.
- Position recovery.
- Backtracking.
- Sudden position jump.

### 19.4 Deviation tests

- Small temporary deviation.
- Confirmed route deviation.
- Return to route.
- GPS noise near deviation threshold.
- Parallel road.
- Route crossing.

### 19.5 Turn guidance tests

- Straight route.
- Left turn.
- Right turn.
- Sharp turn.
- U-turn.
- Closely spaced turns.
- Roundabout geometry.
- Destination approach.
- Repeated instruction prevention.

### 19.6 Performance tests

Measure:

- GPX parsing time.
- Route loading time.
- Position matching execution time.
- Navigation task stack usage.
- Heap usage.
- PSRAM usage.
- Map rendering performance.
- Impact on LVGL refresh.
- Impact on GNSS processing.
- Impact on activity recording.

Tests must include large representative routes.

## 20. Implementation Constraints

The Navigation agent must:

- Inspect the existing project before proposing implementation changes.
- Keep navigation calculations independent of LVGL.
- Consume normalized position data from the Data Hub.
- Avoid direct access to GNSS hardware.
- Use the Storage abstraction for route persistence.
- Avoid assuming that offline map support already exists.
- Avoid loading unnecessarily large routes entirely into RAM.
- Preserve original imported route data.
- Handle invalid and stale position data correctly.
- Avoid blocking sensor processing or activity recording.
- Document algorithm limitations and assumptions.

### 20.1 Approval policy

The project uses `approval_each`.

Therefore:

- Every individual file creation requires explicit approval.
- Every individual file modification requires explicit approval.
- Every deletion requires explicit approval.
- Every rename or move requires explicit approval.

Approval for one file does not authorize modifications to other files.

### 20.2 Execution policy

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

## 21. Future Extensions

The architecture should allow:

- Offline vector maps.
- Automatic rerouting.
- Route import through a companion application.
- Route planning.
- Elevation profiles.
- Climb categorization.
- Surface information.
- Points of interest.
- Advanced turn-by-turn instructions.
- Multiple route alternatives.
- Route synchronization with external platforms.

These features must be introduced incrementally.

## 22. Definition of Done

The initial Navigation implementation can be considered complete when:

- GPX routes can be imported and validated.
- Route metadata is extracted correctly.
- Routes can be stored and loaded.
- Navigation can be started and stopped.
- Current position can be matched to the planned route.
- Distance to route and destination are calculated.
- Route deviations are detected reliably.
- Basic directional guidance is generated.
- Invalid and stale position data are handled correctly.
- Navigation remains independent of LVGL.
- Resource consumption is measured and documented.
- Tests cover route parsing, matching, deviation, and guidance.
- All modifications have received the required individual approval.