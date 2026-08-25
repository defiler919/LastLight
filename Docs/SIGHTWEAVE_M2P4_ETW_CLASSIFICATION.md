# SightWeave M2P.4 Authoritative ETW Tail Classification

## Decision

The elevated ten-process attribution matrix is complete and authoritative for
the M2P.4 production decision gate.

- Batch512 intrinsic on-CPU passes: p50/p95/p99 are
  93.7/148.4/191.7 us. Do not rewrite the Batch production path. Preserve its
  81 real Plugin CPU tail samples and 72 scheduler samples as wall telemetry.
- Broad Dynamic Door intrinsic on-CPU fails: p50/p95/p99 are
  160.2/277.4/351.1 us. Implement the exact incremental dynamic-occluder
  angular-sector path specified by M2P.4, with explicit full-solve fallback.
- Production source remained untouched through this decision. The
  optimization gate is now open only for Dynamic vision work, not for Batch.

M2P.4 remains `IN_PROGRESS`. This document authorizes architecture and
implementation work; it is not final performance closure.

## Authority and capture

The default command host is medium-integrity, so every kernel capture ran in a
UAC-approved high-integrity child. The preserved capability record proves:

- administrator role: true;
- integrity SID: `S-1-16-12288` (High);
- `fltmc`: exit 0;
- WPR profile: `GeneralProfile.Verbose.File`;
- affinity/priority changes: none.

The authoritative scheduler stream is the Windows kernel Thread provider
`3d6fa8d1-fe05-11d0-9dda-00c04fd7ba7c`, using CSwitch opcode 36 and
ReadyThread opcode 50. Process/Thread lifecycle records validate marker
PID/TID ownership. GeneralProfile also preserves sampled profile, page-fault,
and system context in the raw ETLs; authoritative microseconds here come only
from QPC-bounded scheduling intervals, never from raw-cycle/frequency
conversion.

The fixed matrix contains:

| Evidence | Count |
| --- | ---: |
| Elevated independent processes | 10 |
| Batch distributions | 100 |
| Batch total samples | 10,100 |
| Dynamic Door total samples | 3,140 |
| Total/control/exact-stage markers | 105,120 |
| Target-thread scheduling events | 144,728 |
| CSwitch events observed in the ETLs | 4,540,774 |
| ReadyThread events observed in the ETLs | 2,520,179 |
| ETW events lost / buffers lost | 0 / 0 |
| Unclosed timelines / Unknown | 0 / 0 |

Every marker is keyed by run, sample ID, PID, TID, and QPC begin/end. The
offline consumer reconstructs on-CPU, ready, blocked, context switches,
preemptions, migrations, core residency, and a relative interval timeline.
Clock mismatch, lifecycle conflict, malformed scheduling payload, trace loss,
or failed closure is a hard Unknown.

## Calibration

The final calibration contains 188/188 closed timelines, with zero loss and
zero Unknown:

| Workload | wall p50 | on-CPU p50 | ready p50 | blocked p50 |
| --- | ---: | ---: | ---: | ---: |
| Empty probe | 0.0 us | 0.0 us | 0.0 us | 0.0 us |
| Fixed compute | 116.3 us | 116.3 us | 0.0 us | 0.0 us |
| Fixed memory | 168.5 us | 168.5 us | 0.0 us | 0.0 us |
| Sleep 10 ms | 9,800.2 us | 39.8 us | 36.8 us | 9,649.6 us |
| Yield under load | 81,301.7 us | 2,133.1 us | 79,159.1 us | 0.0 us |

Loaded yield produced 2,840 context switches/preemptions and 1,184 migrations.
The test-only exact-stage marker probe measured on-CPU p50/p95/p99 of
2.5/4.1/6.6 us. Synthetic coverage proves event-loss fail-closed behavior,
migration/preemption reconstruction, and cross-process PID/TID rejection.

## Formal timing

| Workload | samples | wall p50/p95/p99/max (us) | on-CPU p50/p95/p99/max (us) |
| --- | ---: | ---: | ---: |
| Batch512 | 10,100 | 93.7 / 149.7 / 220.4 / 790.1 | 93.7 / 148.4 / 191.7 / 787.5 |
| Broad Door 4V2L | 1,010 | 160.2 / 280.7 / 386.8 / 772.8 | 160.2 / 277.4 / 351.1 / 522.6 |
| Dedicated Door | 1,010 | 34.2 / 68.5 / 99.1 / 250.2 | 34.2 / 68.5 / 94.3 / 142.1 |
| Door plus motion | 1,010 | 230.8 / 392.9 / 506.0 / 777.1 | 230.8 / 378.2 / 468.9 / 658.7 |
| Held-reader diagnostic | 110 | 162.6 / 275.6 / 318.5 / 584.7 | 162.6 / 275.6 / 318.5 / 558.8 |

Batch passes all intrinsic gates: median <=150 us, p95 <=180 us, and p99
<=200 us. Broad Door passes its median gate but fails p99 <250 us.

## Final classification

Plugin CPU requires both total on-CPU above the workload budget and an exact
stage whose per-sample on-CPU exceeds that operation/stage median by at least
5 us. The smallest observed qualifying growth was 75.4 us for Batch, 45.6 us
for Broad Door, and 7.5 us for Door plus motion. Thus no candidate was promoted
without internal-stage growth.

| Workload | Within budget | Plugin CPU | Scheduler/Preemption | Migration/Frequency | Unknown |
| --- | ---: | ---: | ---: | ---: | ---: |
| Batch512 | 9,947 | 81 | 72 | 0 | 0 |
| Broad Door 4V2L | 925 | 79 | 6 | 0 | 0 |
| Dedicated Door | 1,009 | 0 | 1 | 0 | 0 |
| Door plus motion | 741 | 269 | 0 | 0 | 0 |
| Held-reader diagnostic | 100 | 10 | 0 | 0 | 0 |

No sample is labeled Migration/Frequency because core migration alone is not
sufficient and this matrix did not establish independent power/frequency
causality. Migrated intervals with authoritative ready time remain under
Scheduler/Preemption.

Batch Plugin CPU contributors are batch result materialization 79/81 and
batch classification 2/81. This is real retained tail evidence, but the fixed
decision gate is aggregate p99; because 191.7 us passes, Batch is not a
production rewrite target in M2P.4.

Broad Door Plugin CPU contributors are:

- vision solve: 69/79;
- illumination solve: 6/79;
- snapshot materialization: 2/79;
- occluder normalization: 1/79;
- prepared-index invalidation: 1/79.

Vision is the dominant systematic contributor and is the only production
optimization authorized by this checkpoint. Rare non-vision tails remain
visible and must be reclassified after the sector implementation.

## Representative timelines

The worst Broad Door sample is run-06/sample-53, PID 9212/TID 27892:

- wall 772.8 us;
- on-CPU 522.6 us;
- ready 250.2 us;
- 14 context switches, 13 migrations;
- vision solve on-CPU 344.3 us;
- core residency:
  `0:55.2|2:134.5|3:54.7|8:145.9|9:23.9|10:5.1|11:103.3` us.

Its timeline begins:

```text
cpu@8:0.000-53.000
ready:53.000-57.800
cpu@10:57.800-62.900
ready:62.900-183.900
cpu@9:183.900-207.800
...
ready:655.300-669.500
cpu@11:669.500-772.800
```

This sample is Plugin CPU, not a pure scheduler tail: removing 250.2 us of
ready time still leaves 522.6 us on-CPU, with 344.3 us in vision.

The representative Broad Door scheduler sample is run-07/sample-49:

```text
wall=411.7 us, on_cpu=216.5 us, ready=195.2 us
cpu@5:0.000-25.300|ready:25.300-220.500|cpu@0:220.500-411.700
```

The worst Batch Plugin CPU sample is run-06/sample-0:

```text
wall=790.1 us, on_cpu=787.5 us, blocked=2.6 us
cpu@1:0.000-672.200|blocked:672.200-674.800|cpu@5:674.800-790.100
batch_result_materialization_on_cpu=776.9 us
```

All wall samples, including every sample >=400 us, remain in the authority
classification CSV/JSON.

## Evidence paths

- Capability:
  `Saved/SightWeaveM2P4/EtwAttribution/admin-uac-formal01/capability.json`
- Per-process ETLs/reports/manifests:
  `Saved/SightWeaveM2P4/EtwAttribution/admin-uac-formal01/run-01` through
  `run-10`
- Raw combined wall data: `batch-all.csv`, `door-all.csv`
- Original attribution: `attribution-all.csv`
- Interval attribution: `attribution-timeline-all.csv`
- Capture summary: `summary.json`
- Final classification: `authority-classification.csv` and
  `authority-classification.json`
- Calibration:
  `Saved/SightWeaveM2P4/EtwCalibration/admin-uac-final02`

## Next step

Write `Docs/SIGHTWEAVE_M2P4_DYNAMIC_SECTOR_ARCHITECTURE.md` before editing
Runtime source. The design must cover old/new projected angular intervals,
endpoint epsilon, wrap-around, exposed background candidates, exact nearest
intersection and stable-ID tie-breaks, seam/topology validation, revisioned
cache reuse, bounded memory, explicit fallback reasons, zero allocation, and
Reference/Optimized/gameplay differential authority.
