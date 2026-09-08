# Runtime stability evidence — 2026-09-08, company machine

Base: f80d9d1f7802cd5c2e9f82a21bed4f8c7b3468f7. Tested source is the runtime source of the commit containing this directory; binary SHA256 is recorded separately. UE 5.8.2 / RTX 4060 / D3D12 SM6. No cross-machine timing comparison.

- `original-crash.txt`: original compact Whole read reproduced BitArray / Sample / WriteWorldSnapshot assertion, exit 3. To reconstruct on the base revision, use the setup in `FDarkwellCurrentGridLifecycle` and call WriteWorldSnapshot immediately after AdvanceConfirmedWhole, before compact-to-dense Advance. This is a deliberately invalid transition reproducer, not a claim about the user's exact input sequence.
- `tiny-pose-counterfactual.txt`: scene-level 0.1 cm movement test with only the exact SnapshotTransform assignment reverted; rejected atomic capture loses legal Whole. Current test passes with exact pose and retained history. The original unrelated startup automation Condition failed lines are retained in the excerpt.
- `automation-summary.json` / `automation-tests.json`: final 19 clean tests including real-frame captures. Expected injected rejection in lifecycle test is intentional.
- `FinalD3D12/CleanBlackLab/`: initial Unknown; Live; Memory; Clear; blocked Live; leave; deactivate first frame and idle; reobserve. Captures use all normal components, temporal AA, persistent view state. No diagnostic component hiding.
- `before-floor-sort-*.png`: failed normal scene before fixed ground authored ordering propagated to history proxies. Compare with final clean 03_clear / 06_deactivated_first_frame. Same dimensions, box and 37-degree pose; physical geometry is unchanged.
- `FinalD3D12/BlackRegionTemporal/`: original heavy Lab 37-degree Clear/Block/leave/release/reobserve regression retained.
- `editor-build.log`: full editor target; pre-existing compiler preference / engine deprecation warnings.
- Final old/new 300-second runs are copied here with storage telemetry and actual game viewport. Runtime counters measure a finite run, not an infinite stability or arbitrary moving-history bound.

Raw investigation runs remain under ignored Saved on this machine, including deliberately failing counterfactuals and the discarded test-only null-owner floor diagnostic. Normal failing visual scenarios were retained and fixed, not removed from the final test.

The curated capture set includes CleanBlackLab, BlackRegionTemporal and normal SpatialPartialCut/A stages. Repeated B/C runs, sample CSVs and diagnostic component-isolation images remain in Saved; they are not acceptance images. The final runner source.patch is retained in Saved and its SHA256 is included here. Log excerpts have trailing whitespace normalized.
