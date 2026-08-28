# SightWeave M4P3 execution plan

Status: **ACTIVE**

Baseline: `902b192c2acc52d8817ccca0ee13cdb377eb600e`

Branch: `codex/m4p3-sightweave-persistence-restore-closure`

## 1. Goal

Implement the frozen `SIGHTWEAVE_M4P3_PERSISTENCE_RESTORE_CONTRACT.md`: one deterministic V1 snapshot blob containing selected durable SightWeave CPU authority, with bounded raw/Zlib storage, BLAKE3 integrity, persistent modifier IDs, existing-provider persistence, local missing-provider fail-black, and validate-before-commit atomic restore. Do not integrate DARKWELL save slots or product content.

## 2. Source strategy

1. Add `SightWeavePersistence.h/.cpp` to `SightWeaveRuntime`; Runtime continues to depend only on Core/CoreUObject/Engine/DeveloperSettings.
2. Add controlled plain-data export/prepare/commit seams to `FSightWeaveMemoryAuthority` and `FSightWeaveSubjectMemoryAuthority`. No serialization code reads private container layout directly.
3. Add explicit transient-by-default persistence metadata to `FSightWeaveMemoryModifierDescription` and retain existing runtime handles as non-durable values.
4. Extend `ISightWeaveSubjectSnapshotProvider` with optional persistence capture and two-phase restore methods; use one exact Provider ID registry.
5. Add WorldSubsystem orchestration that republishes full Memory and static-environment packets only after a successful total commit. The Render module remains unchanged unless testing exposes a concrete derived-rebuild lifecycle defect.
6. Add focused tests in new M4P3 test translation units. Test code consumes public Runtime APIs; no product backdoor or test dependency enters Runtime/Shipping.

## 3. Checkpoints and gates

### Checkpoint A — contract

- add the persistence contract and this plan;
- run `git diff --check`, status, and `Darkwell.uproject` diff;
- commit and push `docs: define SightWeave M4P3 persistence contract`.

### Checkpoint B — deterministic V1 format

- add result/limit/blob/envelope types;
- implement checked little-endian writer/reader, canonical names/paths, stable sorting, BLAKE3, raw/Zlib selection, bounded decode, and explicit version dispatch;
- add isolated envelope, determinism, raw/compressed, corrupt, future, and oversize tests;
- build `DarkwellEditor Win64 Development`, run isolated tests, diff-check, commit and push.

Expected commit: `feat: add deterministic SightWeave snapshot format`.

### Checkpoint C — persistent authority and atomic providers

- add transient-by-default modifier policy/stable ID and export/replacement states;
- add SubjectMemory export/replacement covering all five policies and valid Last-Seen records;
- extend/register providers; implement provider capture, prepare, commit, duplicate detection, and missing-provider domain fallback;
- implement multi-binding prepare/validate/commit and WorldSubsystem derived publication;
- add modifier/provider/atomic/missing-provider focused tests;
- serial Editor build and focused tests, diff-check, commit and push.

Expected commit: `feat: add atomic SightWeave restore providers`.

### Checkpoint D — failure, isolation, and lifecycle coverage

- add the complete malformed-input rollback matrix;
- assert exact pre/post Memory/Subject revisions, target generations, published packet revisions, provider commit counts, and derived resource counts;
- add two-world/multi-scope/teardown/rebuild and 100-success/100-failure loops;
- add independent-process canonical fixture/hash comparison and timing/size/resource reporting;
- serial Editor build and focused suites, diff-check, commit and push.

Expected commits, adjusted to real nonempty work:

- `test: cover SightWeave persistence and restore failures`
- `perf: validate SightWeave snapshot resource stability`

### Checkpoint E — full closure

- run focused M3.5 and M4P1 regressions under NullRHI and D3D12/SM6;
- run complete M4P3, full SightWeave, and DARKWELL 24 gates;
- inspect any automated restore screenshots directly;
- run severe log scans;
- run BuildPlugin and fresh clean-host Editor Development, Game Development, and Game Shipping serially;
- scan dependencies/imports/strings/object names for Editor/Tests/DARKWELL leakage;
- because `.uplugin`, loading phase, Runtime dependencies, Shipping conditions, shaders, and packaged resource declarations are not planned to change, full Cook/Stage/Package is not repeated unless the actual diff crosses one of those triggers;
- write execution report, final validation, and handoff; commit/push; perform exact Git/LFS closure.

Expected final commit: `docs: complete SightWeave M4P3 validation`.

## 4. Verification order

All Unreal builds and automation processes are serialized. Before each build/test launch, inspect for residual UnrealBuildTool, AutomationTool, UnrealEditor, ShaderCompileWorker, and dotnet build processes. After any abnormal exit, inspect the exit code, relevant log, and residual processes before retrying.

The intended order is:

1. Editor Development;
2. serialization/envelope isolated tests;
3. persistent modifier tests;
4. provider/missing-provider tests;
5. corruption/oversize/atomic rollback tests;
6. world/scope/lifecycle loops;
7. independent-process determinism;
8. M3.5 NullRHI;
9. M4P1 NullRHI;
10. complete M4P3 NullRHI;
11. M3.5 D3D12/SM6;
12. M4P1 D3D12/SM6;
13. M4P3 D3D12 derived rebuild;
14. full SightWeave NullRHI;
15. full SightWeave D3D12/SM6;
16. DARKWELL 24 NullRHI;
17. severe log scan;
18. BuildPlugin;
19. clean-host Editor Development;
20. clean-host Game Development;
21. clean-host Game Shipping;
22. Shipping/dependency/import/string/test-leakage scans;
23. final Git/LFS/object integrity.

## 5. Evidence to retain

Reports record exact discovered/performed/succeeded/warning/failed counts; canonical/stored bytes, compression ratio, hash; p50/p95/p99/max capture/prepare/validate/commit/publication times for small/typical/maximum fixtures; allocation high-water; 100-loop before/after authority/provider/UObject/Render/GPU counts; NullRHI and D3D12 adapter/RHI/SM6 identity; screenshot paths and visual inspection; BuildPlugin/clean-host actions; severe-log classifications; and every preserved failed attempt/root cause.

Generated reports, logs, screenshots, Binaries, Intermediate, Saved, DDC, packages, and clean hosts remain untracked.

## 6. Stop conditions

Stop and report `BLOCKED` if a frozen M3.5/M4P1 contract cannot support validation-free replacement without a second authority, if safe missing-provider localization cannot be represented by explicit domains, if the 64 MiB limits are insufficient for the current maximum fixture, or if implementation requires a product map/asset/DARKWELL/descriptor/shader/module-loading change outside this plan.

Report `PARTIAL` rather than `COMPLETED` if any required correctness, rollback, deterministic, D3D12, full regression, BuildPlugin, clean-host, resource, or Git/LFS gate remains unverified or failing.

## 7. Repository discipline

Do not merge, rebase, force-push, reset, clean, stash, restore user changes, delete unknown files, or modify binary assets with filesystem commands. Never stage `Darkwell.uproject`. Each reliable nonempty checkpoint is built/tested in proportion to its contents, committed, and normally pushed immediately.
