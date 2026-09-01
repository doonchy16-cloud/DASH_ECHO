# ECHO_DASH Hardening 05 — Release and Certification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Promote the completed hardening work as one immutable, hash-verifiable, rollback-safe ECHO_DASH v1.1.3 candidate and certify the exact same bytes through build, package, install, runtime, stress, and soak gates.

**Architecture:** Freeze release surfaces at v1.1.3, generate a machine-readable manifest from the exact CI-built `.geode`, package that unchanged file into a predictable upgrade ZIP, and use ASCII-safe PowerShell 5.1-compatible transactional installer/diagnostic scripts. Release promotion is evidence-driven: any byte change creates a new candidate and invalidates stronger evidence gathered on the old bytes.

**Tech Stack:** C++23/Geode build, Python 3 `zipfile`/`hashlib` release tooling, GitHub Actions `windows-latest`, Windows PowerShell 5.1 parser/runtime compatibility, SHA-256, existing Geode `.geode` ZIP packaging.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plans 01-04 are terminal GREEN with no unresolved runtime FAIL.
- Target version is exactly `v1.1.3`; mod ID remains exactly `doonchy.dash-echo`.
- Geode target remains 5.10.1; Geometry Dash Windows target remains 2.2081; DLL architecture remains x86-64.
- The CI-built `.geode` is canonical. Installer/package tools may copy it but may never modify/repack it.
- Required identity chain: CI package SHA256 == upgrade-ZIP payload package SHA256 == installed package SHA256.
- Upgrade cleanup may remove/replace only `.geode` files whose embedded `mod.json.id` equals `doonchy.dash-echo`; filenames alone are never deletion authority.
- Replay/history/user-data files are not part of installer cleanup.
- Executable `.ps1`/`.cmd` files shipped in v1.1.3 are ASCII-only and CRLF-normalized for Windows PowerShell 5.1 safety.
- Installer progress is stage-based; no fake percentage is displayed.
- Diagnostics script is read-only and never repairs automatically.
- A failed upgrade restores the prior verified installation byte-for-byte when one existed.
- No release/runtime/stress PASS is inferred from successful source/build/package/install gates.

---

## File Structure for This Plan

**Create:**
- `tools/release/inspect_geode.py` — inspect metadata, hashes, icon, DLL path, and x86-64 PE machine type.
- `tools/release/generate_manifest.py` — generate canonical `RELEASE_MANIFEST.json` from a built package and source SHA.
- `tools/release/package_upgrade.py` — build predictable outer ZIP without altering the `.geode` payload.
- `release/upgrade ECHO_DASH.ps1`
- `release/upgrade ECHO_DASH.cmd`
- `release/diagnose ECHO_DASH.ps1`
- `release/diagnose ECHO_DASH.cmd`
- `release/README.txt`
- `tests/test_release_pipeline.py`
- `tests/test_installer_assets.py`
- `tests/powershell/test_installer.ps1` — filesystem-level installer/rollback matrix without Pester dependency.
- `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md` — immutable-candidate evidence ledger populated during execution.

**Modify:**
- `mod.json`
- `CMakeLists.txt`
- `src/main.cpp`
- `.github/workflows/build-v1.yml`
- `README.md`
- `about.md`
- `changelog.md`
- `tests/test_v1_1_contract.py`
- `tests/test_release_assets.py`

---

### Task 1: Freeze v1.1.3 release identity, code hygiene, and warning policy

**Files:**
- Modify: `mod.json`
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Modify: `.github/workflows/build-v1.yml`
- Modify: `README.md`
- Modify: `about.md`
- Modify: `changelog.md`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces: consistent visible/source/build version `v1.1.3`; warning-clean ECHO_DASH-owned build policy.

- [ ] **Step 1: Change the contract first and prove RED**

Update branding/version test expectations to:

```python
self.assertEqual(metadata["version"], "v1.1.3")
self.assertIn("VERSION 1.1.3", self.read("CMakeLists.txt"))
self.assertRegex(main, r'kReleaseVersion\s*=\s*"v1\.1\.3"')
```

Update workflow artifact/string expectations to v1.1.3. Run the Python suite; expected FAIL while production files still say v1.1.2.

- [ ] **Step 2: Update all release surfaces together**

Set:

```text
mod.json version = v1.1.3
CMake project version = 1.1.3
kReleaseVersion = v1.1.3
workflow name/artifacts = v1.1.3
```

Update docs/changelog to describe this as reliability/UX/performance hardening with no new gameplay feature scope.

- [ ] **Step 3: Add project-owned MSVC warning options**

In CMake, for MSVC only:

```cmake
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
endif()
```

Do not globally promote third-party Geode SDK warnings to errors. Review/fix ECHO_DASH-owned warnings in the compiler log; do not hide them with broad warning disables.

- [ ] **Step 4: Run dead-surface/comment audit**

Remove obsolete v1.1.1/v1.1.2 implementation-history comments from live logic when they only describe archaeology; retain comments that explain current invariants. Search for stale unused settings fields/helpers/flags and either wire them to the approved behavior or remove them.

- [ ] **Step 5: Prove GREEN and commit**

Run Python/native tests plus Windows Release build.

```bash
git add mod.json CMakeLists.txt src/main.cpp .github/workflows/build-v1.yml README.md about.md changelog.md tests/test_v1_1_contract.py
git commit -m "chore: prepare ECHO_DASH v1.1.3 hardening release"
```

---

### Task 2: Build package inspection and canonical release manifest tooling

**Files:**
- Create: `tools/release/inspect_geode.py`
- Create: `tools/release/generate_manifest.py`
- Create: `tests/test_release_pipeline.py`
- Modify: `tests/test_release_assets.py`

**Interfaces:**
- Produces CLI:

```text
python tools/release/inspect_geode.py <package.geode> --json <report.json>
python tools/release/generate_manifest.py <package.geode> --source-commit <sha> --installer-version v6 --output <RELEASE_MANIFEST.json>
```

Manifest schema exactly:

```json
{
  "schema": 1,
  "product": "ECHO_DASH",
  "version": "v1.1.3",
  "mod_id": "doonchy.dash-echo",
  "source_commit": "<40-hex commit>",
  "geode": "5.10.1",
  "geometry_dash_windows": "2.2081",
  "architecture": "x86-64",
  "package_filename": "doonchy.dash-echo.geode",
  "package_sha256": "<64-hex>",
  "dll_path": "<path inside geode>",
  "dll_sha256": "<64-hex>",
  "logo_sha256": "<64-hex>",
  "installer_version": "v6"
}
```

- [ ] **Step 1: Write failing package-inspection tests**

Create a temporary ZIP fixture with `mod.json`, `logo.png`, and a tiny fake PE64 byte stream. Test extraction of ID/name/version/targets, package hash, logo hash, DLL hash, and PE machine `0x8664` -> `x86-64`. Test rejection of x86 machine `0x014c`, missing logo, duplicate DLL candidates, wrong mod ID, and malformed mod.json.

- [ ] **Step 2: Prove RED and implement `inspect_geode.py` using only Python stdlib**

Use `zipfile`, `json`, `hashlib`, `struct`, and `pathlib`. Read PE `e_lfanew` from offset `0x3c`, verify `PE\0\0`, then read the 16-bit machine field.

- [ ] **Step 3: Implement manifest generation as a pure transformation of inspection output + source SHA**

Reject non-40-hex source SHA, unexpected package metadata, or missing required hashes. JSON output uses sorted keys and UTF-8 with final newline for deterministic content given identical inputs.

- [ ] **Step 4: Extend release-asset test to keep source icon authoritative**

Preserve the current exact logo hash/dimensions contract unless the approved artwork itself is deliberately changed in a separate request. Package inspection must prove the packaged logo hash equals source `logo.png` hash.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add tools/release tests/test_release_pipeline.py tests/test_release_assets.py
git commit -m "build: add ECHO_DASH package manifest tooling"
```

---

### Task 3: Implement the ASCII-safe transactional v6 upgrade installer

**Files:**
- Create: `release/upgrade ECHO_DASH.ps1`
- Create: `release/upgrade ECHO_DASH.cmd`
- Create: `release/README.txt`
- Create: `tests/test_installer_assets.py`

**Interfaces:**
- PowerShell parameters:

```powershell
param(
    [string]$GeometryDashPath = ""
)
```

- Required functions inside the script:

```text
Write-Stage
Assert-PayloadManifest
Find-GeometryDashRoot
Get-GeodeModIdentity
Find-EchoDashPackages
Test-TargetWritable
Backup-ExistingEchoDash
Install-CanonicalPackage
Restore-Backup
Verify-InstalledState
```

- [ ] **Step 1: Write asset-format tests and prove RED**

`tests/test_installer_assets.py` asserts both executable scripts are strict ASCII, `.ps1`/`.cmd` use CRLF with no bare LF, and required stage/function names exist. Run Python tests; expected FAIL before files exist.

- [ ] **Step 2: Implement deterministic preflight before mutation**

Preflight order:

```text
locate payload + manifest beside script
verify manifest schema/product/version/mod ID/target values
SHA256 payload and compare manifest
open payload as ZIP and verify embedded mod.json identity/version
verify GeometryDash.exe + geode/mods target
verify Geometry Dash process is not running
scan every existing *.geode by embedded mod.json ID
verify destination write access using a disposable temp file
```

If any preflight fails, exit nonzero and state that the current installation was not changed.

- [ ] **Step 3: Implement deterministic target discovery**

When `-GeometryDashPath` is supplied, require that exact directory to contain `GeometryDash.exe` and `geode\mods`. Otherwise search in this order:

1. parent installation inferred from an existing Geode mods directory only when uniquely resolvable;
2. Steam library paths parsed from `steamapps\libraryfolders.vdf` under the standard Steam root;
3. standard `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash`.

Reject ambiguous multiple valid installations rather than guessing; tell the user to rerun PowerShell with `-GeometryDashPath`.

- [ ] **Step 4: Implement ID-based backup and mutation**

For every existing package whose embedded `mod.json.id` is exactly `doonchy.dash-echo`, compute SHA256 and copy it to a timestamped backup directory under `geode\mods\.echo-dash-backup\`. Verify every backup hash before deleting/replacing any existing ECHO_DASH package.

Do not touch packages with other IDs and do not touch Geode save/user-data directories.

- [ ] **Step 5: Implement transactional install and automatic rollback**

Copy canonical payload to a temporary file in the mods directory, verify its hash, remove only backed-up matching-ID packages, move temp to `doonchy.dash-echo.geode`, verify installed hash equals manifest, scan and require exactly one installed matching ID/version. On any failure after mutation begins, remove the new file and restore all prior backed-up packages byte-for-byte; verify restored hashes before reporting rollback success.

- [ ] **Step 6: Keep installer animation professional and parser-safe**

Use stage lines such as:

```text
[1/6] Checking package...
[2/6] Finding Geometry Dash...
[3/6] Backing up existing ECHO_DASH...
[4/6] Installing...
[5/6] Verifying...
[6/6] Complete.
```

Optional spinner characters are ASCII only: `| / - \`. Do not output Unicode checkmarks, box-drawing characters, or fake percentages.

- [ ] **Step 7: Implement the CMD wrapper**

The CMD file locates its sibling PowerShell script and invokes Windows PowerShell with `-NoProfile -ExecutionPolicy Bypass -File`, forwarding `%*`. On nonzero exit it keeps the console open with a concise error prompt; on success it reports completion.

- [ ] **Step 8: Prove GREEN and commit**

```bash
git add release tests/test_installer_assets.py
git commit -m "feat: add transactional ECHO_DASH v6 installer"
```

---

### Task 4: Implement the read-only diagnostic script and installer matrix test

**Files:**
- Create: `release/diagnose ECHO_DASH.ps1`
- Create: `release/diagnose ECHO_DASH.cmd`
- Create: `tests/powershell/test_installer.ps1`
- Modify: `tests/test_installer_assets.py`

**Interfaces:**
- Diagnostic parameter: `[string]$GeometryDashPath = ""` with the same deterministic discovery/validation rules as installer.
- Diagnostic output reads only package identity/version/hash/manifest consistency/duplicate matching IDs/target Geode path; it performs no mutation.

- [ ] **Step 1: Extend asset tests and prove RED**

Require diagnostic scripts to be ASCII/CRLF and forbid mutation verbs/patterns in the diagnostic PowerShell source: `Remove-Item`, `Move-Item`, `Copy-Item`, `Set-Content`, `Out-File`, file `Delete`, directory `CreateDirectory`, or installer rollback functions.

- [ ] **Step 2: Implement read-only diagnostic script**

It may use `Get-ChildItem`, `Get-FileHash`, ZIP read APIs, process inspection, and console output only. It reports whether exactly one matching ID exists and whether installed package hash equals the release manifest when a manifest is supplied beside the script.

- [ ] **Step 3: Implement PowerShell filesystem matrix tests without Pester**

`tests/powershell/test_installer.ps1` creates a temp fake Geometry Dash directory containing `GeometryDash.exe` and `geode\mods`, copies known test `.geode` fixtures, invokes the installer with `-GeometryDashPath`, and throws on any mismatch.

Matrix cases:

```text
no prior ECHO_DASH -> one canonical package installed
one older ECHO_DASH -> backed up and replaced
same version -> deterministic verified reinstall
multiple matching-ID packages -> all backed up, exactly one canonical installed
unrelated mod filename containing ECHO -> untouched
bad payload hash -> zero mutation
wrong manifest ID -> zero mutation
unwritable/preflight failure -> zero mutation
injected post-copy verification failure -> prior package restored byte-for-byte
spaces in target path -> success
```

Use a test-only environment variable `ECHO_DASH_INSTALLER_FAIL_STAGE=post-copy` recognized only to inject the rollback failure point. When unset, production behavior is unchanged.

- [ ] **Step 4: Run PowerShell parser on every shipped/test script**

Use:

```powershell
$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) | Out-Null
if ($errors.Count -ne 0) { throw ($errors | Out-String) }
```

Expected: zero parse errors.

- [ ] **Step 5: Prove GREEN and commit**

```bash
git add release tests/powershell tests/test_installer_assets.py
git commit -m "test: verify ECHO_DASH installer rollback matrix"
```

---

### Task 5: Build the predictable upgrade ZIP without modifying the `.geode`

**Files:**
- Create: `tools/release/package_upgrade.py`
- Modify: `tests/test_release_pipeline.py`
- Modify: `release/README.txt`

**Interfaces:**
- CLI:

```text
python tools/release/package_upgrade.py --package <canonical.geode> --manifest <RELEASE_MANIFEST.json> --release-dir release --output <zip>
```

Outer ZIP root exactly:

```text
ECHO_DASH-v1.1.3-UPGRADE/
  upgrade ECHO_DASH.cmd
  upgrade ECHO_DASH.ps1
  diagnose ECHO_DASH.cmd
  diagnose ECHO_DASH.ps1
  payload/
    doonchy.dash-echo.geode
    RELEASE_MANIFEST.json
  README.txt
  SHA256SUMS.txt
```

- [ ] **Step 1: Write failing package-structure/hash tests**

Build a temporary outer ZIP and assert exact member set, no `..`/absolute paths, payload `.geode` bytes exactly equal input bytes, manifest package hash matches payload, and `SHA256SUMS.txt` matches every packaged file except itself.

- [ ] **Step 2: Prove RED and implement the package builder**

Use Python stdlib only. Read the canonical `.geode` once, compute its SHA, and write those unchanged bytes into the outer ZIP. Never open and rewrite the inner `.geode` archive.

- [ ] **Step 3: Normalize release text/script bytes before packaging**

Fail packaging if shipped `.ps1/.cmd` contains any byte above `0x7f` or any bare LF. Do not silently rewrite a failing script during packaging; source assets must already satisfy the contract.

- [ ] **Step 4: Prove GREEN and commit**

```bash
git add tools/release/package_upgrade.py tests/test_release_pipeline.py release/README.txt
git commit -m "build: package immutable ECHO_DASH upgrades"
```

---

### Task 6: Upgrade the Windows CI pipeline into an immutable candidate factory

**Files:**
- Modify: `.github/workflows/build-v1.yml`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:**
- Produces artifacts:

```text
ECHO-DASH-v1.1.3-compiler-evidence
ECHO-DASH-v1.1.3-windows
ECHO-DASH-v1.1.3-release-manifest
ECHO-DASH-v1.1.3-UPGRADE-v6
```

- [ ] **Step 1: Extend workflow-contract tests first and prove RED**

Require native CTest, Python tests, package inspection, manifest generation, PowerShell parse test, installer matrix, upgrade ZIP build, and immutable payload hash comparison stages.

- [ ] **Step 2: Keep current pinned toolchain installation unchanged**

Geode CLI remains exactly 3.7.4 with the already-pinned CLI archive SHA256. SDK remains exact tag/version 5.10.1.

- [ ] **Step 3: After Geode build/collection, inspect the canonical `.geode`**

Run:

```powershell
python tools/release/inspect_geode.py $package --json dist\package-inspection.json
python tools/release/generate_manifest.py $package --source-commit $env:GITHUB_SHA --installer-version v6 --output dist\RELEASE_MANIFEST.json
```

Fail on any metadata/architecture/hash mismatch.

- [ ] **Step 4: Parse/test installer scripts on Windows PowerShell and run matrix**

Run Python ASCII/CRLF tests, PowerShell parser loop, and `tests\powershell\test_installer.ps1`.

- [ ] **Step 5: Build outer upgrade ZIP from the exact collected package**

Run `package_upgrade.py`; reopen ZIP and compare inner payload SHA256 against the original collected `.geode`. Fail if different.

- [ ] **Step 6: Upload each artifact separately with 30-day retention**

Compiler evidence includes complete build transcript. Candidate artifact contains only canonical `.geode`. Manifest artifact contains manifest + inspection report. Upgrade artifact contains the outer ZIP.

- [ ] **Step 7: Prove workflow-contract tests GREEN locally and commit**

```bash
git add .github/workflows/build-v1.yml tests/test_v1_1_contract.py
git commit -m "ci: build immutable ECHO_DASH v1.1.3 candidates"
```

---

### Task 7: Source, contract, build, and package certification gate

**Files:**
- Create: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`
- Modify only if verification finds defects.

**Interfaces:**
- Produces: exact candidate identity ledger.

- [ ] **Step 1: Run all local source tests**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected: zero failures.

- [ ] **Step 2: Trigger the full Windows workflow and wait for terminal success**

Do not proceed on queued/running state.

- [ ] **Step 3: Download the workflow artifacts and independently inspect them**

Record in the evidence ledger:

```text
source commit SHA
workflow run ID/job ID
canonical .geode artifact ID/digest
canonical .geode SHA256
DLL SHA256
logo SHA256
manifest SHA256
upgrade ZIP artifact ID/digest
upgrade ZIP SHA256
inner payload SHA256
compiler evidence artifact ID/digest
```

- [ ] **Step 4: Verify the hash chain independently**

Require:

```text
canonical CI .geode SHA256 == manifest package_sha256
canonical CI .geode SHA256 == upgrade ZIP payload .geode SHA256
source logo SHA256 == packaged logo SHA256
```

Any mismatch is PACKAGE FAIL.

- [ ] **Step 5: Freeze this artifact set as Runtime Candidate 1**

Record “Candidate 1 immutable after this point.” Any byte/source change after this step creates Candidate 2 and reruns Tasks 7 onward.

- [ ] **Step 6: Commit the evidence ledger only with facts already obtained**

Do not mark INSTALL/RUNTIME/STRESS as PASS yet.

---

### Task 8: Installer certification on the exact immutable candidate

**Files:**
- Modify: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`

**Interfaces:**
- Produces: INSTALL gate evidence only.

- [ ] **Step 1: Run v6 upgrade over a working v1.1.2 installation with Geometry Dash closed**

Record installer transcript. Verify Geode shows exactly one installed package with ID `doonchy.dash-echo` and version v1.1.3 before launching gameplay.

- [ ] **Step 2: Hash the installed `.geode`**

Expected: installed SHA256 equals Candidate 1 canonical package SHA256 exactly.

- [ ] **Step 3: Run read-only diagnostic script**

Expected: one matching ID, v1.1.3, package hash matches manifest, correct installation path; diagnostic run does not change any file timestamp/hash in the mods directory except OS access metadata that is not content.

- [ ] **Step 4: Exercise real-machine containment cases where safe**

Run installer while Geometry Dash is open -> preflight refusal/zero mutation. Corrupt a copy of the outer upgrade ZIP/payload in a disposable test folder -> hash refusal/zero live-install mutation. Do not intentionally corrupt user replay data.

- [ ] **Step 5: Mark INSTALL PASS only after exact installed bytes and rollback/preflight behavior are verified**

If installer scripts or package bytes change, freeze Candidate 2 and repeat source/package/install gates.

---

### Task 9: Runtime, upgrade-data, performance, stress, and soak certification

**Files:**
- Modify: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`
- Use: `docs/runtime/ECHO_DASH_RUNTIME_CERTIFICATION_CHECKLIST_v1.1.3.md`

**Interfaces:**
- Produces: RUNTIME and STRESS evidence for the exact candidate.

- [ ] **Step 1: Confirm package identity in Geode UI**

Verify visible name `ECHO_DASH`, version `v1.1.3`, approved icon, and no duplicate matching mod.

- [ ] **Step 2: Execute the complete Plan-04 runtime lifecycle checklist**

All classic/dual/platformer/practice/Studio/continuation/reset/complete/exit cases must PASS. Capture video/screenshots/logs for any behavior that cannot be asserted automatically.

- [ ] **Step 3: Verify v1.1.2 -> v1.1.3 replay-data compatibility**

Before upgrading, preserve a copy/hash of existing v1.1.2 archive data. After upgrade, load existing histories/replays, open historical Replay Studio entries, create new journaled attempts, restart GD, and confirm both old/new authority remain available. Do not overwrite the preserved evidence copy.

- [ ] **Step 4: Run configured ghost-count performance matrix**

Test `1, 8, 16, 64, 128, 256` with Full/Balanced/Performance/Auto as relevant. Record ECHO presentation current/average/p95/worst metrics, allocation/slot counts, visual correctness, and whether Auto changes effective quality only after its hysteresis conditions.

- [ ] **Step 5: Run synthetic persistence/lifecycle soak**

Using native tests/harness, execute at least 10,000 tiny attempt commit/reset cycles with periodic compaction/reload and assert bounded retained state, monotonic revisions/attempt IDs, zero duplicate finalizations, and zero unrecovered corruption.

- [ ] **Step 6: Run real-game soak**

Perform at least 50 mixed real attempts/resets/deaths with periodic Replay Studio use and one game restart. Monitor memory trend, ghost node count, archive pending durability, file handles where practical, and lifecycle logs. No monotonic leak/stale-node/stuck-state pattern may remain unresolved.

- [ ] **Step 7: Power-loss/corruption recovery simulation on copied test data**

On a disposable copy of archive/journal files, truncate the journal tail and corrupt a snapshot copy. Launch/load with the test context or native recovery harness and confirm valid prefix/backup recovery and preserved corrupt evidence. Never run destructive corruption tests on the sole user archive copy.

- [ ] **Step 8: Mark RUNTIME PASS only when all functional runtime checklist items are terminal PASS**

Compiler/package/install evidence is not substituted for runtime evidence.

- [ ] **Step 9: Mark STRESS PASS only when performance/soak/recovery evidence has no unresolved critical/high reliability defect**

If any fix changes candidate bytes, create a new candidate and repeat all affected gates.

---

### Task 10: Final promotion gate

**Files:**
- Modify: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`
- Modify: `changelog.md` only if evidence status text is part of the release notes and does not claim unproven results.

**Interfaces:**
- Produces: final internal promotion decision; public publication is a separate explicit action if requested.

- [ ] **Step 1: Confirm the evidence ledger names one exact source SHA/package SHA/upgrade ZIP SHA**

No mixed candidate identifiers are allowed.

- [ ] **Step 2: Confirm terminal gate states**

Require explicit terminal status for:

```text
SOURCE PASS
CONTRACT PASS
BUILD PASS
PACKAGE PASS
INSTALL PASS
RUNTIME PASS
STRESS PASS
```

- [ ] **Step 3: Re-run read-only package/hash verification one final time**

Fresh hashes must match the ledger.

- [ ] **Step 4: Mark v1.1.3 internally promoted only after Step 3 evidence matches**

Keep the prior v1.1.2 installer/package as rollback baseline until v1.1.3 promotion is recorded.

- [ ] **Step 5: Do not create a GitHub Release/public distribution automatically unless the user explicitly asks for publication.**
