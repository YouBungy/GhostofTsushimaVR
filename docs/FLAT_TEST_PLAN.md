# Flat test session (Claude drives the PC through computer use; the owner launches the game)

**Setup:**
- `play_mode=flat` (already at the top of gotvr.ini). Start the game and leave it on the main screen with the PC unlocked.
- Installed: xinput1_4.dll (0.18.47 + the W0 logging) and GoTVRHost.exe (logs the runtime's extensions on the next VR start).
- After each block below, read gotvr.log through the device bridge.

## Combat stage 1 (0.18.47)

1. **Template.** Near an enemy camp, land one plain light attack (LMB).
   - Expect `STRIKE: light hit template taken ...` with flags 0x03C00041 and the health change.
   - Also expect the +0x170 payload, the +50 / +168 objects, and list counts +1D0 / +1E8.
   - One heavy attack gives the heavy template.
2. **Registry.** Ctrl+Shift+J lists the enemies. They should be hostile, with health and positions. Check enemy z against
   Jin's z: is +0x150 the feet?
3. **Strike.** Ctrl+Shift+H near a hostile enemy.
   - `STRIKE hotkey (light) -> ... health a -> b` with b = a + the template damage.
   - The enemy reacts the way it does to a game blow (flinch, sound, blood); confirm with a screenshot.
   - No "the game kept the message" in the log.
4. **Kill.** Ctrl+Shift+K: a killing blow with the death reaction. Then a plain H on a low-health enemy should turn into a
   killing blow by itself.
5. **Safety:**
   - Strikes with no enemy near → refused.
   - After a load / fast travel → the registry is emptied by the game's weak handles; no crash.
   - Repeated strikes (20 in a row): stable, and the pool returns every message.
   - A dog (class 16) if one is near.
6. **Not testable flat:** the VR blade contact and how it feels. The decision core is self-tested; the owner tests in the headset.

## Wrist HUD (docs/WRIST_HUD.md)

1. **The HUD rectangle (W1).** A screenshot in combat, with the HUD option set to always show health / resolve if the game
   has one. Measure the bottom-left health + resolve cluster as fractions of the screen and save it as `wrist_hud_rect`.
   Check with the HUD in and out of combat, and at another resolution if possible.
2. **Resolve (W0).**
   - Heal with resolve (the heal key). Expect `VITALS: a resolve heal on Jin ... called from` / `Jin healed ... called from`
     with game rva frames. Decompile those frames to find the field the heal subtracts from.
   - Also tag a kill, a parry and a special art (resolve gain / spend) for the confirmation pass.
3. **The pass boundary for W1b (world space).** Census the passes after TAA / tonemap and before the UI composite in flat
   (the render-pass atlas). This picks the point where the HUD quad is drawn against the game's depth.

## After the session

Report what passed, fix what didn't, and update BUGLOG / COMBAT_FINDINGS / WRIST_HUD. No git push unless the owner says.
