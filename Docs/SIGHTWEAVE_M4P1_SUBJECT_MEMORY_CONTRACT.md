# SightWeave M4P1 subject memory contract

Status: **FROZEN FOR M4P1 IMPLEMENTATION**

Frozen baseline: M3.5 `941dcb75fe9fcda05bcb972a4d2d9651c6d521be`

## 1. Authority boundary

M4P1 preserves this one-way data flow:

```text
CPU hard-live query and CPU HardMemory
    -> deterministic subject policy transition
    -> immutable revisioned Last-Seen descriptor
    -> exact world/scope/eligibility validation
    -> render-only proxy presentation
```

The GPU, camera, viewport, Scene Color, SceneDepth, GBuffer, rendered lighting, shadows, VFX, post processing, and presentation feather never decide subject policy, capture eligibility, descriptor content, gameplay authority, clear/suppression authority, or persistence. M4P1 does not use SceneCapture or texture capture.

## 2. Policies

`NeverRemember`

- Live primitives may be shown only while the authoritative hard-live result is legal.
- No subject snapshot or Last-Seen proxy is created.
- The semantic distinction from `VisibleOnly` is durable so host code can identify subjects that must never opt into memory later.

`VisibleOnly`

- Live primitives are shown only while hard live is legal.
- They disappear immediately when hard live ends.
- No remembered state or proxy is created.

`StaticEnvironment`

- Delegates exclusively to the completed M3.5 explicit immutable static-environment authority and neutral-gray composite.
- M4P1 creates neither a second static-memory store nor a subject proxy for this policy.

`LastSeenSnapshot`

- Live primitives are shown while hard live is legal and the proxy is hidden.
- Exactly one immutable descriptor may be captured on each legal live-to-non-live falling edge.
- A valid descriptor may drive a static render-only proxy only while the snapshot location remains remembered and presentation is not suppressed or blocked.
- Reacquisition hides the proxy immediately. A new descriptor is permitted only after a later falling edge.

`Custom`

- Uses a named, versioned host provider route.
- Missing provider, version mismatch, provider rejection, invalid output, or unsupported content fails closed: live behavior may remain policy-legal, but no snapshot or proxy is created.
- No DARKWELL provider is added in M4P1.

## 3. Stable identity and exact scope

Every subject registration owns:

- a generation-safe runtime handle;
- a non-empty stable subject identity supplied by the host or fixture;
- a nonzero subject instance generation;
- one policy;
- one exact memory scope;
- a deterministic sample location and bounds;
- explicit live primitives;
- an optional provider identity and provider schema version.

The exact snapshot scope contains:

- render-world lifetime identity;
- world generation;
- knowledge owner;
- floor;
- floor origin and floor plane;
- precision tier;
- the complete sorted canonical profile sequence.

Scope equality uses full field and canonical sequence comparison. Hashes may accelerate lookup but never establish equality. A stable subject ID reused with a different instance generation is a different subject instance and rejects the old snapshot.

## 4. Immutable Last-Seen descriptor

The CPU descriptor contains at least:

- stable subject identity;
- subject instance generation;
- exact world lifetime identity and generation;
- knowledge owner and floor;
- floor origin/plane, precision tier, and complete canonical profiles;
- memory policy;
- nonzero snapshot revision;
- nonzero eligibility revision;
- nonzero source live revision;
- world transform and world bounds;
- basic opaque static-mesh asset identity;
- ordered static material-slot override identities;
- optional conservative visual-variant identity;
- capture reason;
- nonzero capture-transition identity;
- explicit validity flags.

The descriptor is copied by value and never references mutable Actor, component, MID, skeletal pose, particle, audio, light, physics, or Blueprint runtime state. Descriptor validation rejects non-finite transforms/bounds, missing asset identity, zero required revisions, policy mismatch, scope mismatch, generation mismatch, and unsupported validity flags.

The basic built-in path accepts one ordinary opaque `UStaticMeshComponent` with a stable mesh asset, no skeletal mesh, no Niagara/particle component, no light component, no audio component, no runtime mesh generation, and no dynamic material instance. Ordered material overrides must resolve to stable material assets. Any ambiguous or unsupported case fails closed rather than approximating current state.

## 5. Capture transition

An observation input carries the subject handle, exact scope, authoritative query/snapshot revision, hard-live boolean, memory-write eligibility, current transform/bounds, and an optional capture transition identity.

The state machine records the previous authoritative live state. It captures only when all of the following are true:

1. policy is `LastSeenSnapshot` or an accepted `Custom` route;
2. the previous accepted state was hard live;
3. the current accepted state is non-live;
4. the previous live observation was memory-write eligible;
5. world, owner, floor, precision, floor origin/plane, canonical profiles, instance generation, and source revision are valid;
6. the transition identity has not already been consumed;
7. the built-in descriptor or provider output validates completely.

Remaining hard live never captures and never increments the snapshot revision. Duplicate/delayed falling-edge commands with an already consumed transition identity do not capture. A successful later reacquire followed by a new falling edge increments the snapshot revision exactly once.

Capture stores stable subject state from the last accepted live observation; it does not query or copy a changing off-screen Actor after the edge.

## 6. Deterministic presentation priority

For each registered subject, precedence is independent of container iteration order:

1. teardown, invalid handle, invalid world, generation mismatch, scope mismatch, stale input revision, or invalid policy data: hide live primitives and proxy; reject the input;
2. authoritative hard live: show live primitives, hide proxy immediately;
3. accepted clear mutation affecting the exact snapshot scope and bounds: delete the descriptor and destroy or invalidate the proxy;
4. non-live `NeverRemember`, `VisibleOnly`, or `StaticEnvironment`: hide subject live primitives and any subject proxy;
5. non-live `LastSeenSnapshot`/`Custom` with no fully valid descriptor: hide both;
6. non-live with unknown HardMemory at the descriptor sample/bounds: hide both; no old-snapshot fallback;
7. non-live with active `BlockMemoryWrites` or `SuppressMemoryPresentation`: hide proxy but retain the descriptor;
8. non-live remembered with exact eligibility/scope/revision/generation match: hide live primitives and show the render-only proxy.

Live presentation wins over memory-only suppression because neither memory suppression nor write blocking suppresses hard live. `SuppressLiveVision` changes the authoritative hard-live result before this state machine.

## 7. Clear, suppression, unknown, and teardown

- `ClearMemory` deletes matching subject descriptors whose snapshot bounds intersect the cleared region in the exact scope. The proxy is destroyed or invalidated in the same game-thread mutation.
- `BlockMemoryWrites` and `SuppressMemoryPresentation` hide a non-live proxy while preserving its descriptor. Removal may restore it only after all exact validations pass again.
- Unknown HardMemory is strict black: live primitives and proxies remain hidden.
- Floor unload, world teardown, component unregistration, or owner teardown destroys proxies and removes runtime records. No old-world descriptor crosses PIE/world restart.
- Delayed updates are rejected when their observation, eligibility, source-live, transition, or instance revision is stale.

## 8. Render-only proxy contract

The built-in proxy is a transient, non-saved presentation object created from the immutable asset descriptor. It has:

- no collision or overlap generation;
- no physics or navigation relevance;
- no ticking or gameplay component replication;
- no AI, interaction, targeting, damage, or save authority;
- no audio, Niagara, particle, light, or dynamic-state emission;
- no dynamic shadow or current-light-dependent behavior required for memory;
- no write-back path to the gameplay Actor or source component.

Invalid mesh/material resources destroy or hide the proxy and fail black. The implementation never substitutes the current live Actor, another scope's proxy, or a stale descriptor.

## 9. Custom provider boundary

The provider interface is host-implemented and registered by an exact provider name plus nonzero schema version. Capture receives immutable plain input and returns a bounded descriptor/payload with explicit acceptance. Presentation receives only the accepted immutable descriptor. Provider lookup and version equality are exact.

The provider must not grant gameplay authority or return live Actor ownership to the proxy. Skeletal meshes, animation, Niagara, particles, lights, audio, dynamic materials, Blueprint assemblies, destructible/runtime meshes, and subjects without stable asset identity are unsupported by the built-in path and require a future approved provider implementation. M4P1 only proves the interface and fail-closed routing.

## 10. Required M4P1 proof

Targeted automation must cover all five policies; single falling-edge capture; no per-frame revision churn; immediate reacquire; second-edge revision; clear deletion; suppression retain/hide/restore; strict-black unknown; stale revision and delayed-command rejection; exact world/owner/floor/precision/profile isolation; forced hash collision; subject generation/identity reuse; PIE/world restart; teardown; unsupported-complex fail-closed; provider missing/reject/invalid/version mismatch; and proxy collision/tick/audio/VFX/light/navigation/gameplay isolation.

M3.4 presentation and M3.5 memory/static-environment focused regressions must remain green. Automated Lab screenshots are agent evidence only and never replace user-operated PIE.
