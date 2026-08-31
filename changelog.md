# v1.1.1

## Runtime UX & Persistence Repair

- Moved Replay Studio access out of live gameplay and into the Geometry Dash pause menu.
- Removed the priority aura/halo renderer and all aura settings completely.
- Preserved the blue Last Attempt trail and golden Best Recorded Echo trail as the clean priority identity system.
- Made Attempt #1 use the same explicit begin-and-sample lifecycle as every later attempt.
- Persist every successful finalized attempt before the next attempt begins.
- Raised default replay retention to 10,000 runs with a 100,000-run safety ceiling and storage-budget protection.
- Reorganized Replay Studio into a clearer two-row control layout.
- Kept GD Level PB, Best Recorded Echo, Session Best, and Latest Attempt as separate truth concepts.

Runtime verification remains a separate gate.

# v1.1.0

## ECHO_DASH — Multiverse & Personalization Rebuild

- Renamed the user-facing product from DASH ECHO to **ECHO_DASH** while preserving the legacy Geode ID for upgrade/save compatibility.
- Raised the supported ghost-count target from 6 to **256** through a dynamic pooled renderer.
- Added blue Last Attempt and golden Best Recorded Echo priority identities.
- Added configurable priority trails, opacity, aura sizing, age fade and visual profiles.
- Added configurable recorder sampling and event-frame capture.
- Added a versioned per-level/per-mode persistent history and replay archive design.
- Separated Geometry Dash Level PB, Best Recorded Echo, Session Best and Latest Attempt semantics.
- Expanded settings into a categorized ECHO_DASH Control Center.
- Redesigned first-death presentation and added a screen-space death heat strip.
- Improved Replay Studio discoverability, settings access and archived-attempt navigation.
- Added v1.1 source/config regression tests before the pinned Windows Release build.

Runtime verification remains a separate gate; the v1.1 package must still be exercised in Geometry Dash after build verification.