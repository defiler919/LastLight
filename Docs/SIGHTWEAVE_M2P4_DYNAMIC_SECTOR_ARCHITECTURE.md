# SightWeave M2P.4 Incremental Dynamic Occluder Angular Sector Architecture

Status: **AUTHORIZED FOR IMPLEMENTATION**

Branch: `codex/m2p4-sightweave-etw-dynamic-sector`

Evidence checkpoint: `88937aa` (`docs: record SightWeave authoritative tail classification`)

## 1. Decision gate

The elevated ten-process ETW matrix reconstructed every formal sample from kernel
context-switch events with zero event loss, zero unclosed intervals, and zero
`Unknown` classifications. Batch512 passed its intrinsic CPU gate at 191.7 us
on-CPU p99 and therefore must not be rewritten. The broad 4-vision/2-illumination
dynamic-door workload failed its intrinsic gate at 351.1 us on-CPU p99. Of its
79 plugin-CPU tail samples, 69 were attributed to vision solve. This evidence
authorizes only the exact dynamic vision optimization described here.

## 2. Authority invariant

`UpdateOccluder` remains a synchronous game-thread authority operation. A
successful call publishes a snapshot containing the new occluder geometry before
returning. Readers holding an older immutable snapshot continue to observe that
older revision. Illumination keeps the established full Optimized solve path.

An incremental result is required to be the same result the current full
Optimized solver would produce. It is not a visibility approximation. Candidate
angle ordering, nearest exact intersection, duplicate handling, stable-ID tie
breaks, polygon construction, seam handling, topology validation, snapshot
revision, attribution, and query answers retain their existing policies.

## 3. Data flow and ownership

For an enabled dynamic-to-dynamic occluder transform, `UpdateOccluder` exposes a
scoped change descriptor only while it synchronously publishes the new snapshot:

* occluder handle;
* exact old and new normalized segment arrays;
* prior and published world revisions.

The descriptor never escapes publication and never owns a UObject or a snapshot.
The old segment storage is the subsystem's retained dynamic-update scratch after
the established swap; the new storage is the live occluder record. Both remain
valid for the complete synchronous rebuild.

For each dirty vision source, the previous snapshot entry contributes the prior
candidate angles, nearest distances, and boundary points. The source-local
Prepared Event Index binding contributes the exact old prepared segment slots.
The existing exclusive-cache miss path intentionally retains that binding. An
incremental attempt is allowed only when acquisition returns that same binding,
the source and prior occluder revisions match, and the cache exactly describes
the old input.

Per-thread solver scratch owns temporary previous-result arrays. Arrays are
swapped rather than copied, so warmed updates reuse their established capacity.
The Prepared Event Index memory accounting is unchanged because result arrays are
not retained by the index.

## 4. Dirty angular sectors

For the one changed stable-ID segment, the solver computes the exact angular
projection of both the old and new segment from the source origin. Each projection
is expanded by:

* the solver's endpoint `+/- epsilon` event offset;
* the prepared segment angular padding used by the interval accelerator;
* the existing numeric comparison tolerance.

Wrapped projections are split into canonical intervals in `[-pi, pi]`, sorted,
and merged. This union is the dirty set. It covers every ray that could intersect
the changed segment before or after the update, including endpoint-adjacent rays
and the `0/2pi` seam. A ray outside both expanded projections cannot intersect the
only changed segment in either revision; with an unchanged source and unchanged
remaining segments, its established nearest hit is still authoritative.

The implementation rebuilds the complete candidate event set for the new input,
not merely the door events. Consequently the dirty sectors contain and recompute:

* surviving boundary events;
* new door endpoint events;
* all static and dynamic candidate events behind the moved door;
* exact nearest intersections against every active candidate segment;
* stable-ID ties and boundary points.

Removed old-door events naturally disappear from the new event set. Unchanged
events outside the dirty set are reusable only by an exact double angle match to
the previous ordered array. Missing or ambiguous matches force that ray to be
rebuilt; no nearest hit is inferred from a neighboring sample.

## 5. Seam and topology parity

Every transition between a dirty and reusable interval must have unchanged,
exactly matched guard events on the reusable side. Wrapped sectors treat
`-pi/pi` as one circular boundary. If guard ownership or candidate ordering cannot
be established, the attempt falls back.

Polygon vertices are rebuilt in canonical candidate order from the combined
recomputed/reused boundary points, including cone-origin behavior and duplicate
vertex suppression. The established closing-vertex removal and local topology
validation then run over the complete polygon. A topology failure discards the
attempt and executes a full Optimized solve.

## 6. Exact fallback contract

The full Optimized solver is mandatory when any of the following applies:

* dirty coverage is greater than the bounded sector threshold;
* the source is on or within the guarded tolerance of the old/new segment;
* there is not exactly one changed stable-ID candidate slot;
* old/new stable IDs, floor, height eligibility, slot layout, or source invariants
  do not match;
* multiple changed segments or complex overlapping dynamic intervals are present;
* the old result arrays are absent or internally inconsistent;
* seam guard events or endpoint ordering cannot be validated;
* the incremental polygon is degenerate or fails topology validation;
* the source-local Prepared Index binding is absent, replaced, evicted, over its
  memory cap, or otherwise does not match the prior revision;
* the occluder is enabled/disabled, registered/deleted, or updated non-dynamically;
* a source transform or source geometry revision changed concurrently;
* development Verify/Reference differential validation reports a mismatch.

Fallback is explicit and produces the ordinary full Optimized result in the same
call. It must never publish a partial incremental result or silently approximate.

## 7. Diagnostics and bounded memory

Subsystem diagnostics record, in Development/Test builds where appropriate:

* incremental attempts and successful sector updates;
* full fallbacks by stable reason code;
* dirty radians/degrees (last, accumulated, and maximum);
* rebuilt and reused candidate rays/vertices;
* incremental and fallback elapsed time;
* warmed capacity growth/allocation evidence.

The solver's existing result storage and bounded thread-local frames own all hot
arrays. Expected high-water capacity is established during warmup. No worker
tasks, latent work, new UObject ownership, or unbounded per-update history is
introduced.

## 8. Verification gates

Dynamic differential coverage must compare every incremental publication with a
fresh full Optimized solve and the Reference solver. Comparisons include canonical
vertices, candidate angles/distances/boundary points, dense gameplay
classification, attribution/rejection fields, and revisions. Coverage includes
open/close and repeated toggles, translation/rotation, narrow and layered doors,
collinear and seam cases, near-source fallback, source motion, disjoint/overlapping
doors, source/floor/height/owner isolation, held snapshots, deletion/lifecycle,
multiworld, cache eviction/capacity fallback, allocation, and determinism.

The feature may close M2P.4 only if the authoritative ten-process broad-door
on-CPU median and p99 are both below 250 us, warmed allocation remains zero, all
correctness and lifecycle gates pass, and the complete wall/off-CPU telemetry is
preserved.
