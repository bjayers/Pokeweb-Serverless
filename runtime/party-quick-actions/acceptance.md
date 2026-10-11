# Party toolbar acceptance — 0.1.6

Verified on 2026-10-11. CPU harness checks, app tests and emulator observations are separated below. Physical hardware and general hack compatibility are not certified.

## Passed checks

- Separate English revision-0 B2/W2 profiles verify native hooks/APIs, actor positioning, field-request ownership, saved-flag reading/bounds, next-level EXP, confirmation printing, palette proxies/uploads, selector animation and sound IDs. Native graphics match between games.
- Compiled ARM harness in both profiles: registers/stack; HP bounds/repeat/slot lock; status apply/clear and HP-mode exit; XP confirmation/cancellation/acknowledgement; L/A navigation and native selector fade; Candy request/evolution ownership; nine actors/character uploads, three palette banks, repeated cleanup and allocation/capacity failures.
- All **32 section visibility combinations** in each profile: five sections hide independently, Statuses hides all five badges, remaining actor centers are 13 + 20×visible index at Y=180, hidden actors stay invisible and unavailable, and controller navigation visits only visible icons. An all-hidden toolbar cannot enter toolbar navigation. Missing Candy closes its slot.
- Live section changes: hiding HP exits adjustment without changing HP; hiding a pending Heal or EXP question cancels it without modifying Pokémon data. Navigation chooses the next shown icon. Layout-change input is discarded. Flag bytes and adjacent bits remain unchanged. Missing flag-request ownership preserves the native menu.
- Healing confirmation retains exact “Heal Party? A:Yes B:No” text. Before acceptance/cancellation no party data changes; held input cannot accept twice. Acceptance restores conscious non-Egg members, preserves fainted/Egg/all-fainted members and out-of-party slots, leaves EXP/PP/item quantities unchanged, and plays native recovery sound 1391 once. Confirmation allocation failure and shutdown release owned strings.
- **55 focused app tests across two files**, including forty toolbar tests: defaults, whole/section flag validation, duplicate flag values, partial updates/explicit clearing, config integrity, atomic failure, disable/enable, export/reopening, and upgrades from 0.1.0–0.1.5. Legacy whole flags survive migration; new section settings default to blank. The exact two-line description and five optional fields are checked.
- TypeScript and the production Vite build pass. The existing bundle-size advisory remains.

## Current emulator observations

| Setup | Observed behavior |
| --- | --- |
| B2, C-Gear off, six members; Heal/EXP/Statuses flags set | Only Candy and HP remain, packed left. Touch at the shifted HP position enters adjustment and Left changes HP. L/Right/A skips hidden sections. Empty bar touches do not invoke hidden actions. HP changes survive closing/reopening Party. |
| W2, C-Gear on, six members, companions; Candy/HP/Statuses flags set | Only Heal and EXP remain, packed left. Their shifted touch targets open the proper confirmation; B cancels. L/A navigation skips hidden sections and returns to the same icon after cancellation. Empty bar touches do not activate hidden HP/status actions. Party closes/reopens normally. Companions include Battle Log, Enhanced Party Menu, Learnset Viewer, Summary enhancements, Porta PC and C-Gear Quick Actions. |
| B2, all five section flags set | No toolbar icons or navigation remain. L and touches at former icon positions do not activate custom actions. The blue Back button and Party close/reopen remain available. |
| B2, C-Gear off, no Infinite Candy, six hurt/statused members including one fainted | Candy is absent and all remaining icons pack left. Shifted HP/Heal touch targets work. Healing question/held-touch cancellation, touch Yes acceptance, L/A confirmation/cancellation and reopening pass; five conscious members recover while fainted Oshawott retains zero HP/status. Every member's EXP/PP stays unchanged. |

Earlier 0.1.4/0.1.5 emulator observations covered flag-gated native menus, the full nine-icon layout and native selector pulse, XP edging/acknowledgement, and Candy level-up/evolution/return with companions. Those observations remain historical; this release reruns their action/ownership/resource behavior in both compiled harnesses, while the current emulator rows above cover the new layout/flag behavior.

## Delivery and manual acceptance

The versioned delivery contains separate B2/W2 ROMs, copied initial saves, compacted-layout screenshots, comparison fixtures, controls and this checklist. Delivered DLLs match bundled assets; all thirteen archive members and the version-3 configuration are checked. Original ROMs, personal saves and earlier deliveries are preserved.

- [ ] In Pokeweb, configure any of the five hide flags and Install/Update Party Toolbar. Leave blanks for no flag condition.
- [ ] Set each flag in the game; verify its section disappears, Statuses removes all five badges, and remaining icons pack left. Clear flags and reenter Party to restore sections.
- [ ] Check shifted touch targets and L/arrows/A navigation, including missing Candy and all sections hidden.
- [ ] Test Heal/EXP confirmation/cancellation, HP locking, status toggles and native Candy/evolution flows.
- [ ] Disable/enable and export/reopen; verify every configured flag survives. Clear an individual field without changing other flags.
- [ ] Check actual save/reload, prolonged play and hardware audio; use game-specific unused flags for production designs.

## Limits

English revision-0 BW2 only; BW1, other languages and changed checked handlers are rejected. No upgrade build was gameplay-tested. Infinite Rare Candy's reserved-item compatibility rules still apply. Non-field Party requests retain native behavior, but the complete battle/trade/item-target matrix has not been gameplay-tested. All 32 masks and live flag changes are compiled-harness coverage; the current emulator runs cover representative partial/all-hidden layouts. Physical hardware, audible sound comparison, long sessions and actual save/reload still need manual coverage. Menu reopening is distinct from saving/reloading. Test scripts modify only copied fixtures; installation and activation never write visibility flags.
