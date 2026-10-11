# Party toolbar 0.1.6

An independent PMC module for English revision-0 Black 2 and White 2. Install or update it from **Code Injection → Graphical/UI → Enhanced Party Menu → Install/Update Party Toolbar**. The installer also installs **Infinite Rare Candy** atomically. Its Key Item slot must pass that patch's compatibility checks. The separate Enhanced Party Menu and Battle Log modules are optional. Disable Toolbar restores the native prompt and leaves Pokémon changes intact.

## Controls

The normal field Party Menu removes the “Choose a Pokémon.” box, including its entry/exit flashes, and places up to nine transparent icons on the dark bottom bar. Available icons pack toward the left in their original order, with 20-pixel spacing and no empty slots. Missing Infinite Candy also closes the Candy slot. The red X and checkmark box are hidden and their old touch areas are inactive. The blue Back button remains usable.

| Target | Behavior |
| --- | --- |
| Candy | Native Rare Candy icon, scaled to 16×16 with its original used colors. Hidden unless Infinite Candy (Key Item 622) is in the bag. Applies its native effect once to a selected non-Egg below level 100. The item is retained. Native stat changes, messages, move learning and evolution continue normally. |
| Heal Team | Native Full Restore icon scaled within 16×16, beside Candy. Asks **Heal Party? A:Yes B:No**. A or tapping Yes confirms; B or tapping No cancels without changing the party. Confirmation restores HP and clears status for all non-fainted, non-Egg party members with the native potion recovery sound. Fainted members and Eggs stay unchanged. Needs no item and consumes none; EXP and PP stay unchanged. Finishes HP adjustment. |
| HP | Native italic label. Locks the selected member. Left/Right changes HP immediately, then repeats when held, bounded by zero and maximum. A teal underline marks adjustment mode. A, B, a second HP tap, Heal Team, XP or a status tap finishes adjustment. |
| XP | Italic uppercase beside HP, using the native label's P, height, shading and outline. Asks **Edge XP? A:YES B:NO**. A or tapping YES sets EXP to one below the native next-level requirement, then displays **{NICKNAME}'s xp was edged!**. A, B or a fresh bottom-bar tap dismisses that message. B or tapping NO at the question cancels. Level, stats and HP stay unchanged. Eggs and level-100 members are ignored. |
| SLP / PSN / BRN / FRZ / PAR | Native party status badges and colors. A matching status clears it; a different status replaces the existing condition. A status tap also finishes HP adjustment on that same tap. New conditions require positive HP; clearing an existing condition also works at zero HP. |

Press **L** during party selection to enter toolbar navigation. Left/Right moves through available icons with the native party-member navigation sound; **A** activates the selected action with its activation sound. A one-pixel teal box marks the selected icon, fading with the native party selector's 64-frame brightness cycle (about one second). The selected Pokémon remains locked. Hidden Candy is skipped. **B or L** returns to party selection. HP uses Left/Right for adjustment until finished; A returns to toolbar navigation when it was opened there. Touch remains available. A held touch activates once and cannot confirm healing or immediately dismiss an XP success message. Healing confirmation returns to the same toolbar selection when opened with L/A.

Leave **Hide toolbar when save flag is set (optional)** blank to keep the installed toolbar on by default. Decimal or hexadecimal saved flags 1–3059 are accepted. If a configured flag is set, the entire toolbar stays hidden and the native prompt, red X, checkmark box and party controls return. Apply the field through Install/Update/Enable Toolbar. Updates and ROM export/reopening preserve the chosen flag; clearing the field removes the condition. Installation and activation only read the flag and never change it.

The five optional section fields are **Candy**, **Heal**, **HP**, **EXP** and **Statuses**. A set flag hides that section completely; clearing it restores the section. Statuses controls all five badges together. Blank fields add no condition. Candy still requires its item. Sections may share a flag, and all sections may be hidden. Hidden icons have no touch targets and are skipped by controller navigation. The remaining icons retain their order and shift left together.

Section visibility refreshes while the toolbar is active. Hiding HP leaves adjustment mode; hiding a pending Heal/EXP action cancels its question without changing Pokémon data. Navigation advances to the next visible icon, or exits if none remain. Input on a layout-change frame is discarded so a shifting target cannot activate another action. Flags and neighboring save bits are never written.

The toolbar is available only in normal field Party selection. Battles, trading, item-target requests and other Party modes retain native behavior. Native command menus and applications hide the toolbar.

## Implementation

`build.py` checks call sites, callback words, native APIs, EXP, nickname and prompt-printing routines, selector animation, navigation sound, request/input/item/redraw code and matching graphics independently against both retail ROMs. Separate DLLs use a private `party-tools/ui.narc`. Recognized 0.1.0, 0.1.1, 0.1.2, 0.1.3, 0.1.4 and 0.1.5 installations update their owned files in place. The archive's enable state, optional hide flag and immutable graphics are checked when exported ROMs reopen.

Overlay 165 owns nine actors, nine character handles, three main-display palette banks (13–15), one cell/animation resource, nine persistent graphic buffers, a persistent 96-byte palette buffer and one VBlank upload task. Capacity checks precede allocation; partial failures release owned resources and preserve native behavior, including later native prompts after a failed initialization. Each full character upload is 4.5 KiB. Active native palette proxies must leave banks 13–15 available; native resource, heap and contiguous-block capacity are checked before allocation. The selector occupies unused sprite padding and changes only the previous/next icon's padding. Its reserved palette entries receive the native animated teal shade through scheduled palette uploads; idle pulsing does not reupload character data. HP/XP use spare Candy-bank colors, retaining the exact native rendered colors and a separate fixed teal HP underline. No per-frame allocations or display-register replacement are used.

The native prompt window remains allocated. The toolbar saves and clears its 23×4 BG0 tile-map rectangle through native RAM buffers and scheduled uploads before fade-in, and keeps it clear through fade-out. Actual messages and confirmation restore the frame. Prompt state is tracked separately from icon visibility so a hidden toolbar does not expose an empty white box. Visible order is Candy, Heal Team, HP, XP, SLP, PSN, BRN, FRZ, PAR. Visible centers are `(13 + 20×visible-index, 180)`; 20×24 touch regions follow those compacted positions before native controls. Native actor-position APIs move existing actors; layout changes do not rebuild resources. Action IDs remain stable, with Heal Team appended as action 8.

HP, status and EXP use native parameter accessors. Heal Team uses a separate confirmation state with an owned native string before native tile/HP/status reconstruction for each healed member. Opening or cancelling the question does not modify party data; the recovery sound plays only after acceptance. The field party-request constructor captures an EventWork reference from native GameData, tied to that exact request and cleared on native teardown. The native saved-flag reader gates initialization and prompt suppression; a missing or mismatched owner falls back to the original menu. Configuration member 0 uses version 3 (44 bytes), with enabled state, the whole-toolbar flag, five section flags in Candy/Heal/HP/EXP/Statuses order and a checksum over all flag values. Version 1 enable-only and version 2 whole-flag archives remain recognized for migration; old section settings default to blank. Updates, disable/enable and export/reopening preserve all six settings. XP resolves the next threshold through the native species/form growth table, and owns its confirmation string until the printer is reset. Candy owns the native medicine request using item 622, restores its original fields and owns evolution return handling. Installation stages PMC, the dependency's item/text/icon changes, the toolbar DLL and archive together. Failure leaves the previous project unchanged; installation never grants an item or edits a save.

## Verification

```sh
npm run partytools:build
npm run partytools:verify
npm run partytools:emulator -- /path/to/copied-white2.nds --scenario heal --power-on --companions --flag-configured
npm run partytools:emulator -- /path/to/copied-black2.nds --scenario sections --sections-hidden heal,exp,statuses
npm run partytools:emulator -- /path/to/copied-white2.nds --scenario sections --power-on --companions --sections-hidden candy,hp,statuses
npm run partytools:emulator -- /path/to/copied-black2.nds --scenario sections --sections-hidden candy,heal,hp,exp,statuses
npm run partytools:emulator -- /path/to/copied-black2.nds --scenario status --no-candy
PARTY_FRAMES=7300 npm run partytools:emulator -- /path/to/copied-white2.nds --scenario candy --companions
```

Local retail ROMs, the ARM toolchain and RPM converter are required by the builder. Test scripts use copied fixtures and separate output files. Original saves and ROMs are preserved. See [acceptance.md](acceptance.md) for observed coverage and limits.
