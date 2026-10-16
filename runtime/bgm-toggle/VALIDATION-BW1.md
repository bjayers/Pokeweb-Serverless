# BW1 streamed-music validation

Runtime: **4.0.0**. Validation date: **2026-10-14**.

Navigation follow-up, **2026-10-15**: removed the remaining BW2-only Music
route gate and corrected its disabled tooltip. Executing the actual route gate
confirmed access for B/W/B2/W2 with a loaded ROM, and rejection without the ROM,
without a project, or for Gen 4. The production application build passed again.

## Supported inputs and installation

The player targets US Black (`IRBO`) and White (`IRAO`), revision 0. Both clean
archives contain 2,076 sequences, 179 BGM entries, and zero native streams or
stream players. The first managed stream uses ID 0.

| Input | Original ROM SHA-256 |
| --- | --- |
| Black | `b9979189d299a0231d01c8885c0d054b706976ffc75427ca59d84e0ce1493034` |
| White | `b288bb061fd646894f5059f55cd0a1efb13b4f0cfd3d9e06e9e42a5bd9431ac6` |

The canonical profiles in [bw1_native.json](bw1_native.json) record decompressed
ARM9 identities, seven hook signatures, and 29 native entry-point signatures
per version. Installation checks the ROM ID/revision, those bytes, and
overlapping ARM9 relocations from other installed or staged DLLs. Full-ROM
hashes identify the inspected inputs; installation does not require a wholly
unmodified ROM if the checked code remains compatible.

The Music editor offers BGM Replacements and Standalone for BW1, using names
from the loaded SDAT. Streamed Replacements appears only when an eligible
existing stream is present. Standalone songs do not redirect a native sequence;
custom features can call the exported API documented in [README](README.md).

The existing MP3/WAV browser decoder, crop controls, loop controls, encoders,
and project transactions are reused. This work does not replace browser audio
decoding. BW2 retains its existing 3.0.1 binaries and runtime behavior.

## Automated checks

| Check | Result |
| --- | --- |
| Native decoder | 72 PCM16/ADPCM comparisons against the TypeScript decoder; seeks, arbitrary loop points, tiny/partial blocks, and non-looping tails. Malformed headers rejected. Address and undefined-behavior sanitizers passed. |
| Music and injection unit tests | 195 tests passed across eight test files, including BW2 music and C-Gear integration. |
| Both complete BW1 archives | 179 simultaneous BGM replacements, partial removal to 119, standalone round-trip, stream ID 0, then byte-exact complete SDAT restoration. |
| Both complete BW2 archives | 273 simultaneous BGM replacements, partial removal to 182, standalone round-trip, native title stream preserved, then byte-exact complete SDAT restoration. |
| Both BW1 projects | Standalone runtime installation; multiple replacements; crop/resample/loop configuration; metadata serialization/reload; runtime updates; export/reimport; standalone removal with BGM renumbering to stream 0; byte-exact final SDAT restoration. |
| Failure handling | Invalid conversion, failed runtime fetch, incompatible hook bytes, and a conflicting DLL reject without committing partial music changes. |
| Build | TypeScript check and production application build passed. The existing Vite chunk-size warning remains. |

The archive tests compare all original sound files and unrelated sequence
references, then compare the complete restored archive bytes. This catches
alignment drift as well as incorrect IDs. Project tests also verify the source
ROM hashes after export and reimport.

## Emulator checks

Tests fresh-boot separate ROM/save copies in the application's **headless
WASM DeSmuME core**. They do not load old emulator save states. Audio is captured
as stereo PCM and checked against distinguishable test tones.

Production Black and White builds each ran for 10,400 frames. Both reached the
overworld, entered a trainer battle, played mapped battle music, toggled mute
and unmute, retained native sound effects while muted, defeated the opponent,
and restored the mapped overworld music on battle exit. Both runs completed
without reported player faults or buffer underruns.

The separate `--test-driver` build exposes a test mailbox that invokes normal
game sound controls on the sound-update thread. It is excluded from bundled
production DLLs. This enables deterministic checks of native fades, pause,
volume, same-ID restarts, unmapped music, and the standalone API. Test outputs
include frame captures, `trace.json`, `capture.wav`, and machine-readable audio
verification results under the ignored `build/emulator-*` directories.

Both instrumented versions completed **10,600 frames** and passed:

- BGM playback using stream ID 0.
- Native pause/resume with a stationary consumed cursor while paused.
- Audible volume changes and a 60-frame native fade to silence.
- Unmapped native music, including shortcut mute/unmute and preservation of
  individual instrument-track volume settings.
- Explicit restart of the same sequence ID.
- Standalone PCM16 playback, non-looping completion, and pause/resume.
- Stereo separation exceeding 40 dB with distinct left/right tones.
- Audible content beyond 60 seconds and a completed 75-second loop with a
  nonzero loop point.

Neither instrumented run reported a player fault or buffer underrun. These
tests caught and resolved two integration issues: restoring the previous
hierarchy player after battle without a fresh sequence-start call, and treating
an individual native instrument's volume as whole-song volume.

## Memory and cleanup

The production resident image is **47,548 bytes** per version, plus **10 bytes
per BGM mapping**. That includes the 32 KiB output buffer, 4 KiB worker stack,
decoder scratch, state, and code. PMC loader overhead and SDAT metadata are
additional. Song duration does not change the buffer size.

Eight stereo segments buffer about 250 ms at 32,728 Hz. The worker reads and
decodes bounded chunks; the audio callback queues refill work. Stop releases
native channels and the stream alarm. Track changes and completion close the
archive file. The fixed worker and its storage remain resident for the module's
lifetime. Pause/resume uses a consumed-segment cursor with approximately 31 ms
granularity. An I/O failure or missed refill deadline stops the stream and sets
a diagnostic error.

Only generated test ROMs and copied fixture saves were used. Source ROMs and
the original fixture save remain unchanged; the emulator runner verifies their
hashes before and after execution. The original fixture SHA-256 is
`96809dcdf8eea9413b27aa801b5129e0cce112172100a4d8324516f8fd74156d`.
No user battery save is modified by these tests.

## Reproduction

Run from the application repository. Supply your own inspected US ROM paths.
The runtime build uses the same ARM toolchain and RPM converter as the existing
BW2 runtime build. Audio verification additionally requires Python NumPy.

```sh
python3 runtime/bgm-toggle/build_bw1.py
npx vite-node scripts/verify-bw1-stream-decoder.ts
npx vite-node scripts/verify-bw1-music-project.ts /path/black.nds /path/white.nds
npx vite-node scripts/verify-streamed-bgm-install.ts /path/black.nds /path/white.nds /path/black2.nds /path/white2.nds
npx vitest run src/test/bgmRuntimeMappings.test.ts src/test/streamedBgmModel.test.ts src/test/pmcModel.test.ts src/test/musicEditor.test.ts src/test/cgearQuickActionsBw1.test.ts src/test/cgearQuickActionsModel.test.ts src/test/codeInjectionInstall.test.ts src/test/codeInjectionEditor.test.ts
npx tsc --noEmit
npm run build
npx vite-node runtime/bgm-toggle/emulator_bw1.ts /path/black.nds --battle --stereo --frames 10400
npx vite-node runtime/bgm-toggle/emulator_bw1.ts /path/white.nds --battle --stereo --frames 10400
python3 runtime/bgm-toggle/build_bw1.py --test-driver
npx vite-node runtime/bgm-toggle/emulator_bw1.ts /path/black.nds --controls --stereo --frames 10600
npx vite-node runtime/bgm-toggle/emulator_bw1.ts /path/white.nds --controls --stereo --frames 10600
python3 runtime/bgm-toggle/verify_bw1_audio.py runtime/bgm-toggle/build/emulator-B-battle runtime/bgm-toggle/build/emulator-W-battle runtime/bgm-toggle/build/emulator-B-controls runtime/bgm-toggle/build/emulator-W-controls
```

## Remaining coverage

Physical DS/DSi hardware, flashcart throughput, lid sleep/wake, and exhaustive
game-area/transition coverage have not been tested. The memory figure describes
this module, not a guarantee that every combination of other patches fits.
The emulator runs test generated tones through the playback path; they are not
new browser-level MP3 decoder tests. Other regions and revisions remain
unsupported until their bindings and hooks are verified.
