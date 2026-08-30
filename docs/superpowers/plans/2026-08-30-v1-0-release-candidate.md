# DASH ECHO v1.0 Release Candidate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce the first build-verified Windows DASH ECHO release candidate from the complete v0.1–v0.9 source set.

**Architecture:** v1.0 adds no new product subsystem. It stamps one coherent release version, installs a pinned/reproducible GitHub Actions Windows build gate, repairs every compiler/package defect revealed by that gate, and leaves in-game verification explicitly separate.

**Tech Stack:** C++23, Geode CLI v3.7.4, Geode SDK v5.10.1, GD 2.2081 Windows bindings, GitHub Actions `windows-latest`.

**Spec:** `docs/superpowers/specs/2026-08-30-v1-0-release-candidate-design.md`

## Global Constraints

- `main` is the only development/source branch; create no branches.
- v1.0 feature scope is frozen to the functionality implemented through v0.9.
- Build PASS requires terminal successful build evidence, not source inspection.
- In-game PASS requires Main-PC Geometry Dash evidence and cannot be inferred from CI.
- No save/account/input/physics/collision/completion authority mutation.

---

### Task 1: Stamp coherent v1.0 metadata

**Files:**
- Modify: `mod.json`
- Modify: `CMakeLists.txt`
- Modify: `README.md`
- Modify: `about.md`
- Create: `docs/V1_0_RELEASE_CANDIDATE.md`

- [ ] Set package version `v1.0.0` and CMake version `1.0.0`.
- [ ] Preserve Geode v5.10.1 and GD Windows 2.2081 requirements.
- [ ] Document the full v0.1–v0.9 feature set and split BUILD vs IN-GAME verification status.

### Task 2: Add pinned Windows CI build gate

**Files:**
- Create: `.github/workflows/build-v1.yml`

- [ ] Use `windows-latest` and `actions/checkout@v4`.
- [ ] Download `geode-cli-v3.7.4-win.zip` from the official Geode CLI release.
- [ ] Verify SHA-256 `467ac1fa975e391c32c894a7aff659949dbde5d32656bd13797f78f366da6c26` before extraction.
- [ ] Clone `geode-sdk/geode` tag `v5.10.1` and set `GEODE_SDK` for later steps.
- [ ] Install Windows Geode SDK binaries with the pinned CLI.
- [ ] Run `geode build --platform win --config Release`.
- [ ] Find generated `.geode` package(s) and upload them with `actions/upload-artifact@v4`.

### Task 3: Execute first build and repair defects

**Files:**
- Modify only files implicated by concrete compiler/package errors.

- [ ] Wait for the workflow triggered by the v1.0 CI commit to reach a terminal state.
- [ ] If failed, inspect job steps/logs and identify the first generating defect.
- [ ] Repair that defect on `main` without unrelated refactoring.
- [ ] Repeat until the Windows build is terminal successful or an external blocker is proven with logs.

### Task 4: Final repository evidence gate

**Evidence:**
- branch list
- `mod.json`
- CMake project version
- terminal workflow/job status
- build artifact listing

- [ ] Verify the repository still exposes only `main`.
- [ ] Verify `v1.0.0` metadata consistency.
- [ ] Verify build workflow terminal result.
- [ ] Verify `.geode` artifact exists when build succeeded.
- [ ] Report in-game verification as HOLD until Main-PC evidence exists.
