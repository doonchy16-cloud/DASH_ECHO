# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is a local Geometry Dash replay and training studio: multighost attempts, history, death intelligence, interactive replay timelines, playback controls, and cinematic camera modes.

## v0.9 — Cinematic Replay Camera

Replay Studio now has six source-level camera modes: Recorded, Follow, Smooth, Drone, Dynamic Zoom, and Death Cam. They are derived from the same immutable replay clip and authoritative timeline cursor that drive the replay ghost.

Recorded remains the deterministic compatibility fallback. Cinematic calculations consume recorded camera/player/death data rather than live ghost nodes, and invalid calculations fall back to the recorded viewport. The active live-attempt viewport is still restored when Replay Studio closes.

v1.0 is the first build/runtime verification gate.
