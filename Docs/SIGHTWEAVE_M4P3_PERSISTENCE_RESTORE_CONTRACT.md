# SightWeave M4P3 deterministic persistence and atomic restore contract

Status: **FROZEN FOR M4P3 IMPLEMENTATION**

Baseline: `902b192c2acc52d8817ccca0ee13cdb377eb600e`

Branch: `codex/m4p3-sightweave-persistence-restore-closure`

Format written by this milestone: **SightWeave Snapshot V1**

## 1. Ownership and non-goals

SightWeave owns one complete, deterministic, bounded snapshot blob per API call. It validates and restores that blob, atomically replaces the selected CPU authority, publishes new revisions, and rebuilds derived presentation resources. It does not retain snapshot history.

The host owns save slots, paths, disk I/O, autosave policy, cloud/profile/history policy, level loading, `USaveGame`, and DARKWELL progression. M4P3 does not change `Source/Darkwell`, `/Game/Maps/L_Prototype`, any `.uasset`/`.umap`, `Darkwell.uproject`, the legacy v6 fog payload, or the M6/M7 integration boundary.

The snapshot never contains UObject/Actor/World pointers, RHI/RDG/GPU resources, render handles, current-frame visibility, live vision or illumination polygons, active sources, reveal overrides, schedulers, queues, residency, page tables, dirty caches, Lab/test/editor state, timestamps, random values, or process addresses.

## 2. Existing source mapping

M4P3 extends the existing authorities instead of creating another world authority:

| Contract concept | Existing source symbol |
| --- | --- |
| CPU packed HardMemory | `FSightWeaveMemoryAuthority` and `FSightWeavePackedMemoryTile` in `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveMemory.h` |
| Exact runtime scope | `FSightWeaveMemoryScopeKey::IsEquivalentTo` in `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveMemory.cpp:383` |
| Clear result | `FSightWeaveMemoryAuthority::ClearMemory` in `Plugins/SightWeave/Source/SightWeaveRuntime/Private/SightWeaveMemory.cpp:777` |
| Active memory modifiers | `FSightWeaveMemoryModifierDescription` and `FSightWeaveMemoryAuthority::RegisterModifier` in `SightWeaveMemory.h` and `SightWeaveMemory.cpp:845` |
| Immutable CPU memory publication | `FSightWeaveMemoryAuthority::PublishPacket` in `SightWeaveMemory.cpp:960` |
| Static-environment route | `FSightWeaveStaticEnvironmentAuthority` in `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveStaticEnvironment.h` |
| Five subject policies | `ESightWeaveSubjectMemoryPolicy` in `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveSubjectMemory.h` |
| Last-Seen authority | `FSightWeaveSubjectMemoryAuthority` and `FSightWeaveLastSeenSnapshotDescriptor` in `SightWeaveSubjectMemory.h` |
| Existing Custom provider seam | `ISightWeaveSubjectSnapshotProvider` in `SightWeaveSubjectMemory.h` |
| Runtime render publication | `USightWeaveWorldSubsystem::OnMemoryPacketPublished` and `OnStaticEnvironmentPacketPublished` in `Plugins/SightWeave/Source/SightWeaveRuntime/Public/SightWeaveWorldSubsystem.h:338-356` |
| Derived GPU rebuild consumer | `USightWeaveRenderWorldSubsystem::HandleMemoryPacketPublished` and `HandleStaticEnvironmentPacketPublished` in `Plugins/SightWeave/Source/SightWeaveRender/Private/SightWeaveRenderWorldSubsystem.cpp:149-193` |
| GPU fail-black validation | `FSightWeaveSparseAtlasRenderState::ProcessMemoryPending_RenderThread` and `ProcessStaticEnvironmentPending_RenderThread` in `Plugins/SightWeave/Source/SightWeaveRender/Private/SightWeaveSparseAtlasRenderState.cpp` |

The current runtime scope includes `FSightWeaveRenderWorldIdentity::Serial` and `WorldGeneration`; these are lifetime guards, not cross-process identities. V1 serializes a host-supplied stable scope ID plus the stable owner/floor/origin/plane/precision/profile semantics. Restore maps that durable record onto the current target's world identity and generation and rejects a target that changes or tears down during prepare.

## 3. Public API family

M4P3 adds `SightWeavePersistence.h` with neutral runtime-only value types:

- `FSightWeaveSnapshotBlob`: owned `TArray<uint8>` and no file/slot behavior;
- `FSightWeaveSnapshotLimits`: validated limits, defaulting to 64 MiB canonical and 64 MiB total stored blob;
- `ESightWeaveSnapshotResult` and `FSightWeaveSnapshotDiagnostic`: structured success/failure/fallback results;
- `FSightWeavePersistenceScopeBinding`: stable scope ID plus non-owning, call-local bindings to the existing world/memory/subject authorities;
- `FSightWeavePersistenceProviderRegistry`: game-thread registry of the existing subject-provider interface by stable provider ID;
- `FSightWeavePersistence::Capture` and `FSightWeavePersistence::Restore`: synchronous game-thread entry points.

The blob is a plain value suitable for embedding in a host save. Non-owning bindings and providers are never serialized or retained by a snapshot.

## 4. Included authoritative state

For every selected durable scope V1 contains:

1. stable scope ID, Knowledge Owner ID, Floor ID, floor origin/plane, selected memory precision, and the complete sorted canonical profile capabilities;
2. every nonempty packed HardMemory tile, keyed by signed logical X/Y and containing exactly 7,688 authoritative bytes;
3. the effects of prior clear operations as the resulting tile bytes and absence of cleared Last-Seen records; clear history is not duplicated;
4. only active modifiers explicitly marked persistent, including their stable semantic ID, operation, exact normalized region, height/floor/scope, and enabled state;
5. subject registrations for all five policies, keyed by stable subject ID and instance generation, so policy identity remains durable;
6. valid Last-Seen descriptors for `LastSeenSnapshot` and accepted `Custom` subjects, including stable mesh/material paths and descriptor revisions required by the frozen M4P1 contract;
7. sorted, versioned provider payload records and their explicit subject/region/semantic domains.

Unknown is represented by absent memory bits and absent Last-Seen records. Suppression is durable only when its modifier is explicitly persistent. Transient suppression/modifiers are absent from the blob.

`FSightWeaveStaticEnvironmentAuthority` registrations and neutral attribute tiles are deterministic authored/derived eligibility data, not mutable exploration knowledge. V1 does not serialize its runtime handles or derived attribute tiles. After restore the target's already-authored static eligibility is republished while HardMemory is fully rebuilt from the restored CPU tiles. The combined M3.5 remembered presentation must therefore round-trip and is a required D3D12 test, while no GPU/static-mirror content is read from the blob.

## 5. Persistent modifier boundary

`FSightWeaveMemoryModifierDescription` gains an explicit persistence policy and stable semantic ID.

- The default is transient/non-persistent.
- A persistent modifier requires a nonempty stable semantic ID valid across processes.
- Runtime handles and registration order are never durable IDs.
- Duplicate stable IDs within one stable scope fail capture and restore.
- A persistent modifier missing an ID fails capture; it is not skipped.
- Canonical order is stable scope ID, lower-cased semantic ID, operation, then normalized region bytes.
- Only the existing authoritative operation/region/enabled fields are stored; modifier revision, handles, dirty tiles, fade/cache state, and registration indices are excluded.

Transient modifiers remain active in the source authority but are intentionally absent from capture and are replaced by the target's post-restore transient runtime configuration. Tests must prove that changing only transient modifier registration order or handle values does not change the blob.

## 6. Provider extension and two-phase restore

M4P3 extends `ISightWeaveSubjectSnapshotProvider`; it does not add a parallel provider system. Existing M4P1 capture methods remain valid. Persistence methods are optional defaults so existing providers that do not claim persistence continue to compile and fail closed when persistence is required.

Each persistent provider exposes:

- a nonempty stable Provider ID;
- a nonzero payload schema version;
- deterministic canonical payload records;
- an explicit domain: subject identity, region, or named semantic domain inside a stable scope;
- capture with structured failure;
- side-effect-free validate/prepare producing an owned temporary prepared object;
- an infallible game-thread commit after all core/provider validation succeeds.

The registry rejects duplicate Provider IDs. Capture also rejects duplicate IDs, provider errors, invalid versions/domains, duplicate domain records, oversized payloads, or a persistent Custom subject without its exact provider/domain payload. No partial blob is returned.

Restore behavior is frozen:

- registered provider + corrupt/version-invalid/rejected payload: fail the entire restore before commit;
- missing provider: do not execute or trust its payload; remove only that provider's subject records and/or clear only its declared memory region in the prepared replacement; named semantic domains remain unapplied and are reported Unknown;
- other core scopes, other providers, static HardMemory, and valid subject records restore in the same commit;
- the result is `SucceededWithProviderFallback`, includes every missing Provider ID/domain, and is never reported as warning-free success;
- reloading the same complete blob after the provider returns restores that domain normally.

Prepared provider objects are temporary. No provider commit occurs until every envelope, core, scope, and provider check has passed and every target lifetime/revision guard is rechecked.

## 7. V1 envelope

All integer fields are fixed-width little-endian. The 80-byte header is written field-by-field; no C++ struct layout or padding is serialized.

| Offset | Bytes | Field |
| ---: | ---: | --- |
| 0 | 8 | ASCII magic `SWPERSV1` |
| 8 | 2 | format version, exactly `1` |
| 10 | 2 | header bytes, exactly `80` |
| 12 | 4 | flags; V1 permits only bit 0 `Compressed` |
| 16 | 1 | compression method: `0=None`, `1=Zlib` |
| 17 | 7 | reserved, all zero |
| 24 | 8 | canonical uncompressed payload bytes |
| 32 | 8 | stored payload bytes |
| 40 | 4 | stable scope count |
| 44 | 4 | provider payload record count |
| 48 | 32 | BLAKE3 hash of the canonical uncompressed payload |
| 80 | variable | raw or compressed payload |

The total blob size must equal `80 + stored payload bytes` with checked arithmetic. V1 rejects unknown flags/methods, nonzero reserved bytes, header-size mismatch, trailing data, truncation, and inconsistent scope/provider counts.

## 8. Canonical payload and ordering

The payload is a field-by-field binary grammar with explicit counts and lengths. Booleans are one byte `0` or `1`; enums are range-checked bytes; floats/doubles are serialized by their IEEE bit pattern only after finite validation; strings/paths are length-prefixed UTF-8. Stable `FName` IDs are lower-cased before encoding so display-case history cannot change semantic bytes. Soft object paths preserve their canonical path string.

Collections are sorted before encoding:

1. scopes by stable scope ID, owner ID, floor ID, origin/plane, precision, then the full profile sequence;
2. profile capabilities lexically, with duplicate capabilities rejected;
3. memory tiles by logical X then logical Y;
4. persistent modifiers by stable semantic ID then canonical description;
5. subjects by stable subject ID then instance generation;
6. material overrides by slot order because slot order is semantic;
7. providers by Provider ID, schema version, domain type, stable scope ID, and canonical domain bytes.

Duplicate scope IDs, tile keys, modifier IDs, subject identities, provider IDs, or provider domains fail closed. Container bucket order, actor creation order, runtime handles, timestamps, revisions unrelated to durable semantics, thread timing, and addresses never determine ordering.

## 9. Compression, hash, and limits

- canonical payload is the deterministic authority;
- BLAKE3 covers the entire uncompressed canonical payload;
- payloads smaller than 4,096 bytes are stored raw;
- payloads of at least 4,096 bytes are compressed with UE Runtime `FCompression` and `NAME_Zlib`, `COMPRESS_NoFlags`;
- compressed storage is selected only when strictly smaller than the raw payload;
- the envelope records raw versus Zlib explicitly;
- restore decompresses into one bounded temporary array, then verifies exact size and BLAKE3 before parsing.

Defaults are 64 MiB canonical payload and 64 MiB total stored blob, including the header. Declared sizes, counts, length prefixes, additions, multiplications, array growth, and decompression destinations are checked before allocation. The parser rejects overflow, impossible compression ratios, excessive counts, truncated fields, trailing bytes, and size disagreement. No invalid input can request unbounded allocation.

V1 is the only readable/writable format. Versions greater than 1 return `FutureVersion`. Versions below 1 return `UnsupportedLegacyVersion`; there is no fictitious V0 migration. Parsing dispatches through an explicit version switch so a future real migration can be added without weakening V1.

## 10. Prepare, validate, commit

Restore is game-thread-only and follows this order:

1. validate envelope, flags, version, declared sizes, and limits;
2. bounded decompression;
3. hash verification;
4. defensive canonical parsing;
5. duplicate, enum, finite-number, scope, tile, subject, modifier, and reference validation;
6. map every durable scope to exactly one target binding and remap runtime world identity/generation;
7. prepare complete replacement Memory and Subject authority states without mutation;
8. ask every registered provider to validate/prepare without side effects;
9. apply missing-provider local Unknown/black fallback to the prepared replacement;
10. recheck target world identity, generation, configured scope, authoritative revisions, and teardown state captured at prepare start;
11. swap every replacement state on the game thread; these commit operations are validation-free/infallible after prepare;
12. commit prepared providers;
13. advance core restore/memory/modifier/subject revisions exactly once as applicable;
14. publish full-rebuild memory and static-environment packets only after all swaps;
15. notify completion and let the existing Render subsystem rebuild residency/pages/resources from those CPU packets.

No query/callback is dispatched between core swaps on the game thread. Any failure before step 11 leaves Memory tiles, modifiers, subjects, revisions, generations, published packets, provider state, and Render/GPU resource counts unchanged. World teardown or revision drift before commit returns a structured stale-target failure and publishes no rebuild.

## 11. Result codes and diagnostics

The structured result distinguishes at least:

- `Succeeded`;
- `SucceededWithProviderFallback`;
- `EmptyBlob`, `Truncated`, `InvalidMagic`, `UnsupportedLegacyVersion`, `FutureVersion`;
- `InvalidHeader`, `InvalidFlags`, `InvalidCompressionMethod`, `SizeOverflow`, `SizeLimitExceeded`, `SizeMismatch`, `DecompressionFailed`, `ChecksumMismatch`;
- `PayloadMalformed`, `TrailingPayload`, `InvalidScope`, `MissingTargetScope`, `DuplicateScope`, `DuplicateTile`, `InvalidTile`, `DuplicateModifierId`, `InvalidPersistentModifier`, `DuplicateSubjectId`, `InvalidSubject`, `InvalidReference`;
- `DuplicateProviderId`, `InvalidProviderPayload`, `ProviderCaptureFailed`, `ProviderVersionMismatch`, `ProviderPrepareFailed`;
- `TargetChanged`, `WorldTornDown`, `CommitInvariantFailed`.

Diagnostics include format/method/sizes/hash, failing byte offset and stable IDs where safe, counts, missing-provider fallback domains, and prepare/validate/commit/derived-publication timings. Logs supplement but never replace the structured result.

## 12. Threading and lifecycle

Capture and restore run synchronously on the game thread because current Memory, SubjectMemory, WorldSubsystem publication, and provider transitions are game-thread authorities. Blob inspection helpers may operate on plain bytes off-thread only when they have no provider or UObject access and do not mutate authority.

Bindings and providers are call-local/non-owning. Prepare stores only owned plain replacement data and owned prepared-provider objects. Before commit, the coordinator verifies that every bound world/scope still has its captured lifetime identity, generation, scope, and revision. Teardown invalidates the operation. Delayed render commands continue to use existing world/generation/revision rejection and may never revive pre-restore GPU resources.

## 13. Compatibility and failure fallback

An incompatible or corrupt core snapshot never clears current state. The host receives an explicit failure and decides whether to continue with current state, start fresh, or choose another save. SightWeave performs no slot fallback itself.

Missing-provider fallback is the only successful degraded restore in V1. It is local to declared provider domains and cannot clear an unrelated scope or whole world. Unknown provider bytes are never passed to another provider or interpreted as trusted core state.

## 14. Required test matrix

The M4P3 suite covers:

- empty, 1/8/128-tile, single-scope and multi-scope round trips;
- M3.5 HardMemory/static presentation, clear, blocker, presentation suppression, Unknown, and post-restore re-exploration;
- all five M4P1 policies, falling edge, Last-Seen, snapshot, reacquire, clear, suppression, identity/generation isolation;
- persistent versus transient modifiers, missing/duplicate stable IDs, insertion/registration order independence;
- multiple providers, deterministic provider ordering, provider capture/prepare failures, duplicate IDs, missing-provider local fail-black and later recovery;
- raw and Zlib paths plus identical canonical bytes/blob/hash across repeated and independent processes;
- empty/truncated/bad magic/version/flags/overflow/oversize/compression/hash/payload/duplicate/reference corruption, proving state/revisions/resources unchanged;
- two worlds, PIE instances, scopes, teardown/rebuild, restore then clear/reacquire/suppress, 100 successful and 100 failed loops without monotonic UObject/provider/Render/GPU growth;
- NullRHI CPU authority and D3D12/SM6 derived rebuild, M3.4 Width=50, M3.5, M4P1, full SightWeave, DARKWELL 24, BuildPlugin, and clean-host Editor/Game Development/Game Shipping;
- Runtime/Shipping dependency, import, string, and test/editor leakage scans.

## 15. Frozen behavior preserved

M4P3 does not alter M2 CPU visibility authority, black/gray/live ordering, black-to-gray legal sight, Coarse 25 cm memory precision, M3.4 Width=50 inward feather, M3.5 clear/block/suppress behavior, M4P1 five policies/falling-edge/reacquire/proxy semantics, Subject Reveal isolation, source-compatible queries, or the rule that GPU/Render data is presentation-only.

