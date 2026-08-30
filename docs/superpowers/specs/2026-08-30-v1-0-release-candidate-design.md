# DASH ECHO v1.0 — Release Candidate / First Verification Gate Design

Status: **APPROVED BY USER GO / MAIN ONLY**

## Goal

Turn the v0.9 source-complete feature set into the first reproducibly build-verified DASH ECHO release candidate without adding unrelated product scope.

## Release doctrine

v1.0 is a hardening milestone, not a feature-expansion milestone.

It must:

1. preserve v0.1–v0.9 architecture and safety boundaries,
2. stamp one coherent `v1.0.0` product version,
3. add reproducible Windows build verification,
4. use real compiler/package evidence to find and repair source/API defects,
5. produce a `.geode` package artifact when the build succeeds,
6. keep build verification distinct from in-game verification.

## Verification hierarchy

### Gate A — Source/metadata consistency

Before CI:

- `mod.json` version is `v1.0.0`
- CMake project version is `1.0.0`
- Geode SDK requirement remains pinned to `v5.10.1`
- GD Windows requirement remains `2.2081`
- documentation describes the same feature set and authority boundaries
- repository still has only `main`

### Gate B — Reproducible Windows build

GitHub Actions on `windows-latest` will:

1. check out `main`,
2. download the exact Geode CLI **v3.7.4 Windows ZIP**,
3. verify SHA-256 `467ac1fa975e391c32c894a7aff659949dbde5d32656bd13797f78f366da6c26`,
4. clone Geode SDK tag `v5.10.1` into a runner-local SDK path,
5. set `GEODE_SDK`,
6. install required Windows SDK binaries through the Geode CLI,
7. run the canonical `geode build` Windows Release build,
8. locate generated `.geode` package(s),
9. upload them as a workflow artifact.

The workflow uses only `main`; it creates no development branch.

### Gate C — Defect-repair loop

If Gate B fails:

- inspect the exact failing compiler/package step,
- repair the generating source/API defect on `main`,
- rerun the build,
- repeat until the build is terminal PASS or an external toolchain blocker is proven.

Do not call build PASS from queued/running state.

### Gate D — In-game verification

A GitHub runner cannot truthfully prove Geometry Dash runtime behavior. Build PASS therefore does **not** imply in-game PASS.

The generated `.geode` artifact is the v1.0 Main-PC test candidate. Required in-game checks include:

- normal level startup/reset/death/completion/exit
- one and multiple ghosts
- dual mode
- death markers and analytics
- Replay Studio open/close and viewport restoration
- pause/resume/scrub/frame stepping and all five replay speeds
- Recorded/Follow/Smooth/Drone/Dynamic Zoom/Death Cam
- backward scrub discontinuities
- portals/teleports/camera triggers
- PB retention/selection
- no save/account/physics/collision/completion regressions
- compatibility smoke test with the normal Geode environment

Until those are confirmed on the Main PC, v1.0 status is **BUILD VERIFIED / IN-GAME HOLD**, not full PASS.

## Hardening changes allowed in v1.0

Only changes needed for:

- compile/package correctness
- deterministic lifecycle safety
- clear diagnostics
- configuration/schema correctness
- fail-open behavior
- packaging/CI reproducibility
- documentation/version consistency

New unrelated product features are out of scope.

## Artifact policy

The CI artifact is the canonical v1.0 candidate package produced from `main`. It is not silently installed anywhere by CI.

## Safety invariants

v1.0 must not intentionally modify:

- Geometry Dash saves/account data
- live player input
- live physics/collision authority
- completion authority
- unrelated mods

Replay Studio camera changes remain scoped to Studio and restore the active viewport on close.
