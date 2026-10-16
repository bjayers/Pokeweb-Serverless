# Background music runtime

`BgmToggleB.dll` / `BgmToggleW.dll` (4.0.0) support US Black / White revision 0.
`BgmToggleB2.dll` / `BgmToggleW2.dll` (3.0.1) retain the existing US BW2 runtime.
All four provide shared background-music controls. Their versioned
configuration supports a shortcut, any number of distinct streamed-BGM
mappings within native ID/export limits, or both. Version 3.0.1 uses a
variable-length ABI 2 mapping table and migrates ABI 1 single-track installs.

## Black and White player (4.0.0)

Clean BW1 archives have 2,076 sequences and no streams or stream players. The
Music editor now enables **BGM Replacements** and **Standalone**. Its reference
list uses the loaded BW1 archive's actual sequence symbols. **Streamed
Replacements** appears only if an eligible existing stream is present.

BW1 uses a resident buffered player built for this patch. It calls the retail
low-level stream driver, filesystem, channel allocator, and thread scheduler.
A worker reads bounded chunks from `rom:/wb_sound_data.sdat`; the audio alarm
only queues completed segments. Nothing reads or decodes a whole song into RAM.
The shared output buffer is 32 KiB (eight stereo PCM16 segments). PCM16 streams
are read directly; IMA ADPCM blocks are decoded into fixed scratch memory.
Generated stereo audio is 32,728 Hz. The game's Stereo/Mono option is respected.

The player reserves channels 0 and 6 through the native channel allocator and
releases channels and its alarm on stop. It closes its file on track changes or
completion. Buffer, decoder and worker memory belong to the resident module,
so sound-heap rollback cannot invalidate them. There is one worker and one
playback buffer regardless of library size. A failed read, invalid header,
channel-allocation failure, or missed buffer deadline stops playback and records
an error in `PokewebStreamDiagnostics`.

Native sequence state remains responsible for fades and effective volume.
Native instrument-track volume masks are retained per sequence player, so a
seasonal arrangement's muted instruments do not silence the entire replacement
or prevent native music from returning after shortcut unmute.
Explicit starts (including asynchronous map loading), same-ID restarts, and
restoration of a previous hierarchy player after battle all update the selected
stream. Genuine pause/resume retains a consumed-buffer cursor, with up to one
1,024-sample segment (about 31 ms) of timing granularity. Mute changes volume
without stopping or seeking playback. Standalone playback shares the same player; a new
game BGM request takes ownership back.

The first managed stream can use ID **0**. Installation creates a marked stream
player record only when needed. Managed stream records retain the original SDAT
alignment in their padding, preventing layout drift across repeated edits and
export/reimport. Removing the final managed stream removes an unused owned
player record. Existing native entries are preserved. **Update Music Runtime**
also upgrades older BW1 standalone-only projects without reconverting audio.

Both versions are pinned by ROM ID/revision and inspected executable signatures
in `bw1_native.json`. The installer checks raw ARM9 bytes and conflicting DLL
relocations before committing a music transaction. B/W binaries are independent
of the existing B2/W2 binaries. See [BW1 validation](VALIDATION-BW1.md) for the
checks performed and remaining hardware coverage.

### Standalone API

These exported Thumb functions may be imported by custom patches:

| Function | Behavior |
| --- | --- |
| `PokewebStreamPlay(streamId)` | Queue playback for the next sound update; returns 0 for an invalid 16-bit ID, otherwise 1 for an accepted request. Check diagnostics for start failure. |
| `PokewebStreamStop()` | Stop standalone playback. |
| `PokewebStreamPause(paused)` | Pause/resume standalone playback. |
| `PokewebStreamSetVolume(volume)` | Set standalone volume from 0 to 127. |
| `PokewebStreamPosition()` | Consumed position in source PCM frames. |
| `PokewebBgmMute(request)` | 0 queries; 1 toggles shared BGM mute. |

The caller owns the decision to start a standalone song; adding one in the
editor never redirects a game sequence. The caller should stop or pause other
game music if it does not want it mixed with standalone playback. Each explicit
play starts at sample zero, with the encoded loop point used at end of track.

Build with `python3 runtime/bgm-toggle/build_bw1.py`. Build logs and emulator
artifacts belong under `runtime/bgm-toggle/build/`. The `--test-driver` build
writes separate test DLLs there; its command mailbox is excluded from the
bundled production assets.

## Music editor and archive editing

On the Music page, choose a target and import audio to **Add Replacement**.
Choosing an already installed target changes the action to **Update Selected
Replacement**; it replaces only that track. The installed table has per-track
Select/Remove actions. Remove All restores every original sequence reference.
Adding/removing a track automatically installs the matching current runtime;
**Update Music Runtime** upgrades existing tracks without reconverting them.
Export and fresh-boot; do not load an old emulator state to test new runtime code.

The **Streamed Replacements** tab edits an existing SDAT `STRM`
directly. Retail US Black 2 and White 2 expose stream 0 (`STRM_TITLE`) for the
title/opening presentation. Imported MP3/WAV audio can replace it as PCM16 or
IMA ADPCM, with optional looping, without installing a sequence redirect, the
background-music DLL, or its 32 KiB gameplay replacement buffer. The categorized
stream reference lists the retail title stream, streams appended by sequence
replacements, and any other streams found in the active ROM. Appended custom
streams are read-only there and remain managed by their sequence entries.

Direct-stream removal verifies the current and original audio hashes, then
restores bytes from the ROM loaded for the saved project. Its management metadata
is project state rather than an embedded ROM marker. If an exported ROM is later
imported as a new project, that ROM becomes the new restoration baseline.

Both replacement panels can prepare a 15-second browser preview of the track
the selected target currently plays. Unmodified `SSEQ` targets use the browser's
SDAT sequence renderer; installed replacement streams and native `STRM` targets
are decoded directly from the active archive. Imported MP3/WAV controls honor
the configured crop window during playback. **Keep from** and **Keep until** are
source-file times, while a loop start is relative to the cropped result. Cropping
is sample-aligned, happens before resampling/encoding, and never modifies the
source file on disk.

Each mapping uses a private silent SSEQ and native STRM. The installer verifies
the owned tail, rebuilds it in one transaction, and renumbers surviving owned
IDs when a middle entry is removed. Unrelated content and BW2 title stream 0 are
preserved. BW1 can use stream 0 for a managed song. Unexpected external references or changed owned files cause an error
instead of an unsafe removal. Runtime configuration includes a sorted table,
relative offset, count, ABI marker, and FNV-1a integrity checksum. Audio hashes
are checked against saved project metadata. Export/reimport reconstructs all
mappings, although original source filenames are not stored in the ROM.

There is no fixed application track-count cap. Native IDs are 16-bit with
0xffff reserved; each target can have one replacement. The installer still
enforces its **512 MiB export safety limit**; this is not a claim about the
SDK's maximum addressable ROM size. Multi-track support does not raise that
limit or establish 2 GiB exporter/flashcart compatibility.

Press **L + R + Select** to mute or unmute BGM. This changes volume rather than
pausing the sequence player, so battle scripts waiting on music state continue
normally. Genuine game pause requests save a native stream offset and resume
there. This offset is block-granular rather than a sample-accurate DAC cursor.

## Black 2 and White 2 native player

For BW2 streamed replacement, the update hook performs a constant-time sequence
and state check while the ID is unchanged. A changed ID or explicit play request
uses binary search through the sorted mapping table, stops the previous stream,
and selects the next one. Sequence play, pause, fade, and volume calls are mirrored to
BW2's native stream player. The DLL has 36 bytes of BSS, including a four-byte
native handle separate from the title screen's temporary handle. It initializes
native streaming on demand, using the existing sound heap. Configured DLLs
materialize BSS as zero bytes before appending the table, preserving all existing
relocation addresses. The table costs **10 resident bytes per replacement**,
plus a fixed header and two cached IDs; native SDAT INFO/FAT metadata also grows.
There is still only **one shared playback buffer**, not one buffer per song.
The buffering policy introduced in version 2.0.3 requests
a **32 KiB native stereo output buffer**, about 250 ms at 32,728 Hz, instead of
the retail 4 KiB / 31 ms buffer. That is **28 KiB additional sound-heap use**, not
the size of the imported song. The fixed US sound heap is 628 KiB; it is not an
expandable allocation from all remaining DS RAM.

The allocation-site hook applies only during replacement initialization. It
keeps the native allocator, 32-byte alignment, dispose callback and heap section.
It requires at least 8 KiB payload headroom after the larger allocation and its
bookkeeping, and falls back to 4 KiB on low capacity or allocation failure.
Fallback favors preserving playback over removing every hitch. The reserve is
not proof that every future sound allocation in every game mode will fit.

Live native buffers are reused on restarts, avoiding another allocation each
time. The game's sound-heap rollback frees them and clears the native pointer;
the patch does not retain a pointer to rolled-back memory. Stopping the stream
alone does not free a frame-heap allocation. Title-screen and toggle-only calls
retain the retail allocation policy. There is no new audio thread, alarm,
patch-owned sample buffer or streaming heap.

Tradeoffs are less free sound-heap capacity and a larger initial prefill/read
burst. Steady-state audio bitrate and the update hook are unchanged. Genuine
pause/resume subtracts the added prefetch duration from the native read-head
offset, clamped at zero; it remains approximate near block/loop boundaries.
Shortcut mute/unmute does not stop or seek the stream. Both PCM16 and generated
ADPCM feed this PCM16 output buffer; the generated ADPCM blocks remain at most
484 bytes, fitting the native 512-byte decode scratch space even during a large
prefill.

Version 2.0.2 resolves all five hook destinations to ARM9 addresses, initializes
streaming during gameplay even when Quick Launch skips the title, treats a
prepared handle at playback position zero as valid, and preserves the native
track-mask/volume argument order. Existing installations can use **Update Music
Runtime** on the Music page without reconverting their audio.

Version 2.0.3 adds a sixth, version-specific native allocation-site hook and
the guarded buffering policy above. Use **Update Music Runtime**, export, and
fresh-boot the ROM using a normal battery save. Loading an old emulator state
restores the old runtime and its buffers.

The earlier 2.0.2 playback was checked in fresh Route 19 wild battles on both US revisions using
the browser emulator core, with recorded output matched against the imported
track. White 2 shortcut testing also confirmed silent output while muted and
continued playback position across mute/unmute. This is not a complete gameplay
validation: catching tutorials, captures, battle exit/re-entry, lid resume, and
the other transition cases still need dedicated checks. **No emulator gameplay
tests were run for 2.0.3.** Its verification covers host-native allocation-policy
tests with address/undefined-behavior sanitizers, both retail ROM signatures,
DLL relocations, installer conflict rejection, and audio-preserving upgrades.
Battle-start dropout removal still needs user gameplay verification. Also check
repeat encounters, capture/flee endings, cries/SE, and genuine pause/resume.

**No emulator gameplay tests were run for 3.0.0.** Host tests cover mapping
lookup, configuration migration/integrity, preserved relocations, transactional
failure handling, per-track edits, export/reimport, and shortcut coexistence.
The full retail B2/W2 sound archives were each checked with all 273 BGM targets
mapped, partial removal to 182, unchanged original files/title stream, and
byte-exact full uninstall. These are archive checks, not playback validation.
User gameplay checks should include mapped overworld → mapped battle → mapped
overworld, switching to an unmapped theme, low-HP/victory music, repeated battles,
the catching tutorial, native fades/pause/resume, and mute/unmute.

The Music editor chooses encoding per target. New targets default to
block-seekable stereo IMA ADPCM at 32,728 Hz, which is about 75% smaller than
PCM16; maximum-quality stereo PCM16 remains available. Existing targets reopen
with their installed encoding selected. Programmatic callers that omit an
encoding retain the earlier 128 MiB automatic compression policy; that threshold
is a storage policy, not a native SDAT limit.
ROM export updates the DS-accessible region boundary when files grow, so the
game can still open gameplay archives displaced by a larger sound archive.

Run `python3 runtime/bgm-toggle/build.py` from the application repository to
rebuild the two bundled DLLs. The build emits reports in `runtime/bgm-toggle/build/`;
those reports are generated and are not source inputs.

Use `python3 runtime/bgm-toggle/verify.py` to verify every patched retail ARM9
call site and native entry point against local Black 2 and White 2 inputs. Use
`npm run bgmtoggle:test` for host-only allocator tests (no DS emulation). Use
`npm run streamedbgm:verify-rom -- <b2.nds> <w2.nds>` to round-trip SDAT
installation and removal on full ROM sound archives.

## Shared C-Gear mute control (3.0.1)

`PokewebBgmMute(request)` is a resident exported Thumb API: 0 queries the
current mute state, 1 toggles it, and the return value is 0 for audible or 1
for muted. It uses the same state and volume path as L + R + Select, including
replacement streams. It accepts no pointers and allocates no resources.
The C-Gear installer upgrades this runtime without changing existing music
mappings or shortcut settings.
