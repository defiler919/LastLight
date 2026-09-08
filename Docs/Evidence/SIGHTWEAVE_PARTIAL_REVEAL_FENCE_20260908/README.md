# Partial reveal fence — company D3D12 evidence

Base runtime: 842c66c2948fbf1a73aa0074ec32ac069b99c880. UE 5.8.2, RTX 4060, D3D12 SM6. Current source is the containing commit; binary SHA256 and final test report are recorded here. No cross-machine timing comparisons.

- Before: PartialCurrentBeforeMinus15, original runtime plus diagnostic test only. `probe_180.png` is partially observed Current. `probe_left.png` is the same partial capture after turning away: the full fence. `probe_full.png` follows broader observation.
- After: PartialFenceFinal, matching positions, -15-degree observer yaw, 37-degree primitive, and frame sequence. All ordinary source, cap and history components remain enabled. No alpha overrides, AA disabling or material modifications.
- Opposite: 85-degree observer yaw, partial capture from the other side. The preserved cut shows that the result is not forced Whole rendering.
- `*_submitted_0/1/2/3.png`: active submitted Current texture channels R=appearance, G=Live blend, B=stale, A=legal coverage. Historical `*_history_2/3.png`: B=frozen opacity, A=hard ownership. These are channel visualizations, not screenshots of CPU masks.
- Each raw.zip contains exact CSV values: Local coverage/current-observation/capture masks, coarse world raster RGBA and sealed fine capture/geometry/opacity/AA/state. Before also includes its diagnostic source.patch. The After CSVs were collected in PartialFenceRegression2; their data match PartialFenceFinal, including byte-identical fine CSV. The final and opposite reports include exact transforms and grid metadata.
- `comparison.json`: before/after hashes proving unchanged Current inputs/submission, plus captured and zero-envelope counts. Capture increased by 369 physical-edge samples, explicitly authorized by the user after the lossy mapping was identified. Local authority precision and rules were not changed.
- `03_clear`, `06_deactivated_first_frame`, `07_reobserved` preserve the ordinary Trigger sequence. The full 21-test report also covers original Unknown/Partial/Whole/cap/residency/pose/lifecycle tests.

Raw intermediate failures remain under Saved, including the AA-only attempt that did not fix capture A and the first broad regression's Whole synthetic-fixture scope assertion. Neither was accepted as a pass. Final 21/21 passed (one external HTTP timeout warning), opposite 1/1 clean, full editor build succeeded.
