# Summary Type Icons validation

Version 1.0.0, English US Black 2 and White 2 revision 0.

Both modules compile without unresolved symbols. Their retail overlay hashes,
hook targets, native helper signatures, VBlank thunk and VRAM configuration are
checked independently. The delivered artwork has 17 32×16 character resources
using only 0–15 pixel indices and RGB555 palettes. Transport is 8bpp with extended
OBJ palettes, rather than claiming independent ordinary 4bpp palette banks.

Compiled Thumb execution checks cover stack/callee-saved registers, seven-argument
native palette loading, all 17 tile/palette pairs on two information-page actors,
type changes, unchanged native actor/resource state, uploads during VBlank,
information-page show/hide behavior, missing-archive and every allocation-failure
unwind, badge positions and teardown. Both games pass.

The 69 focused installer, Summary IV/EV and Code Injection UI tests pass.
Both clean-ROM install/export/reimport checks pass, including reinstall, staged
removal, IV/EV coexistence, exact native archive members and unchanged input hashes.
TypeScript checking and the production application build pass. Emulator verification is recorded separately below;
CPU and export checks are not presented as visual acceptance.

Live DS rendering checks use the bundled emulator and an isolated fixture save.
The final profile uses two replacement actors on the information page only.
Both games pass the isolated 3,550-frame scenario alongside the Summary IV/EV
Viewer. White 2 also passes explicit replacement-actor visibility assertions.
The isolated scenario covers the first Type row and Psychic badge, native
Dark/Fire/Ice/Rock move badges and move details, IV/EV navigation, switching to
Ground/Steel Excadrill, and returning to the field with cleared resources.
Every type palette is additionally covered by the compiled CPU checks; the
live scenario does not cover every type or every move-learning context.
White 2 move-screen comparisons against the same fixture with unmodified icons
check the opaque pixels of all four move badges, the move category and the
Pokémon type on move details. The information actors' screen rectangles match
the native baseline on Stats. Actor visibility traces check that replacements
stay hidden on Stats, move details and the IV/EV pages. The native selected move
plate's flashing phase is not included in the exact badge comparison.
Hardware and DSi execution have not been tested. Unsupported languages, revisions,
BW1, other native Summary layouts and Fairy/Stellar expansion are outside this
profile.
