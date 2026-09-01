# ECHO_DASH Hardening 05 — Release and Certification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Freeze the already-versioned hardening source as one immutable, hash-verifiable, rollback-safe ECHO_DASH v1.1.3 candidate and certify the exact same `.geode` bytes through build, package, install, runtime, stress, and soak gates.

**Architecture:** Verify all release surfaces already identify as v1.1.3, replace development artifact labels with candidate labels, generate a machine-readable manifest from the exact CI-built `.geode`, and package that unchanged file into a deterministic outer upgrade ZIP. Use ASCII/CRLF Windows PowerShell 5.1 scripts with shared read-only discovery helpers, safe elevation, ID-based backup/rollback, and a read-only diagnostic path. Any release-affecting byte/source change after candidate freeze creates a new candidate; evidence-only documentation commits do not.

**Tech Stack:** C++23/Geode build, Python 3 stdlib `zipfile`/`hashlib`/`struct`, GitHub Actions `windows-latest`, Windows PowerShell 5.1 parser/runtime compatibility, SHA-256, existing Geode `.geode` ZIP packaging.

**Spec:** `docs/superpowers/specs/2026-09-01-echo-dash-quality-hardening-design.md`

## Global Constraints

- Run only after Plans 01-04 are terminal GREEN with no unresolved runtime FAIL.
- Source/package version is already exactly `v1.1.3`; mod ID remains exactly `doonchy.dash-echo`.
- Geode target remains 5.10.1; Geometry Dash Windows target remains 2.2081; DLL architecture remains x86-64.
- The CI-built `.geode` is canonical. Installer/package tools copy it but never modify/repack it.
- Required identity chain: canonical CI `.geode` SHA256 == manifest package SHA256 == upgrade-ZIP inner package SHA256 == installed package SHA256.
- Upgrade cleanup touches only `.geode` packages whose embedded `mod.json.id` is exactly `doonchy.dash-echo`; filenames are never deletion authority.
- Replay/history/user data is never installer cleanup scope.
- Shipped `.ps1`/`.cmd` files are strict ASCII and CRLF-normalized for Windows PowerShell 5.1.
- Installer progress is stage-based; no fake percentages.
- Diagnostic script and shared discovery helper are read-only and never auto-repair.
- A failed upgrade restores every prior verified matching-ID package byte-for-byte when one existed.
- Backup storage lives outside `geode/mods` so Geode cannot scan backup `.geode` files as installed mods.
- Target discovery order is exact: explicit path -> current Geode profile -> Steam library metadata -> standard Steam path.
- Elevation, when required, occurs before installed mod mutation; elevated child reruns full preflight from the beginning.
- No release/runtime/stress PASS is inferred from weaker gates.
- Evidence-ledger-only documentation commits after candidate freeze do not change candidate identity. Any C++/metadata/asset/workflow/installer/diagnostic/package-tool change does.

---

## File Structure

**Create:**
- `tools/release/inspect_geode.py`
- `tools/release/generate_manifest.py`
- `tools/release/package_upgrade.py`
- `release/ECHO_DASH.Common.ps1` — read-only discovery/identity helper library only.
- `release/upgrade ECHO_DASH.ps1`
- `release/upgrade ECHO_DASH.cmd`
- `release/diagnose ECHO_DASH.ps1`
- `release/diagnose ECHO_DASH.cmd`
- `release/README.txt`
- `tests/test_release_pipeline.py`
- `tests/test_installer_assets.py`
- `tests/powershell/test_installer.ps1`
- `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`

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

### Task 1: Freeze v1.1.3 release identity and code hygiene

**Files:**
- Modify: `.github/workflows/build-v1.yml`
- Modify if audit requires: `mod.json`, `CMakeLists.txt`, `src/main.cpp`, `README.md`, `about.md`, `changelog.md`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:** all version/target surfaces are already v1.1.3 from Plan01; this task converts CI naming from development artifacts to candidate/release artifact names and applies warning/dead-surface hygiene.

- [ ] **Step 1: Write freeze contracts and prove RED only for development-label removal**

Require:

```python
self.assertEqual(metadata["version"], "v1.1.3")
self.assertIn("VERSION 1.1.3", self.read("CMakeLists.txt"))
self.assertRegex(main, r'kReleaseVersion\s*=\s*"v1\.1\.3"')
self.assertNotIn("hardening-dev", workflow)
```

Also require workflow candidate artifact names:

```text
ECHO-DASH-v1.1.3-compiler-evidence
ECHO-DASH-v1.1.3-windows
ECHO-DASH-v1.1.3-release-manifest
ECHO-DASH-v1.1.3-UPGRADE-v6
```

Expected initial RED comes from `hardening-dev` naming, not a version bump.

- [ ] **Step 2: Verify release identity, do not re-version source**

If any production surface is not already v1.1.3, treat it as a Plan01/04 regression and fix with a test. Do not invent v1.1.4 or revert to v1.1.2.

- [ ] **Step 3: Convert workflow artifact/display names from hardening-dev to candidate names**

Keep toolchain pins unchanged. Candidate naming must not include a different package than the exact collected `.geode`.

- [ ] **Step 4: Add ECHO_DASH-owned MSVC warning policy**

```cmake
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive-)
endif()
```

Do not globally promote third-party SDK warnings to errors. Review/fix ECHO_DASH-owned warnings; no broad suppression to hide them.

- [ ] **Step 5: Run dead-surface/current-invariant comment audit**

Remove unused settings fields/helpers/flags or wire them to approved existing behavior. Replace historical implementation-archeology comments with current invariant comments. Do not change user-facing feature scope.

- [ ] **Step 6: GREEN + commit**

Run Python/native tests + Windows Release build.

```bash
git add mod.json CMakeLists.txt src/main.cpp .github/workflows/build-v1.yml README.md about.md changelog.md tests/test_v1_1_contract.py
git commit -m "chore: freeze ECHO_DASH v1.1.3 release surfaces"
```

---

### Task 2: Build package inspection and canonical release-manifest tooling

**Files:**
- Create: `tools/release/inspect_geode.py`
- Create: `tools/release/generate_manifest.py`
- Create: `tests/test_release_pipeline.py`
- Modify: `tests/test_release_assets.py`

**Interfaces:**

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

- [ ] **Step 1: Write failing inspection tests**

Temporary `.geode` ZIP with mod.json/logo/fake PE64. Test metadata/hashes/PE machine `0x8664`; reject x86 `0x014c`, missing logo, duplicate DLL candidates, wrong ID, malformed JSON.

- [ ] **Step 2: Prove RED and implement inspect tool with stdlib only**

Use `zipfile/json/hashlib/struct/pathlib`. PE: `e_lfanew` at 0x3c, `PE\0\0`, then 16-bit machine.

- [ ] **Step 3: Implement deterministic manifest transform**

Reject non-40-hex SHA, unexpected metadata, missing hashes. JSON sorted keys, UTF-8 final newline.

- [ ] **Step 4: Preserve source-icon authority**

Package inspection proves packaged logo hash equals repo `logo.png` hash; retain current approved exact source asset unless separately changed.

- [ ] **Step 5: GREEN + commit**

```bash
git add tools/release tests/test_release_pipeline.py tests/test_release_assets.py
git commit -m "build: add ECHO_DASH package manifest tooling"
```

---

### Task 3: Implement shared read-only Windows target discovery

**Files:**
- Create: `release/ECHO_DASH.Common.ps1`
- Create: `tests/test_installer_assets.py`
- Create/modify: `tests/powershell/test_installer.ps1`

**Interfaces:** read-only helper functions:

```text
Test-GeometryDashRoot
Get-GeodeCurrentProfileRoot
Get-SteamRoot
Get-SteamLibraryRoots
Resolve-GeometryDashRoot
Get-GeodeModIdentity
Find-EchoDashPackages
Assert-PayloadManifest
```

`Resolve-GeometryDashRoot` internal parameters:

```powershell
param(
    [string]$ExplicitPath = "",
    [string]$LocalAppDataRoot = $env:LOCALAPPDATA,
    [string]$SteamRootOverride = ""
)
```

Resolution order is binding:

1. **Explicit** `-GeometryDashPath`: if supplied, validate only that path; no fallback on invalid explicit input.
2. **Current Geode profile**: read `$LocalAppDataRoot\Geode\config.json`, parse `current-profile`, find the matching object in `profiles`, read `gd-path`; if it points to `GeometryDash.exe`, use its parent; validate `GeometryDash.exe` + `geode\mods`.
3. **Steam metadata**: determine Steam root from explicit test override, otherwise `HKCU:\Software\Valve\Steam` `SteamPath` when available, then standard Steam root. Parse `steamapps\libraryfolders.vdf` quoted `path` entries, include the Steam root library, construct `<library>\steamapps\common\Geometry Dash`, validate. One valid match -> use; multiple -> ambiguity error requiring explicit path.
4. **Standard path**: `C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash`.

No registry/config/VDF write occurs.

- [ ] **Step 1: Write ASCII/CRLF/read-only asset tests and prove RED**

Common script must be strict ASCII/CRLF and must not contain mutating cmdlets such as `Remove-Item`, `Move-Item`, `Copy-Item`, `Set-Content`, `Out-File`, `New-Item`, `Start-Process`.

- [ ] **Step 2: Implement exact discovery/identity functions**

ZIP identity reads embedded `mod.json`; ID matching is exact `doonchy.dash-echo`.

- [ ] **Step 3: Unit-test discovery with disposable fake roots**

PowerShell test dot-sources Common and covers explicit path, invalid explicit path no fallback, current Geode profile JSON, profile `gd-path` as exe and directory, Steam VDF single match, Steam ambiguous multiple match, and standard fallback. Test paths include spaces.

- [ ] **Step 4: GREEN + commit**

```bash
git add release/ECHO_DASH.Common.ps1 tests/test_installer_assets.py tests/powershell/test_installer.ps1
git commit -m "feat: add deterministic ECHO_DASH install discovery"
```

---

### Task 4: Implement ASCII-safe transactional v6 installer with safe elevation

**Files:**
- Create: `release/upgrade ECHO_DASH.ps1`
- Create: `release/upgrade ECHO_DASH.cmd`
- Create: `release/README.txt`
- Modify: `tests/test_installer_assets.py`
- Modify: `tests/powershell/test_installer.ps1`

**Interfaces:**

```powershell
param([string]$GeometryDashPath = "")
```

Installer dot-sources `$PSScriptRoot\ECHO_DASH.Common.ps1` and expects:

```text
$PSScriptRoot\payload\doonchy.dash-echo.geode
$PSScriptRoot\payload\RELEASE_MANIFEST.json
```

Installer-only functions:

```text
Write-Stage
Test-IsAdministrator
Test-TargetWritable
Invoke-ElevatedSelf
Backup-ExistingEchoDash
Install-CanonicalPackage
Restore-Backup
Verify-InstalledState
```

- [ ] **Step 1: Write installer asset/function tests and prove RED**

Upgrade `.ps1/.cmd` strict ASCII/CRLF, required functions/stages present, payload paths explicitly under `payload`, common script dot-sourced.

- [ ] **Step 2: Implement read-only preflight before installed-mod mutation**

Order:

```text
locate common helper + payload + manifest
validate manifest schema/product/version/ID/targets
SHA256 payload == manifest
open payload ZIP and verify embedded mod.json ID/version
resolve target using exact common discovery order
validate GeometryDash.exe + geode/mods
verify Geometry Dash process not running
scan current *.geode by embedded ID
probe destination writability
```

A disposable write-probe file may be created/deleted only to determine ACL writability; no installed `.geode` file is altered during preflight.

- [ ] **Step 3: Elevate safely before mutation when required**

If target write probe fails due access denied and process is not administrator, relaunch exact sibling installer with Windows PowerShell using `Start-Process -Verb RunAs`, preserving the resolved `-GeometryDashPath` argument. Parent exits without mod mutation. Elevated child reruns the entire preflight from step 2; it never trusts parent results. If elevation denied/fails, existing installation remains unchanged.

- [ ] **Step 4: Backup existing matching-ID packages outside mod scan root**

Backup directory:

```text
<GeometryDashRoot>\geode\echo-dash-installer-backups\<UTC timestamp>-<guid>\
```

For each matching package record original filename + SHA256, copy, and verify backup hash before any deletion/replacement. Unrelated IDs untouched. Replay/save data untouched.

- [ ] **Step 5: Transactional install + automatic rollback**

Copy canonical payload to temp file in `geode\mods`, verify hash, remove only backed-up matching-ID packages, move temp to `doonchy.dash-echo.geode`, verify installed hash/embedded ID/version, scan and require exactly one matching installed ID. On any post-mutation failure: remove new package, restore all prior packages to original filenames byte-for-byte, verify restored hashes, then report rollback result.

- [ ] **Step 6: Add test-only fault injection safely gated to disposable roots**

`ECHO_DASH_INSTALLER_FAIL_STAGE=post-copy` is honored **only** when target root contains marker file `.echo-dash-installer-test-root`. Without marker, environment variable is ignored. Asset tests require this marker guard in source.

- [ ] **Step 7: Keep animated UX professional and parser-safe**

```text
[1/6] Checking package...
[2/6] Finding Geometry Dash...
[3/6] Backing up existing ECHO_DASH...
[4/6] Installing...
[5/6] Verifying...
[6/6] Complete.
```

Optional spinner only `| / - \`; no Unicode glyphs/fake percentage.

- [ ] **Step 8: Implement CMD wrapper**

Locate sibling PS1, invoke Windows PowerShell `-NoProfile -ExecutionPolicy Bypass -File`, forward `%*`; concise success/failure and preserve console on error.

- [ ] **Step 9: GREEN + commit**

```bash
git add release tests/test_installer_assets.py tests/powershell/test_installer.ps1
git commit -m "feat: add transactional ECHO_DASH v6 installer"
```

---

### Task 5: Implement read-only diagnostic script and complete installer/rollback matrix

**Files:**
- Create: `release/diagnose ECHO_DASH.ps1`
- Create: `release/diagnose ECHO_DASH.cmd`
- Modify: `tests/test_installer_assets.py`
- Modify: `tests/powershell/test_installer.ps1`

**Interfaces:** diagnostic accepts `[string]$GeometryDashPath = ""`, dot-sources read-only Common, and reads manifest from `$PSScriptRoot\payload\RELEASE_MANIFEST.json` when present.

- [ ] **Step 1: Extend read-only/format tests and prove RED**

Diagnostic scripts strict ASCII/CRLF. Diagnostic and Common forbid mutation cmdlets/patterns: `Remove-Item`, `Move-Item`, `Copy-Item`, `Set-Content`, `Out-File`, `New-Item`, `Start-Process`, file delete, directory create, installer rollback functions.

- [ ] **Step 2: Implement read-only diagnostic**

May use Get-ChildItem/Get-FileHash/ZIP read/process inspection/console only. Report resolved target, count of matching IDs, installed version/hash, manifest consistency. No repair.

- [ ] **Step 3: Build disposable final-layout staging tree in matrix**

Each installer matrix case stages:

```text
staging\
  ECHO_DASH.Common.ps1
  upgrade ECHO_DASH.ps1
  diagnose ECHO_DASH.ps1
  payload\
    doonchy.dash-echo.geode
    RELEASE_MANIFEST.json
```

Fake GD root contains `GeometryDash.exe`, `geode\mods`, and `.echo-dash-installer-test-root`. Invoke installer with explicit fake path.

- [ ] **Step 4: Execute matrix**

```text
no prior -> one canonical installed
one older -> backed up/replaced
same version -> verified reinstall
multiple matching IDs -> all backed up, exactly one canonical
unrelated filename containing ECHO -> untouched
bad payload hash -> zero installed-mod mutation
wrong manifest ID -> zero installed-mod mutation
write/preflight failure -> zero installed-mod mutation
post-copy injected failure -> every prior package restored byte-for-byte
spaces in target path -> success
backup directory outside geode/mods -> true
diagnostic run -> no content hash changes
```

- [ ] **Step 5: Parse every shipped/test PowerShell script with Windows PowerShell parser**

```powershell
$tokens = $null
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors) | Out-Null
if ($errors.Count -ne 0) { throw ($errors | Out-String) }
```

Expected zero errors.

- [ ] **Step 6: GREEN + commit**

```bash
git add release tests/powershell tests/test_installer_assets.py
git commit -m "test: verify ECHO_DASH installer rollback matrix"
```

---

### Task 6: Build deterministic upgrade ZIP without modifying `.geode`

**Files:**
- Create: `tools/release/package_upgrade.py`
- Modify: `tests/test_release_pipeline.py`
- Modify: `release/README.txt`

**Interfaces:**

```text
python tools/release/package_upgrade.py --package <canonical.geode> --manifest <RELEASE_MANIFEST.json> --release-dir release --output <zip>
```

Outer ZIP exact member order/set:

```text
ECHO_DASH-v1.1.3-UPGRADE/ECHO_DASH.Common.ps1
ECHO_DASH-v1.1.3-UPGRADE/upgrade ECHO_DASH.cmd
ECHO_DASH-v1.1.3-UPGRADE/upgrade ECHO_DASH.ps1
ECHO_DASH-v1.1.3-UPGRADE/diagnose ECHO_DASH.cmd
ECHO_DASH-v1.1.3-UPGRADE/diagnose ECHO_DASH.ps1
ECHO_DASH-v1.1.3-UPGRADE/payload/doonchy.dash-echo.geode
ECHO_DASH-v1.1.3-UPGRADE/payload/RELEASE_MANIFEST.json
ECHO_DASH-v1.1.3-UPGRADE/README.txt
ECHO_DASH-v1.1.3-UPGRADE/SHA256SUMS.txt
```

- [ ] **Step 1: Write structure/hash/reproducibility tests first**

Assert exact member order/set, no absolute/`..` path, inner `.geode` bytes identical to input, manifest hash matches, SHA256SUMS covers every member except itself.

- [ ] **Step 2: Prove RED and implement deterministic ZIP writer**

Use stdlib only. Inner `.geode` is read once and written unchanged. Use explicit `ZipInfo` for every member with fixed timestamp `1980-01-01 00:00:00`, stable DOS/Unix permission attributes, fixed compression choice (use `ZIP_STORED` for all outer members), and exact member order above.

- [ ] **Step 3: Enforce source script bytes, do not silently normalize**

Fail packaging if shipped PS1/CMD/Common contains non-ASCII byte or bare LF. Source must already be compliant.

- [ ] **Step 4: Prove byte reproducibility**

Build outer ZIP twice from same inputs into different filenames and assert identical SHA256.

- [ ] **Step 5: GREEN + commit**

```bash
git add tools/release/package_upgrade.py tests/test_release_pipeline.py release/README.txt
git commit -m "build: package immutable ECHO_DASH upgrades"
```

---

### Task 7: Upgrade Windows CI into immutable candidate factory

**Files:**
- Modify: `.github/workflows/build-v1.yml`
- Modify: `tests/test_v1_1_contract.py`

**Interfaces:** artifacts:

```text
ECHO-DASH-v1.1.3-compiler-evidence
ECHO-DASH-v1.1.3-windows
ECHO-DASH-v1.1.3-release-manifest
ECHO-DASH-v1.1.3-UPGRADE-v6
```

- [ ] **Step 1: Extend workflow contracts first and prove RED**

Require Python tests, native CTest, package inspection, manifest generation, PS parser, installer matrix, deterministic ZIP build, and immutable inner-payload hash comparison.

- [ ] **Step 2: Keep pinned toolchain unchanged**

Geode CLI 3.7.4 + existing verified CLI ZIP SHA; SDK exact 5.10.1.

- [ ] **Step 3: Inspect exact collected `.geode` and generate manifest**

```powershell
python tools/release/inspect_geode.py $package --json dist\package-inspection.json
python tools/release/generate_manifest.py $package --source-commit $env:GITHUB_SHA --installer-version v6 --output dist\RELEASE_MANIFEST.json
```

Verify runtime `EchoBuildIdentity.sourceSha` was compiled from the same `$GITHUB_SHA` via source/build contract or binary/log evidence available to workflow.

- [ ] **Step 4: Parse/test installer assets and matrix**

Run Python ASCII/CRLF tests, PowerShell parser, disposable matrix.

- [ ] **Step 5: Build deterministic outer ZIP from exact collected package**

Build twice and compare ZIP hashes; reopen chosen ZIP and compare inner `.geode` hash to canonical collected package.

- [ ] **Step 6: Upload separate 30-day artifacts**

Compiler evidence = full transcript. Windows candidate = only canonical `.geode`. Manifest artifact = manifest + inspection. Upgrade artifact = deterministic outer ZIP.

- [ ] **Step 7: GREEN + commit**

```bash
git add .github/workflows/build-v1.yml tests/test_v1_1_contract.py
git commit -m "ci: build immutable ECHO_DASH v1.1.3 candidates"
```

---

### Task 8: SOURCE/CONTRACT/BUILD/PACKAGE certification and Candidate 1 freeze

**Files:**
- Create: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`
- Modify release-affecting files only if verification finds defects; any such fix restarts this task with a new source SHA.

- [ ] **Step 1: Run all local source tests**

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
cmake -S . -B build-core-tests -DECHO_DASH_BUILD_CORE_TESTS=ON
cmake --build build-core-tests --config Release --target EchoDashCoreTests
ctest --test-dir build-core-tests -C Release --output-on-failure
```

Expected zero failures.

- [ ] **Step 2: Trigger full Windows workflow and wait for terminal success**

Queued/running is not evidence.

- [ ] **Step 3: Independently inspect downloaded artifacts**

Record:

```text
release-affecting source commit SHA
workflow run ID/job ID
canonical .geode artifact ID/digest/SHA256
DLL SHA256
logo SHA256
manifest SHA256
upgrade artifact ID/digest/SHA256
upgrade inner payload SHA256
compiler evidence artifact ID/digest
```

- [ ] **Step 4: Verify hash/build identity chain**

```text
canonical .geode SHA == manifest package_sha256 == upgrade inner .geode SHA
source logo SHA == packaged logo SHA
manifest source_commit == release-affecting source SHA == runtime/build source identity
```

Mismatch = PACKAGE FAIL.

- [ ] **Step 5: Freeze exact artifact set as Runtime Candidate 1**

The candidate identity is the release-affecting source SHA + canonical package SHA + deterministic outer ZIP SHA. Any later change to C++/metadata/assets/workflow/release scripts/release tooling makes Candidate 2. Writing/updating only `docs/runtime/...RELEASE_EVIDENCE.md` does **not** change Candidate 1.

- [ ] **Step 6: Commit evidence ledger with only obtained facts**

Do not mark INSTALL/RUNTIME/STRESS PASS.

---

### Task 9: Installer certification on exact immutable candidate

**Files:**
- Modify: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md` only.

- [ ] **Step 1: Run v6 upgrade over working v1.1.2 with GD closed**

Record transcript. Verify exactly one installed `doonchy.dash-echo` v1.1.3 before gameplay.

- [ ] **Step 2: Hash installed `.geode`**

Installed SHA must equal Candidate1 canonical package SHA exactly.

- [ ] **Step 3: Run read-only diagnostic**

Expected one matching ID, v1.1.3, hash matches manifest, correct target. Snapshot content hashes of mods directory before/after diagnostic; no file content changes.

- [ ] **Step 4: Exercise safe containment**

Installer with GD open -> refusal/no installed-mod mutation. Corrupt a disposable copy of upgrade payload -> hash refusal/no live mutation. Where administrator elevation is needed, verify the elevated child reruns full preflight and installs the same canonical payload. Never corrupt sole replay data.

- [ ] **Step 5: Mark INSTALL PASS only after exact bytes + containment verified**

Release-affecting fix creates Candidate2 and repeats Tasks8+.

---

### Task 10: Runtime, upgrade-data, performance, stress, soak, recovery certification

**Files:**
- Modify: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`
- Use: `docs/runtime/ECHO_DASH_RUNTIME_CERTIFICATION_CHECKLIST_v1.1.3.md`

- [ ] **Step 1: Confirm package/build identity in Geode/diagnostics**

Visible ECHO_DASH v1.1.3, approved icon, no duplicate, diagnostic short source SHA matches Candidate1 source.

- [ ] **Step 2: Execute complete Plan04 runtime lifecycle checklist**

Classic/dual/platformer/practice/Studio/input isolation/continuation/no-ghost/reset/complete/exit all PASS. Capture evidence where automation cannot.

- [ ] **Step 3: Verify v1.1.2 -> v1.1.3 replay-data compatibility**

Preserve/hash copy of existing v1.1.2 archive before upgrade. Load old histories/replays; open historical Studio entries; create new journaled attempts; restart; old/new authority remains. Preserved evidence copy untouched.

- [ ] **Step 4: Run ghost-count/quality performance matrix**

Counts 1,8,16,64,128,256 with Full/Balanced/Performance/Auto as relevant. Record ECHO current/avg/p95/worst presentation metrics, slot/allocation counts, visual correctness, Auto hysteresis.

- [ ] **Step 5: Synthetic persistence/lifecycle soak**

At least 10,000 tiny commit/reset cycles with periodic compaction/reload; bounded state, monotonic revisions/attempt IDs, zero duplicate finalization, no unrecovered corruption.

- [ ] **Step 6: Real-game soak**

At least 50 mixed real attempts/resets/deaths, periodic Studio, one restart. Observe memory trend/node count/pending durability/file handles where practical/lifecycle logs. No unresolved monotonic leak/stale-node/stuck pattern.

- [ ] **Step 7: Disposable power-loss/corruption simulations**

Copied data only: truncate journal tail, corrupt snapshot, verify valid prefix/backup recovery and corrupt evidence preserved.

- [ ] **Step 8: Mark RUNTIME PASS only after every functional runtime item terminal PASS**

- [ ] **Step 9: Mark STRESS PASS only when performance/soak/recovery has no unresolved critical/high reliability defect**

Release-affecting fix -> Candidate2 and rerun affected gates.

---

### Task 11: Final promotion gate

**Files:**
- Modify: `docs/runtime/ECHO_DASH_v1.1.3_RELEASE_EVIDENCE.md`
- Modify `changelog.md` only if wording does not claim unproven evidence; any release-affecting changelog packaging change before final artifact freeze must be included before Candidate freeze, not after.

- [ ] **Step 1: Confirm ledger names one exact candidate identity**

One release-affecting source SHA, package SHA, outer ZIP SHA; evidence-doc commit SHA is recorded separately and is not candidate source identity.

- [ ] **Step 2: Confirm terminal gates**

```text
SOURCE PASS
CONTRACT PASS
BUILD PASS
PACKAGE PASS
INSTALL PASS
RUNTIME PASS
STRESS PASS
```

- [ ] **Step 3: Fresh read-only package/hash verification**

Rehash retained canonical artifact/outer ZIP and compare ledger.

- [ ] **Step 4: Mark v1.1.3 internally promoted only after hashes match**

Keep v1.1.2 rollback baseline.

- [ ] **Step 5: Do not create a GitHub Release/public distribution unless user explicitly asks for publication.**
