# SightWeave M2P.3 performance contracts

## Status

`PARTIAL`, fail-closed. The old absolute wall assertions have not been removed,
renamed out of the full SightWeave prefix, relaxed, or converted to soft
telemetry. Current evidence still contains plugin-cycle tails and Unknown
samples, so the task's prerequisite for a completed dual-layer contract is not
met.

## Why the existing wall test is variable

The ordinary ten-process run contains stable Batch512 medians but variable
tails. Across 100 fixed distributions, all 100 medians meet 150 us, 99 p95s
meet 180 us, and 86 p99s meet 200 us. The fourteen failed p99 distributions,
including the 332.7 us worst per-distribution p99 and 1468.7 us maximum sample,
remain in `ordinary-10p-partial-prepared-02/batch-all.csv`.

The same run's broad 4-vision/2-illumination door path has p99 above 250 us in
all ten processes, while the dedicated dynamic-door control is below 250 us in
all ten. Raw cycle evidence attributes some tails to work inside result
materialization or vision geometry, but the host's per-sample `GetThreadTimes`
value is always zero at this scale and no ContextSwitch ETW trace is available.
Consequently neither a blanket Windows-noise conclusion nor an intrinsic CPU
microsecond pass is authorized.

## Current contract layers

1. Correctness and parity remain deterministic assertions in their existing
   suites. Their data, seeds, epsilon, query counts, fields, synchronous
   revisions, and optimized/reference comparisons are unchanged.
2. The existing wall gates remain hard failures. Batch512 still requires every
   in-process distribution to meet 150/180/200 us median/p95/p99 with zero
   capacity growth. The broad dynamic path's inherited 250 us wall target is
   still reported and is not called an intrinsic CPU pass.
3. Raw current-thread cycles, coarse kernel/user time, core migration,
   frequency evidence, page faults, adjacent fixed controls, and internal
   stages are attribution evidence. Cycles are never converted to time.
4. The frame-level soak gate hard-fails correctness loss, post-warmup result
   capacity growth, missing prepared-index hits/rebuilds, a sustained 60-frame
   run above 1 ms, an incomplete report, or a falsely labeled render mode. It
   preserves every frame including maximums and Unknown classifications.
5. An Intrinsic CPU microsecond gate is deliberately unavailable until exact
   running intervals exist. On this machine `GetThreadTimes` advances in a
   15.625 ms quantum; UE's ContextSwitch ETW path requires authority not used
   by the ordinary acceptance processes.

This is not a completed dual-layer contract. It is the enforceable boundary
that prevents unavailable CPU authority from being represented as a passing
metric.

## Reproducible fail-closed audit

`Scripts/TestSightWeaveM2P3PerformanceContracts.ps1` consumes only preserved
ordinary attribution and separately captured NullRHI/rendered soak artifacts.
It verifies exact process/distribution/row counts, unchanged wall limits,
classification counts, soak completeness, and D3D12 identity. It always writes
`performance-contract.json` before returning non-zero when any completion
condition is absent.

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\Scripts\TestSightWeaveM2P3PerformanceContracts.ps1 `
  -AttributionRoot .\Saved\SightWeaveM2P3\Attribution\ordinary-10p-partial-prepared-02 `
  -NullRhiSoakRoot .\Saved\SightWeaveM2P3\Soak\nullrhi-36000-final02 `
  -RenderedSoakRoot .\Saved\SightWeaveM2P3\Soak\rendered-36000-final01 `
  -OutputPath .\Saved\SightWeaveM2P3\Contracts\performance-contract-final02.json
```

Expected current result: a preserved JSON report with `status: PARTIAL`, then a
non-zero exit. A zero exit is permitted only when evidence is complete,
per-sample intrinsic CPU authority exists, Plugin CPU/Unknown overruns are
zero, all unchanged wall gates pass, and both frame-soak gates pass.
