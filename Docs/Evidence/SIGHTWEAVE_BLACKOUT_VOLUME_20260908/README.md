# Blackout Event Volume evidence

- Final build: full DarkwellEditor Win64 Development target, Succeeded (logs.zip).
- Final D3D12/SM6 run: BlackVolumeFinal, 9/9 passed, 8 clean + 1 HTTP connectivity warning; see summary/report. No SpawnActor warning.
- DLL SHA256: 708FB8CEB79E48A9C8870D27B32E357CE9C9AF85D2A721B4271E5632E86EB089.
- VolumeBlackLab: actual C++ character capsule overlap via SetActorLocation, with initialized playing world; 00 Unknown, 01 Live, 02 Memory, 03 entered/Clear, 04 blocked Live, 05 left Live/black, 06 exited/unblocked/no old gray, 07 reobserved/new Memory.
- Regression: existing 37-degree Partial reveal and clear/unblock captures.
- Manual: actual LaunchBlackRegionLab game window, 01 outside, 02 entered, 03 exited. Desktop short key taps did not sustain WASD movement. Supplemental positioning used engine BugItGo and Walk (BugItGo enables Ghost; Walk restores collision). Not a human WASD walkthrough. Real collision-enabled move-in/move-out is covered by VolumeDemo.
- Cyan wireframe is the independent event box, not the target knowledge AABB.
- logs.zip includes exact test source patch/metadata, test log, build log and actual game log. Starting HEAD is aa201c20b3c68ca3fad748e2c034b51d2795c0ae; source was unchanged after final build/test.
