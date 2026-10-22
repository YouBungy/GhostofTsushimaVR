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

## Guard trace: our own block / parry signal (0.18.73, round 2 in 0.18.75, flat play)

Goal: find where the game decides block vs parry vs hit, so the mod can send that decision itself instead of pressing the guard.

Setup: `play_mode=flat` in `GoTVR\gotvr.ini`. `guard_trace=1` and `guard_trace_dr=1` are written by default. Start the game
flat, sword drawn, and go to one or two plain swordsmen (no archers or brutes first). If the game stutters badly, quit, set
`guard_trace_dr=0`, and run again.

1. **Unguarded hits (3-4):** stand still and let blows land without blocking.
2. **Held blocks (4-6):** hold block (Q or LB) well before the blow and keep it held.
3. **Parries (4-6):** tap block just before each blow lands, so the game shows its parry flash. Perfect parries count too.
4. **Experiment (needs 1 + 2 first):** press Ctrl+Shift+B. For 20 s, do NOT block and let 2-3 blows land. The mod writes the
   record fields it learned from your guarded blows. If the game guards those blows by itself, that is our block signal.
5. Optional: a spearman, and a dog bite, guarded and unguarded.
6. **Red and blue glints (as many as you can, 5+ of each):** fight enemies that use them (a brute or spearman for red;
   swordsmen's blue). The moment you SEE a glint, press:
   - F5 or Mouse 4 (back side button) = RED
   - F6 or Mouse 5 (front side button) = BLUE

   Then deal with the attack as usual (dodge the red, parry the blue). Press the key as soon as you see the glint; it only
   needs to be roughly on time.

Round 2 (0.18.75) is the same session. The new lines are `GTR HITREC` (each enemy blow's record at the hit, with its chain),
`GTR STATE` (interaction state changes, e.g. a parry), `GTR EVENTS` and `GLINT EVENTS` (event-bus ids). The read watchpoints
are off. Most useful: several held blocks, several parries, some unguarded hits, and 5+ glints of each colour.

What the log shows (GTR lines):
- `GTR BLOW`: each enemy blow on Jin, with:
  - the record before and after the game's submit,
  - whether it was cancelled (parried) or guarded,
  - how long block had been held,
  - Jin's guard flags,
  - the call chain.
- `GTR SELF`: the parry reward. Its call chain leads to the parry decision.
- `GTR DIFF`: the record fields that tell guarded from unguarded.
- `GTR NEAR` and `GTR READERS`: the game instructions that read Jin's guard fields at the moment of a blow.
- `GLINT TAG`: per tag, what changed on that enemy in the second before your key, and its script events.
- `GLINT LEARN`: the enemy fields that change before (nearly) every red or blue glint but seldom otherwise. This is the early
  warning the VR mod lacks today.

Afterwards: decompile the decision functions from the chains and readers. Then write the block / parry signal into the
record, or call the decision code directly, from the VR blade.

## Guard trace round 3 (0.18.77, flat play): the game's own spark / clang, red and blue attacks

Setup: `play_mode=flat` in `GoTVR\gotvr.ini`. Record the screen with Win+Alt+R in clips of about 3 minutes or less. No F5/F6
needed; the video finds the glints.
1. **Block 2-3 blows** with block held well before them. The mod learns a real guarded blow and its tags.
2. **Ctrl+Shift+N**, then let 2-3 blows land WITHOUT blocking. The mod sends the full guard signal. Does the game spark /
   clang / play Jin's block reaction? (The video shows it.)
3. **Glint attackers:** fight brutes, spearmen and swordsmen that do red and blue attacks. Get as many glints as possible;
   dodge the reds, parry the blues as usual.
4. Send the clips. The log and `GoTVR\gtr_attacks.bin` are picked up from the game folder.

Analysis: tools/glint_video.py finds the glints and aligns the video to the log. Each glint is matched to the enemy's
wind-up dump in gtr_attacks.bin; red vs blue vs plain attacks are then compared field by field.

## Guard trace round 4 (0.18.78, flat play)

Same as round 3: record with Win+Alt+R (clips of 3 minutes or less), `play_mode=flat`.
1. Ctrl+Shift+N (fixed), then 2-3 blows without blocking. Does the game spark / clang / play Jin's block reaction?
2. Long glint fights: as many red AND blue attacks as possible, from several enemy types (swordsmen, spearmen, brutes, shield
   men). Dodge reds, parry blues. Two or three clips.

## Guard trace round 5 (0.18.79, flat play)

Record with Win+Alt+R as before (clips of 3 minutes or less). No hotkeys needed.
1. Hold block well before 3-4 ordinary blows (blocked).
2. Take 2-3 ordinary hits without blocking.
3. HOLD block against 2-3 BLUE attacks (they should break through).
4. PARRY 2-3 blue attacks (tap block just before they land).
5. A few RED attacks (dodge, or let one hit).

## After the session

Report what passed, fix what didn't, and update BUGLOG / COMBAT_FINDINGS / WRIST_HUD. No git push unless the owner says.
