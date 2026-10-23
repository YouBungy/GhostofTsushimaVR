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

## Guard trace round 6 (0.18.80, flat play): our own block and parry

Record with Win+Alt+R (clips of 3 minutes or less), `play_mode=flat`. Don't press block or parry during steps 1-3.
1. **Ctrl+Shift+B**, then let 3-4 ordinary blows land. Do you see the game's block spark and clang, and does the attacker recoil? Does
   Jin lose health?
2. **Ctrl+Shift+N**, then let 3-4 blows land: perfect parry. Does the attacker play its parried animation (slow motion, gold flash)?
3. **Ctrl+Shift+P**, then 3-4 blows: ordinary parry.
4. Without hotkeys: hold block against 2 blows, then parry 2 the normal way (to compare with the game's own).
5. Red and blue attackers: press **F5** at each red glint and **F6** at each blue one. The log compares each tag with the mod's verdict.

## Round 7 (0.18.87, flat play): katana sounds and render captures

About 15 minutes. Nothing needs recording except the optional part C.

### Before you start

The test keys are Insert, Delete and End, pressed on their own (no Ctrl or Shift), so they never crouch Jin or open the
stance menu.


1. In `GoTVR\gotvr.ini`, set `play_mode = flat`.
2. In the game's graphics menu, set anti-aliasing to **TAA**. The captures must show the same anti-aliasing VR uses.
3. Load a save outdoors with some enemies nearby but not yet fighting you. A quiet field is fine for parts A and B.

### Part A: teach the mod the katana sounds

The mod has no sound files of its own. It plays the game's own sounds, but it first has to find which of the game's thousands of
sound IDs is the sword whoosh, the draw and the sheathe. It does that by watching which sound plays every time you swing, draw or
sheathe, and not while you stand still.

1. **Light swings:** with the sword out, do 5 light attacks into the air (left mouse, or X on a pad), about one a second.
2. **Heavy swings:** then 5 heavy attacks (right mouse, or Y on a pad).
3. **Draws:** press **Delete**, then within a second draw the sword with the game's own key. Sheathe it again any way you like.
   Do this 3 times. The key tells the mod "a draw is coming".
4. **Sheathes:** with the sword out, press **End**, then within a second sheathe it with the game's own key. Do this 3 times.
5. **Baseline:** stand still, hands off, for 30 seconds. This tells the mod what the background sounds are, so it doesn't pick
   wind or birds by mistake.

Nothing changes on screen. When it works, the log has lines like `SOUND: the game's light swing sound is event 0x...`, and the ID
is saved to gotvr.ini (`sound_swing_event` and so on). VR then plays those sounds from your own sword.

### Part B: render captures

**What a capture is.** Each frame the game draws two images you never see, alongside the picture:
- **Motion vectors:** for every pixel, how far it moved since the last frame. The game's TAA uses them to line up the previous
  frame with this one before blending the two. The ghost hand in VR is TAA fetching the previous frame from the wrong place,
  which means wrong motion vectors on the arm.
- **Depth:** for every pixel, how far away it is. The headset needs this to warp a repeated frame correctly (the Spacewarp
  depth layer).

**Insert** copies both of those images out of the game half a second after you press it. It saves them as two files in
the `GoTVR` folder (`cap_0_velocity.raw` and `cap_0_depth.raw`, then `cap_1_...` and so on) and writes a summary to the log.
- It is not a screenshot. You see nothing on screen, maybe a tiny hitch.
- It does not need a video.

Two ways to press it:
- **Once:** captures half a second later. Use this when you want a picture of what is on screen now.
- **Twice quickly** (within half a second): waits for your next attack (left mouse or pad X) and captures 0.2 s after it, in
  the middle of the swing. Use this for the swing, because you can't press the hotkey and swing at the same moment.

Wait about 2 seconds between captures. The five captures, each one answers one question:

1. **Standing still.** Sword sheathed, don't touch the mouse or keyboard, press Insert once, keep still for a second.
   This is the reference: almost nothing should be moving.
2. **Mid-swing.** Sword out, camera still. Press Insert twice quickly, then do one light attack without moving the
   mouse. *Question: does the game give Jin's swinging sword and arm their own motion? This decides how the ghost gets fixed.*
3. **Camera pan.** Start turning the camera slowly and steadily with the mouse, press Insert once, keep turning for
   another second. *Question: how the motion numbers are scaled, from a movement we know.*
4. **Running sideways.** Run sideways (A or D) past a wall, a fence or trees, press Insert once while running. *Question:
   do near things move more than far things in the motion image, as they should?*
5. **Near and far.** Stand right in front of a wall, fence or rock (within 2 m) so it fills the middle of the screen, press
   Insert once. Then turn to face open landscape with sky, and press it once more. That makes 6 captures in total.
   *Question: which end of the depth range means near. The headset needs this the right way round.*

In the log, each capture shows as:
- `HOTKEY Insert: render capture in 0.5 s`;
- then `CAPTURE: velocity ... copied` and `CAPTURE: depth ... copied`;
- a moment later, `CAPTURE #n velocity: ... % of the image moves` with a 16 x 9 grid of numbers (how much of each part of the
  screen moved) and `CAPTURE #n depth: values ... .. ...`.
- If a line says `not read` instead, keep going and tell me which step.

The files are large (tens of MB each). You don't need to send them: tell me when you're done and I'll read the log and the files
from your game folder myself.

### Part C (optional, record this one with Win+Alt+R): glint check

Fight a group that has red and blue attackers. Press **F5** (or Mouse 4) when you see a red glint and **F6** (or Mouse 5) when you
see a blue one. The log checks each press against the mod's own verdict on that attack. They should agree.

### Round 7 results (0.18.87)

- Captures: all 6 taken. Motion vectors and depth decoded (docs/RENDER_PLAN.md, "Flat round 7 results").
- Sounds:
  - Heavy swing learned: 0x57a4ac64, tied with four other ids at 83 %.
  - Sheathe learned: 0xff2b4871, tied with four at 100 %.
  - Light swing not learned. The best id is at 28 %: each combo step plays a different whoosh.
  - Draw not learned. 20 stray Delete edges arrived at the start screen. From 0.18.88 tags count only with Jin loaded, and once
    per 2 s.

## After the session

Report what passed, fix what didn't, and update BUGLOG / COMBAT_FINDINGS / WRIST_HUD. No git push unless the owner says.
