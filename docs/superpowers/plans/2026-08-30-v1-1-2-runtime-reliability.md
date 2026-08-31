# ECHO_DASH v1.1.2 Runtime Reliability Implementation Plan

## Goal
Harden replay persistence and load trust without changing the still-uncertified v1.1.1 Unified Ghost Engine runtime behavior.

## Execution policy
- main only
- TDD RED before production changes
- no package/release claim without terminal Windows evidence
- no in-game PASS without user runtime evidence

## Task 1 — Reliability contract (RED)

Modify `tests/test_v1_1_contract.py` to require:
- visible/source version 1.1.2;
- retained `.bak` known-good backup semantics;
- primary -> backup recovery path;
- semantic frame/replay validation;
- quarantine count telemetry;
- settings poll before Replay Studio early return;
- existing unified-engine continuation invariants remain present.

Push the test-only commit and require the workflow to fail at the contract step before compiler setup.

## Task 2 — Archive parsing extraction

Refactor archive load parsing into a private candidate loader that can parse either primary or backup into temporary containers without mutating authority until the candidate is accepted.

Acceptance:
- failed candidate leaves live archive containers untouched;
- structural parse failure is distinguishable from semantic replay quarantine;
- context mismatch rejects the candidate.

## Task 3 — Semantic validators

Add pure validation helpers for:
- player snapshot;
- camera snapshot;
- frame;
- replay;
- summary/death summary where applicable.

Acceptance:
- non-finite values rejected;
- replay timestamps monotonic non-decreasing;
- frame sequences strictly increasing;
- progress bounded to 0..100 rather than silently normalizing corrupt data;
- semantically invalid replay omitted/quarantined without rejecting unrelated structurally readable replays.

## Task 4 — Known-good backup lifecycle

Change save pipeline:
- serialize temp;
- validate temp candidate;
- rotate valid primary to retained `.bak`;
- do not overwrite good `.bak` with an invalid primary;
- install validated temp as primary;
- retain `.bak` after success.

Change load pipeline:
- try primary first;
- on primary failure, try `.bak`;
- if backup accepted, mark recovery telemetry and restore primary bytes when safe;
- if neither candidate exists, load empty archive;
- if both reject, return failure with empty safe authority.

## Task 5 — Runtime diagnostics and versioning

- bump `mod.json`, `CMakeLists.txt`, release constant, README/about/changelog to v1.1.2;
- expose `recoveredFromBackup` and `quarantinedReplayCount` in diagnostics/logging;
- preserve settings polling before Replay Studio early return.

## Task 6 — GREEN + Windows compile

Require:
- regression contract GREEN;
- CLI 3.7.4 GREEN;
- SDK 5.10.1 GREEN;
- Windows Release compile GREEN;
- compiler-evidence artifact GREEN;
- exactly one candidate `.geode` artifact GREEN.

## Task 7 — Independent package inspection

Before producing any user installer:
- verify artifact digest;
- verify `.geode` ZIP integrity;
- verify metadata version/id/geode/gd target;
- verify Windows x64 DLL PE signature;
- preserve approved `logo.png` exactly if CI raw package omits it;
- keep runtime behavior status HOLD until in-game testing.
