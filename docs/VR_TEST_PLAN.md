# VR test plan: everything since the last headset session (0.18.82 → 0.18.89)

The last VR session (GoTTest1.mp4) ran 0.18.82, before the mod used the game's own block and parry. This list covers what changed
since then, most important first. Install `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (both changed). The gotvr.ini defaults are
right unless a step says otherwise. Record with the headset if you can, and send the log afterwards (I can read it from your game
folder).

## 1. Block and parry with the katana (0.18.83, the main new feature)

No buttons. Fight a few ordinary swordsmen first.
1. **Block:** hold the katana up in front of you, blade across, as an enemy attacks. Expect the game's own block: a spark, a clang,
   the attacker recoiling, and no damage.
2. **Parry:** stand ready, then move the blade into the enemy's weapon just before the blow lands. Expect the game's parry: the
   attacker staggers.
3. **Perfect parry:** the same, but as late as you can (the last ~0.15 s). Expect the game's perfect-parry effect and its reward.
4. **Blue attacks** (blue glint): holding the blade up should not stop them, but a parry should.
5. **Red attacks** (red glint): neither block nor parry stops them; dodge.
6. **Your own blows** still cut enemies, with no attack animations.

Tell me: which of 1-5 felt right, which didn't trigger, and any blow that was blocked when it shouldn't have been (or the reverse).

## 2. Sheathing (0.18.83)

Sheathe slowly, then quickly, with the blade coming in at an angle. The blade should pivot into the sheath smoothly, with no
sudden rotation when the tip reaches the mouth.

## 3. Katana sounds (0.18.83, 0.18.89)

- **Swings:** each swing plays one of the game's combo whooshes, chosen at random. Slow swings are quiet and fast ones are full
  volume. Listen for anything that isn't a whoosh (a grunt, a cloth rustle). If you hear one, tell me; flat PageDown / PageUp can
  find and drop it later.
- **Heavy swing:** charge and release. You should hear a heavier sound.
- **Sheathe:** the sound plays only while the blade moves, and pauses if you stop halfway.
- **Draw:** may be silent at first. The mod tries to learn it from the game while you draw in VR; it is saved to gotvr.ini as
  `sound_draw_event` once learned.

## 4. Swing step (0.18.84)

Swing at an enemy a few metres away while facing it. Jin steps toward it. The step should start and stop smoothly, not as a sudden
push.

## 5. The hand / sword ghost (0.18.88)

1. Draw the katana, keep your head still, and swing fast. Watch for the faint second hand or blade.
2. Press **F6** until the headset shows **8** squares and swing again; then **9**. Then back to **7** (keep pressing F6; it wraps
   around after 9).
3. In each mode, press **Insert twice**, then swing: that captures both eyes just after the swing.

Tell me which mode looks best and whether 8 or 9 removes the ghost. Also say if 8 or 9 costs frame rate (the log shows it too).

## 6. Pagoda / high places (0.18.82)

Climb something tall (a pagoda roof or a cliff). You should stay in first person.

## 7. The depth layer (0.18.89, optional, last)

This sends each eye's depth to the headset, so a repeated frame can be warped correctly for near and far objects.
1. Play a minute with `xr_depth_layer = 0` (the default) and Virtual Desktop's SSW on. Turn your head quickly past something close
   (a post, a tree trunk) with hills behind it, and remember how it looks.
2. Quit, set `xr_depth_layer = 1` in gotvr.ini, start again and repeat.
3. Tell me if close objects smear or wobble less, or if anything new looks wrong (black edges, flicker, a crash).

The log's `DEPTH/s` line shows whether the depth reached the headset ("host frames with depth"). If the game or the headset
misbehaves, set it back to 0.

## 8. DLSS (optional, only if you want to try it)

Set `vr_aa = dlss` and `dlss_quality = balanced` (or `performance` / `quality`), then restart the game. Look for streaks or
smearing compared with TAA. Set `vr_aa = taa` to go back.
