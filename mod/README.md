# GoTVR 0.18.46 — SYNCHRONIZED STEREO 6DoF (xr=4): both eyes from the same moment of the game

Single-player only. Never enter Legends (online) with any GoTVR file installed.
What changed and why: `docs/BUGLOG.md` (BUG-5, BUG-6).

- **Crash fix:** both 0.5 crashes happened inside Virtual Desktop's own code running in the game process
  (an NVIDIA shader-compile thread with a 64 KB stack overflowed; Virtual Desktop's compositor used freed
  memory). 0.6 runs the headset side in its own small program, `GoTVRHost.exe`, which the mod starts and
  stops with the game. The game process no longer loads any VR runtime.
- **Never an empty frame:** the headset always gets a picture (black until the game image arrives), which
  avoids a known Virtual Desktop crash path.
- **Game image fixed:** the mod now finds the real swapchain behind NVIDIA Streamline to copy from.
- **New `xr=2`: stereo 6DoF.** The game renders the left eye on one frame and the right eye on the next, each
  with your real eye position and the headset's exact lens frustum. This is the first-stage stereo
  (alternate-eye). True same-frame native stereo (both eyes every frame) is the next stage.

## 0.6.2 fix

xr=1/2 showed black because the mod mistook the game's occlusion-test Present for a second frame and
refused to send images ("FRAME GENERATION is on" in the log, although it was off). Fixed. 0.6.2 also logs an
"R1 census" every 5 s (or on F10): the list of views the engine renders each frame. That decides how true
same-frame stereo (both eyes every frame) gets built. Please press **F10** once in each of: main menu, standing
outdoors, riding, a cutscene, photo mode.

## 0.18.69: the movement-direction bug (corrected), and the HIT SCAN flat test

**Install:** `xinput1_4.dll` only.

- **Correction:** 0.18.68 blamed a horse dismount for the movement-direction bug; there was no horse. The log actually
  shows about 3 s with Jin in the air or sliding at 7.6-8.8 m/s (the jump window was held all that time), with the game
  camera pulled back to 4.2-4.6 m. The direction was wrong right after. A brief "riding" reading after landing was a
  false alarm.
  - Now, when a fall or slide of 1 s or more ends, the start-up turn kick runs (what a snap does to the game camera).
  - The faster direction guard from 0.18.68 stays: 45 deg off for 0.3 s at 1.5 m/s or more.
- **HIT SCAN (flat play):** for the game's own guard and parry. Every enemy blow on Jin saves Jin's 32 KB of data just
  before the game processes it, then logs what the game made of it: the composed flags, the damage, and whether block
  was held and for how long.
  - Guarded blows are compared with unguarded ones. The `HIT SCAN` lines list Jin's values that always differ between
    the two groups; that is what the game's guard actually reads at the moment of the hit.

**The flat test (`play_mode=flat`), one ordinary swordsman, heal whenever you need to:**
1. Let him hit you about 5 times without blocking.
2. Hold block (Q / LB) well before his swing and keep holding while he hits your guard, about 5 times.
3. Try for parries, pressing block just before his blow lands, as many as you like. A few perfect parries would be ideal.
4. Then set `play_mode` back to VR.

## 0.18.68: fixes from VR session 23

**Install:** `xinput1_4.dll` only (the host is unchanged from 0.15.6).

- **Damage vignette and the game's damage HUD on a block / parry:**
  - 0.18.67's hybrid route let the game take the blow first. The guard bit alone did not make the game guard (every
    blow was composed "not guarded"); the health was put back afterwards, but by then the damage effects had played.
  - `parry_engine` is back to `mod` (changed once): a defended blow never reaches Jin. `hybrid` stays as an option.
  - The game's guard needs more than that one bit. That is the next research step.
- **The parried enemy did not react and kept attacking:**
  - Every blow this session, the parry staggers included, fell back to the message path ("the record's pointers could
    not be found"). That path only does damage; it carries no hit reaction.
  - When the attack's moveset entry is missing, the saved record now uses its own copy of the attack definition.
  - The log says which step failed if it still does.
  - The parry stagger now uses only the record path. A parry never does damage-only, reaction-less hits.
- **No damage from a deflect:** the katana's blows wait `parry_strike_hold_ms` (200 ms). A blade touch on an enemy whose
  attack you block or parry in that time (or did in the last 0.6 s) is dropped; the log says so.
- **Snap turn:** the body turns with a snap or smooth turn in 0.06 s instead of the eased 0.35 s, so it lands with the
  view.
- **The game sheathing its sword (Jin stops moving):** the game's guard is tapped briefly every `sword_keep_drawn_ms`
  (3000, changed once) while you hold the katana, with enemies around or not. The log reports the taps and the game's
  own put-aways every 10 taps.
- **Movement direction after a dismount:** the log shows it went wrong right after leaving the horse. The start-up
  turn kick now also runs when riding ends, and the direction guard reacts sooner: 45 deg off for 0.3 s at 1.5 m/s
  or more.
- **The game's damage HUD:** the first time Jin is hit in a session, the HUD image is saved by itself
  (`ui_hit_color.bmp` / `ui_hit_alpha.bmp` in the GoTVR folder), so its overlay can be keyed out exactly.

## 0.18.67: the game's own block effects in VR (from your flat GUARD SCAN)

**Install:** `xinput1_4.dll` only. The host is unchanged from 0.15.6.

- **What the flat test found:** in all 12 of your block holds, one bit of Jin's data switched on while guarding and
  off on release: bit 0 of hero+0x14C (0x3000104 -> 0x3000105). It was the only value that did so every time. A few
  direction values (+0x1E0, +0x240) are filled only while guarding.
- **New default `parry_engine=hybrid` (changed once in your ini):**
  - Your katana's hitbox still decides block or parry, exactly as in 0.18.66.
  - The blow is then handed to the game with Jin's guard bit set for that one call, so the game handles it as a
    guarded hit, with its own spark, clash and recoil. The bit is cleared again immediately.
  - On a parry the attacker also staggers, as before.
  - If the game still takes health (an unblockable attack, or the bit alone not being enough), the health is put
    back, so the defence never fails.
  - The `PARRY:` lines show what the game made of each blow ("GUARDED by the game", or "not guarded" and "health put
    back"). `PARRY/total` counts both.
- `parry_engine=mod` = 0.18.66's silent defence; `parry_engine=game` = the old guard-button route; `parry_guard_offset`
  (0x14C) is where the flag lives.

## 0.18.66 / GoTVRHost 0.15.6: fixes from VR session 21

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe`. Both must be from this build, because the shared data block
is now v18.

- **Black square behind picked-up items, fixed with your UI capture:**
  - The capture showed the pickup panel writing fully opaque alpha over its whole rectangle while its colour there is
    black. On a monitor that does nothing; on the headset's separate HUD layer it is an opaque black box.
  - The HUD layer now goes through a small shader on the way to the headset. A fully opaque pixel darker than
    `hud_black_key_level` (0.20) becomes as see-through as it is dark, so black disappears and the dark glow around
    the icon fades like it does on the monitor. Everything else is unchanged.
  - `hud_black_key=0` returns to the plain copy.
- **The game's own hit effect is gone:** for 1.5 s after Jin is hit, the game's dark and red hit overlay is keyed out
  of the HUD layer. The red vignette replaces it (`hud_hide_game_hit=0` keeps the game's).
- **Parry = motion (`parry_mode=motion`, the new default; changed once):**
  - The blade must touch the blow, as before.
  - Then: if your blade moved at least `parry_min_travel_cm` (12) at up to `parry_min_speed` (1.2 m/s) or more within
    the parry window (`parry_timing_ms`, 220 ms before the blow lands), it is a PARRY. Held mostly still, it is a
    BLOCK.
  - The movement is measured against Jin's body, so walking with the sword held still is still a block.
  - The `PARRY:` lines log the distance and speed of every contact.
- **The sound is gone** (`fx_sound_volume` 0, changed once). The gold (parry) and silver (block) flashes stay.
- **The sheathe animation after sheathing:** 0.18.65 asked the game to sheathe too, which played its sheathe
  animation. That request is removed. Instead, the drawn stance's hip turn is taken out of Jin's body while he walks
  with the game's sword out (the walking hips' average line). This fixes the 30 deg turn without the game sheathing.
- **Wrist HUD glued to the forearm bone (host):** the game now sends Jin's actual left elbow and wrist positions to the
  headset program. The panel lies flat on top of that bone, `wrist_hud_from_wrist_m` (0.10) up from his wrist and
  `wrist_hud_out_m` (0.045) above the bone's line, and moves with your live controller. When that data is missing
  it falls back to the estimated elbow.
- **Walking on ropes / logs faces your headset:** the "riding" test (which stops the body follow) now also needs Jin
  to be sitting, i.e. a thigh near level. On a beam or rope the thighs hang down, so the body follows your head like
  normal walking. The log notes each riding on/off with the measurement.
- **UI prompts jittering:** the frame cap swung between 46 and 58 every few seconds, and that changing rhythm against
  the headset's 72 Hz made the HUD's prompts step unevenly. The cap now drops only after 2 bad seconds, by 2, never
  under 50, and climbs back after 6 good seconds.
- **The game's own block / parry effects in VR:** not yet. Jin's guard field still has to be found with the flat test
  below (GUARD SCAN).

## 0.18.65 / GoTVRHost 0.15.5: fixes from VR session 20

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe`. Both must be from this build, because they share a new data
block (v17). Your gotvr.ini gets a few one-time changes, listed below.

- **Parry hitbox:**
  - The katana's parry hitbox is now the whole sword: from 30 cm below the guard (`parry_hilt_extend_cm`, covering
    the hilt and your hands) to 12 cm past the tip (`parry_tip_extend_cm`).
  - The margin around it is 40 cm (`parry_blade_tolerance_cm`, changed once from 30).
  - It is recorded whenever the katana is in your hand. Before, it also needed the strike system to be ready.
- **Feedback you can feel, hear and see:**
  - Block: a short rumble, a low steel clang and a cool-white flash at the edges of the view.
  - Parry: a stronger rumble, a bright ringing clang and a gold flash. The attacker also flinches.
  - The clang is synthesized by the mod, so no game files are used. `fx_sound_volume` (0.7) sets its volume; 0 turns
    it off.
- **Damage vignette (host):**
  - When Jin loses health, the edges of the view flash red, stronger for a bigger hit, and fade over
    `fx_hit_fade_ms` (450).
  - Below 30 % health a dim red heartbeat pulses at the edges.
  - The centre of the view is never covered, like Half-Life: Alyx or Blade & Sorcery.
  - `fx_vignette=0` turns it off; `fx_hit_strength` and `fx_low_health` adjust it.
- **VR arms:** the game now takes Jin's arms only for climbing, takedowns and cutscenes.
  - Climbing means a hand above the head while Jin moves slowly. Grapple swings are fast, so they keep the VR arms
    (`arms_game_when_raised` 2; 1 = any raised hand, 0 = never).
  - Ropes and logs set off the "riding" check; that check no longer takes the arms (`arms_game_when_riding` 0).
- **The game's sheathe sound while holding the katana:**
  - When the game puts its own sword away, all of Jin's sounds are silenced for 1.6 s. They are moved far away for
    that moment.
  - The hidden background draw is silenced too.
- **Jin turned about 30 deg after sheathing:**
  - Cause: the game's drawn-sword stance. Its sword stayed out (in the scabbard only for you) until its own
    auto-sheathe.
  - Now, once your katana is home and your hand has left the hilt for 0.4 s, the game puts its sword away too. This
    is hidden and silent, so the stance ends right away.
- **F1 is not the HUD switch in VR:** F1 / F2 / F3 trim the hand angle, so session 20's F1 presses turned the hands
  from 20 to 45 deg. This build sets the hand pitch back to 20 once.
  - The HUD switch for that test is **Alt+F1**: it draws the HUD into the 3D image (the old panel).
  - For the black square, press **Ctrl+Shift+U** while it is on screen. That saves the HUD image as
    `ui_dump_color.bmp` and `ui_dump_alpha.bmp` in the GoTVR folder. Send me those and the fix can be exact.
- **Finding the game's guard (a flat test, for the game's own block / parry effects in VR):**
  - Set `play_mode=flat` and draw your sword near an enemy-free spot.
  - Hold block (Q, or LB on a pad) for about 1 s, let go for about 1 s, and repeat 10 or more times.
  - After each hold the log gets a `GUARD SCAN` line listing the parts of Jin's data that switch on while you guard
    and back off when you let go. With that field found, the VR parry can switch the game's own guard on for the blow
    your blade met.
- **Wrist HUD stays along the forearm (host):** the forearm line now runs from an estimated elbow (from your headset
  and a two-bone arm) to your hand. Only positions are used, so turning or bending the wrist never moves it
  (`wrist_hud_forearm=body`; `aim` / `grip` = the earlier versions; `wrist_hud_upper_arm_m` / `wrist_hud_forearm_m`
  0.30 / 0.29).

## 0.18.64 / GoTVRHost 0.15.4: fixes from VR session 19

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe`. Your gotvr.ini gets a few one-time changes (listed below).

- **Parry and block never worked:** the log showed three blows judged as parries, but the game never accepted its guard
  in time. Worse, while the mod held the blow back, the same swing hit Jin again 30-60 ms later, so he took it twice.
  - New default `parry_engine=mod`: the mod defends by itself. A blow that meets your blade is not applied to Jin
    (block). A parry also staggers the attacker, with a light blow at `parry_stagger_damage` (0.15) of its damage.
    No guard button, no game draw and no stance are involved. You feel a rumble: short for a block, longer for a
    parry.
  - One swing gets one defence: every further contact from the same attacker within `parry_swallow_ms` (450) is
    ignored.
  - Timing is the same as before: blade in the blow's path early = block, just in time = parry. `parry_mode=blade`
    still works with it.
  - `parry_engine=game` keeps the old route (the game's own guard, its sparks and slow-motion). It now waits 130 ms
    for the guard instead of 50 ms (changed once in your ini), and also ignores repeat contacts.
  - Not handled yet: the game's unblockable attacks are blocked too.
- **Sword stance turning Jin's body:** `sword_mask_while_held` is now 2 (changed once). While you hold the katana and
  Jin is standing, his pose from before the draw is kept, so the game's stance doesn't show. As soon as he moves, his
  walk and run animations play as normal.
- **Movement direction wrong until a snap turn:** the log showed Jin running about 120 deg away from where you pushed.
  The cause isn't found yet, so the mod now checks for it:
  - If Jin runs at least 60 deg off where the stick points, steadily for half a second, the stick is turned by the
    difference.
  - The correction is cleared at the next turn, since a snap turn fixed it before.
  - The log records each correction with the camera values, to help find the real cause.
- **Third person after respawning:** the death camera counted as a cutscene, and the respawn didn't end it. A load,
  a black screen, or a new Jin (respawn) now ends it, so first person comes back right away. The death camera itself
  still goes to third person.
- **Lag when grappling:** the host held 72 fps most of the time but dropped to 55-67 in heavy stretches while the
  game ran at its cap. The cap now adapts (`xr_game_fps_adaptive` 1):
  - When the headset loop falls more than 4 fps behind, the cap drops 3 pairs/s (never below 40).
  - After 3 s at full rate it climbs back, up to `xr_game_fps_cap`.
  - `PERF/5s` shows the current cap.
- **Wrist HUD beside the wrist (host 0.15.4):** the forearm line was taken from the controller's grip, whose axis runs
  through your fist, across the forearm. It now uses the controller's pointing direction, which runs along the
  forearm. The HUD sits 15 cm up the arm and 5 cm out on top (`wrist_hud_from_hand_m` / `wrist_hud_out_m`, changed
  once). `wrist_hud_forearm=grip` restores the old line.

## 0.18.63: fixes from a code review of 0.18.62 (no new features)

**Install:** `xinput1_4.dll` only (the host is unchanged from 0.15.3).

- **Possible crash fixed:** to save time, 0.18.61 remembered for 200 ms which game memory was safe to read. Game
  memory can be handed back to Windows at any moment, so a remembered answer could go stale. Now only the game's
  own program code is remembered that long; everything else is checked again after 50 ms.
- **Guard could put the sword away:** the game's draw button is a toggle. A blocked blow could send it a second
  time right after the mod had just drawn, and the game then sheathed again. Now it is sent only for a blow that
  actually met your blade, and never within 2.5 s of the last draw.
- **A parry right after a block counted as a block:** the guard was still held from the block, so the game never
  saw a new press. It is now let go for 50 ms first, and the blow waits that long too.
- **The keep-drawn tap could cut a block's guard short:** it now skips while a blow is held back or the guard is
  pressed, and a short press never shortens a longer one.
- **One-handed heavy charge:** moving your free left hand no longer restarts the charge.
- **Log:** a `PARRY/total` line (blocks, parries, misses, and how often the enemy's weapon was found in
  `parry_mode=blade`) appears whenever these change.

## 0.18.62 / GoTVRHost 0.15.3: fixes from VR session 18

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe`. Your gotvr.ini gets a few one-time changes (listed below).

- **Black screen when hit:** the 0.18.61 hit flash (the game's HUD over the whole view) went black in the headset.
  It is off by default now (`ui_hit_fullscreen_ms` 0, changed once in your ini). A red vignette can come later.
- **Parry / block never appeared:**
  - The blade history held only 76 ms, because it was written several times a frame. It now takes one sample per
    8 ms (about half a second).
  - Both blocks in the log "went through" because the game had put its own sword away by itself 0.1 s earlier.
    A guard now draws the game's sword first (hidden on Jin) and waits for it.
  - While you hold the katana and no enemy is within 8 m, the mod taps the game's guard briefly every
    `sword_keep_drawn_ms` (4000; 0 = off), with Jin's upper body masked for that moment. That should stop the
    game from auto-sheathing, which caused the sheathe sound and the sheath blink you saw.
  - **New test option `parry_mode=blade`:** the enemy's own weapon is the hitbox. Its sword is taken from its
    skeleton: the right hand, then `parry_enemy_blade_cm` (90) along the forearm. Your katana's hitbox is the
    blade, guard to tip, with `parry_blade_tolerance_cm` (30) around it. The timing rule is the same (early =
    block, just in time = parry). When the weapon can't be found (dogs, odd rigs), it falls back to the blow's
    line. The log says which one was used.
- **Heavy attack:** hold the trigger to charge, then just swing. You can keep holding the trigger.
  - The charge used to be lost once your hand drifted 10 cm while you waited. The limit is 25 cm now
    (`sword_heavy_charge_move_cm`, changed once).
  - The charged state lasts `sword_heavy_swing_window_ms` (300) into the swing.
  - One hand works too; `sword_heavy_two_hands=1` requires both.
- **Two-handed flicker:** the katana's weight spring became unstable with two hands, flipping the blade every frame
  (the log showed the tip at 1600 m/s and endless "HEAVY" swings). The spring is now solved implicitly, which stays
  stable at any frame time. A self-test covers hitches, two hands, and a target that flips.
- **VR arms on logs / ropes:** the arms are only handed to the game when a hand is above the head
  (`arms_game_above_head_cm` 0). After a grapple they stay with the game for `arms_game_hold_ms` (1200), so they no
  longer flick back in between.
- **Jin's body when walking backwards:** the 0.18.61 mask froze Jin's upper body while the katana was out, so his
  walk no longer turned with your headset. It is off by default now (`sword_mask_while_held` 0, changed once).
- **Judder when looking or moving in some directions, and in menus, with no frame drops:** in xr=4 the game could
  run faster than the headset's compositor could take frames. It then fought the compositor for the GPU, and Virtual
  Desktop got frames late. The game is now paced:
  - `xr_game_fps_cap` 58 stereo pairs/s;
  - `xr_menu_fps_cap` 60 in flat menus;
  - 0 = off.
  `PERF/5s` shows the waits.
- **Wrist HUD never appeared (host 0.15.3):**
  - It is on by default again (`wrist_hud_mode` 1, changed once).
  - It is easier to bring up: a glance within 70 deg of the forearm's face (`wrist_hud_face_deg`), the arm within
    40 deg of your view (`wrist_hud_view_deg`), up to 0.80 m away (`wrist_hud_max_m`).
  - The game itself hides its health art out of combat, so the wrist shows nothing then. If you want something
    there all the time, `wrist_hud_mode=3` adds our gauge.
  - The host logs every 10 s while it is shown.
- **Known, not fixed yet:** the black square behind the ration picture when picking an item up. Our best guess is
  that the game draws that picture so it adds onto the scene (black adds nothing on a flat screen). On the
  headset's separate HUD layer, though, its opaque black background shows. A fix needs a look at how that draw is
  set up. It is on the list for the next session.

## 0.18.61 / GoTVRHost 0.15.2: block and parry, frame-rate fix, controls and HUD changes from the first VR combat test

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe`.

- **First VR combat test (0.18.60):** katana blows made enemies flinch (game hit path with the saved record, x0.82..x1.10
  damage by swing speed). One blow fell back to the message path; it was the first blow after Jin went down, and the
  record is found again from Jin on the next blow.
- **Block and parry (new; there was none before):**
  - **Timing mode (default, `parry_mode=timing`):** when an enemy's blow is about to land on Jin, the mod checks whether
    your blade was in the blow's path (from the attacker to where it lands, `parry_blade_tolerance_cm` 30).
    - **Parry:** the blade arrived in the path at most `parry_timing_ms` (220) × `perfect_parry_window_mult` before the
      blow landed, i.e. you intercepted it in time.
    - **Block:** it was already there earlier (raised at the start of the attack and held).
    - No speed is needed. `parry_mode=velocity` keeps the speed-based version.
  - The game itself decides block / parry / perfect parry / guard break: the mod holds the enemy's blow back, presses
    the game's own guard (LB: a fresh press for a parry, held for a block), and replays the same blow a moment later
    (`parry_deflect_delay_ms` 50, `parry_block_delay_ms` 300). Rumble on a parry or block (`parry_rumble`).
  - `PARRY:` log lines show the contact, what was requested and what the game did with it (design doc "Show Contact
    and Result").
  - The enemy's weapon itself is not tracked; the blow's line stands in for it. If parries feel too strict or too
    loose, change `parry_blade_tolerance_cm`.
- **Frame rate with the katana out:** the log showed 55-60 game ticks/s with the katana sheathed and 21-37 with it in
  your hand. The rate was lower the more characters were around. Every per-frame check of an enemy asked Windows
  about its memory (VirtualQuery), which gets slow on the game's huge memory blocks. Those answers are now kept for
  200 ms per thread. `PERF/5s` lines show how many checks still reach Windows.
- **Left hand on the hilt (two hands):** the katana is drawn where Jin's right fist actually is, but the left hand aimed
  at the controller's katana from a tick earlier, so it slid off while you moved. The left hand now gets the same
  correction as the katana.
- **No game sheathe / draw / stance animation while you hold the katana** (`sword_mask_while_held=1`): Jin's upper
  body keeps his pose from before the draw the whole time the katana is out; the legs still walk. The game's
  auto-sheathe came anywhere from 16 to 30 s after drawing, so masking it at a learned time could not catch it.
- **Triggers (your new scheme; your gotvr.ini is changed once):**
  - RT = the game's own right trigger (interact / pick up supplies), plus standoff (D-pad up) when the game shows
    the prompt.
  - LT = Focus when nothing is held. The bow's slow-down will go on LT too, when archery comes.
  - Both are held off while the katana is out (RT charges the heavy).
- **Wheel button (X):** a quick press heals (D-pad down, `wheel_tap`); holding it past `wheel_hold_ms` (300) opens
  the wheel.
- **Heavy swing:** a big rumble on both hands for `sword_heavy_swing_rumble_ms` (220) when the charged swing is
  released, so you can feel it went.
- **Hit flash:** when Jin is hit, the game's HUD (its red hit effect) covers the whole view for `ui_hit_fullscreen_ms`
  (1500; 0 = off). A custom vignette can replace it later.
- **Death:** the fade to the death / revive screen no longer counts as a cutscene, so the revive prompt stays on the
  world-locked HUD panel.
- **Wrist HUD (host 0.15.2):**
  - Only the game's own health / resolve art is shown; our gauge is gone (`wrist_hud_mode` 1; your ini is changed
    once).
  - It sits on TOP of the left forearm (the outside, like your photo) and no longer rolls with the wrist
    (`wrist_hud_lock=top`; `hand` brings back the old behaviour). The kunai goes on the inner forearm.

## 0.18.60: records saved - VR is ready to test; dogs get their own records

**Install:** `xinput1_4.dll` only. Keep your gotvr.ini: it now holds the saved records.

- **Session 16 (flat): saved.** Every record found its pointers again, with its own copy of the attack definition
  (`SAVED for VR`). gotvr.ini now holds:
  - `cbt_rec_light`: attack 0x522baba1b8c7943c, flags 0x11C00841, **with the hit reaction**, 75 damage;
  - `cbt_rec_heavy`: attack 0xb9ea921bfd74396d, flags 0x03C00141, 225 damage, posture -150 (no reaction bit, but it
    breaks guard).
  - The next session, flat or VR, loads them at start (`REC: saved light record loaded`). Katana blows go through the
    game's own hit path with no real blow first.
- **Dogs:** your real hit on a dog was composed with the hit reaction too (0x11C01041, -85). Records are now kept per
  target type: people light / heavy, dog light / heavy (`cbt_rec_light_dog`, `cbt_rec_heavy_dog`). A dog is struck with
  a dog record when there is one, otherwise the people's. That dog hit came before this build, so no dog record is saved
  yet; the next real hit on a dog saves one.
- **Plainest blow kept:** some "light" blows came out at 150, 170 or 225 damage (stance / combo bonuses). Within the same
  reaction class the library now keeps the lowest-damage blow, so the katana's base damage stays the ordinary 75. Your
  swing speed and grip scale from there.
- **VR test:**
  1. Draw the katana and cut an enemy.
  2. The log should show `STRIKE katana (game hit path, light record 0x522baba1b8c7943c with the hit reaction ...)`, and
     the enemy should flinch.
  3. Try two hands with the right trigger held still until it charges, then cut (heavy).
  4. Notice the one-handed weight (`sword_weight`).

## 0.18.59: the flinching blow saved for VR

**Install:** `xinput1_4.dll` only.

- **Session 15 (flat, about 49 blows):**
  - The library saw 7 different light attacks. Only one composed the hit reaction, attack 0x522baba1b8c7943c (flags
    0x11C00841); it was kept, as intended. The same attack can come out with or without the reaction depending on the
    combo, which is why the library keeps a reacting record once it has one.
  - It still could not be saved. Jin's attack component and the second object were found again, but the attack
    definition the record points at is not reachable from Jin even three pointer steps away. It lives in its own asset
    block (definitions 0xB0 bytes apart, e.g. 0x17135df0e00 / 0eb0 / 0f60).
- **Fix:** the game only ever reads three values from the definition (its hash, one flag word and the move number). So a
  saved record now carries its own copy of the definition's 0xB0 bytes. Everything else is found again from Jin.
  - The log line becomes `REC: ... (not found - its 0xB0 bytes are carried instead) - SAVED for VR`.
- **Saved records load when the game starts.** A reacting record from an earlier session is never replaced by a plainer
  blow; only another reacting blow replaces it.
- **VR:** once `cbt_rec_light` is in gotvr.ini, every katana blow in VR runs the game's own hit path with that record,
  with your blade's contact point and swing direction, scaled by your swing speed. No game attack and no animation.
  Without a saved record VR blows still do damage (message path), just without the flinch.
- **One more short flat session is needed to write the record:** land combos until a `REC: light record ... WITH the hit
  reaction` line appears, followed by `SAVED for VR`.

## 0.18.58: VR combat on the game's own hit path; katana weight

**Install:** `xinput1_4.dll` only. Pushed before this build as "(WIP) VR combat" (0.18.57).

- **Session 14 (flat):** Ctrl+Shift+H flinched and Ctrl+Shift+M did damage only, with no crash. That settles the
  method: **the katana's blows in VR go through the game's own hit path, from a record of one of Jin's reacting blows**.
  The message path is the fallback (damage, no flinch).
- **In VR:**
  - The game's own attacks are held off while the katana is out, so a VR session cannot make a fresh record. It has to
    load the saved one (`cbt_rec_light` / `cbt_rec_heavy` in gotvr.ini).
  - Saving still failed in session 14: the component was found, but the attack definition was not within one step of
    the moveset entry. 0.18.58 now follows up to three pointer steps from the moveset entry and from the per-attack entry.
    It records the path and the definition's hash, and checks that hash when the record is rebuilt.
  - Look for `REC: ... SAVED for VR` after a flat session with a few combos. Until a record is saved, VR blows fall back
    to the message path.
  - Each katana blow now carries **where the blade met the enemy and which way it was moving** (the record's contact
    point, velocity and direction). The game's reaction responds to your actual cut, not to the swing it was copied
    from. Hotkey blows keep the copied swing.
  - Damage is still scaled by swing speed and the two-hand multiplier on the record's base damage. Heavy (two hands +
    charged) uses the heavy record.
- **Katana weight (one-handed), after Blade & Sorcery and the playbook:**
  - **Spring-damper:** B&S joins the weapon to the hand with a spring (pulls it back to the hand) and a damper (gives it
    inertia: it builds speed slowly and is hard to stop). The katana's direction now follows the hand the same way: a
    rotational spring-damper with damping 0.85, so a hint of follow-through when you stop a swing.
  - **Gravity sag:** held level in one hand, the tip droops a little (`sword_weight_sag_deg`, 3° per unit of weight).
  - **Two hands = a second joint:** the lag drops to a third (`sword_weight_two_hand` 0.33) and the sag to a fifth.
  - **Soft limit (playbook ch02):** the blade trails the hand 1:1 up to 16°, and beyond that is compressed toward 28°.
    It never stops dead or springs back.
  - **Reset list (playbook ch02, FEAR VR):** the filter resets, with a log line, on a jump of the hilt (teleport,
    recenter, body snap), a snap or smooth turn, the flat screen (menus, cutscenes), or a bad frame.
  - **`sword_weight` = 1.0 by default:** 45 ms period one-handed, 15 ms two-handed, 3° sag: noticeable but small. Your
    old 0 (off) was moved to 1.0 once. 0 turns it off; 0.5 lighter, 2 heavier. Blows are judged on the blade where you
    see it (after the weight).

## 0.18.57: Ctrl+Shift+M crash fixed; one more step toward saving the flinching blow for VR

**Install:** `xinput1_4.dll` only.

- **Session 13:** Ctrl+Shift+H flinched. Ctrl+Shift+M (the message path) froze and crashed the game: the log stops
  inside that blow. 0.18.56 had let the message path copy a blow carrying the game's hit-reaction bit (0x11C01041).
  The bit asks the game to react, but the message path skips the game's own composition of the blow, so the data a
  reaction needs is missing.
  - The message path now never carries that bit. A template with it is dropped from gotvr.ini at load, and only plain
    blows are kept. The message path is therefore damage only (no flinch), and stays the fallback.
  - **The flinch comes from the record path (Ctrl+Shift+H / the katana).**
- **Saving the flinching record:** Jin's attack component was found this time (two steps from Jin, hero+0x408 → +0x230),
  and so was the +0x68 object. Still missing is the attack definition the record points at: it is not in the moveset
  entry. 0.18.57 also searches the component's per-attack array and one more step from both. If that fails again, the
  log dumps both entries (`REC moveset entry`, `REC per-attack entry`) so the next build can follow it.
- Records no longer need a message-path template to be usable.

## 0.18.56: making the flinch survive a restart (and VR)

**Install:** `xinput1_4.dll` only.

- **Session 12 (flat, two runs):**
  - **Run 1:** the library kept a reacting light record (attack 0x74bf08798839b183, flags 0x11C00841). Ctrl+Shift+H
    flinched. Ctrl+Shift+K kills **by design**: K is the "kill the nearest enemy" test key; H is the normal blow.
  - **Run 2 (after the restart): no flinch.** The record had not been saved. Jin's attack component is not stored in
    Jin's object directly, so the mod could not note where to find it again. The first blows went by the message path
    (damage only), and then your first real blow, the opener of a combo with no hit-reaction bit, became the record.
  - The one H blow that killed a full-health enemy (300 → 0) was the game's own decision: it composed a lethal blow
    itself, most likely because that enemy was not yet aware of Jin (the game's one-hit kill on unaware enemies).
  - In your own combos the game flags the opener without the reaction and the second blow with it. That is why the
    library prefers reacting records.
- **Fixes:**
  - The component is now searched one step deeper from Jin.
  - A rebuilt record carries its own copy of the attack definition, found in Jin's moveset by the attack's hash. The
    game only ever reads that one pointer.
  - When it still cannot be found, the log lists where the pieces live (`REC near` lines) for the next build.
  - The message path's templates (always saved, and used whenever no record is available) now also prefer a blow with
    the hit-reaction bit.
- **Ctrl+Shift+M:** one blow by the message path, to compare. If that flinches too, VR has a fallback that needs nothing
  from the session at all.

## 0.18.55: the combat method - the game's own hit path from a library of Jin's real blows (no animation)

**Install:** `xinput1_4.dll` only.

- **Session 11 (flat):** Ctrl+Shift+H made enemies flinch with no animation at all. The difference from session 10 is the
  blow the record came from:
  - session 10 copied Jin's attack 0x74bf...: the game composed flags 0x03C00041 and enemies never reacted;
  - session 11 copied attack 0x9561...: flags 0x11C01041. Bit 0x10000000 is the hit reaction. Enemies flinched.
  - So the reaction comes with the attack the record belongs to, not with Jin's animation. The swing test (Ctrl+Shift+G)
    confirmed that a playing animation is not needed.
- **The insta-kills** were a bad record, not the swing test. After you died, a real blow of Jin's that killed outright
  (flags 0x03C00001, lethal damage, probably a finisher on a staggered enemy) became the copied record. Every blow after
  that, from H and G alike, copied a kill.
- **The method from now on (cbt_mode 2):**
  - The mod keeps a **record library**. A real blow of Jin's is kept only after the game has composed it, and only if
    it was an ordinary damaging blow: no finishers, kills, heals, revives or spawns.
  - Light and heavy blows each get their own record. A blow carrying the hit reaction always wins over one without it.
  - The katana's speed, two-handed grip and heavy fallback scale the record's own base damage. A kill (Ctrl+Shift+K)
    is a lethal blow composed by the game itself.
  - **Saved for VR:** each record is written to gotvr.ini (`cbt_rec_light`, `cbt_rec_heavy`), together with where its
    pointers live on Jin. A later session, including VR, rebuilds it from Jin without needing a real blow. The log line
    `REC: pointers found again from Jin ... can be saved for VR` says whether that worked.
  - The animated method stays available as `cbt_assist=1`, for comparison only.
- **What to do once in flat:** fight normally for a minute with the game's own light and heavy attacks, so the library
  gets a reacting light and heavy record. Then use Ctrl+Shift+H / K (no swing) to check the flinch.

## 0.18.54: no attack animations - finding the flinch from the data; the old method kept as a setting; resolve maximum from the game

**Install:** `xinput1_4.dll` only.

- **Session 10 (flat, 0.18.53):** the katana's blows ran the game's entire hit path. The first part of the message, the
  enemy's changes and the enemy's events were identical to a real blow; damage and kills landed; no flinch.
- **The mod never presses an attack button** unless you turn on `cbt_assist=1` (owner: no attack animations in VR for
  free-flowing combat).
  - `cbt_assist=1` keeps the old idea **only to compare**: every blow taps the game's attack button (light X / heavy Y,
    `cbt_assist_press_ms` 90) and lands `cbt_assist_delay_ms` (200) later while Jin's attack animation plays; Jin's own
    blows in that window are held back. Default 0.
  - (A first 0.18.54 build waited for an "attacks in progress" list; that list turned out to be Jin's whole moveset, so
    that build was replaced before testing.)
- **Finding the flinch without animation (flat diagnostics):**
  - `MSGDIFF`: the whole 0x210-byte message as delivered, real and ours, plus every field of ours that differs from Jin's
    latest real blow. The game composes the back half of the message while reading Jin's animation state; only the first
    0xB0 bytes were compared until now. Whatever differs there can be copied in, with no animation.
  - **Ctrl+Shift+G (swing test):** arms the test; your next attack press (left mouse / pad X) is followed 180 ms later
    (`cbt_swing_test_ms`) by the katana's blow on the nearest enemy, while your own swing plays, with your own blow held
    back. If enemies flinch here and not with Ctrl+Shift+H, Jin's playing attack matters, and the MSGDIFF lines show what
    it changes.
- **Resolve maximum:** read from the game's own progression value, `hero+0x4644 + 3`, the formula every game function
  that refills, clamps or shows resolve uses, so it grows with Jin's upgrades. `wrist_resolve_max` is only a fallback.
  `VITALS` lines (also in flat play) log resolve and its maximum when either changes.
- The resolve finder is off (`cbt_resolve_scan=0`); its snapshots cost frame time and its job is done.

## 0.18.53: the katana's blows go through the game's whole hit path (the flinch fix, take 3); resolve on the wrist

**Install:** `xinput1_4.dll` only (GoTVRHost 0.15.1 is unchanged).

- **Session 9 (flat, 0.18.52)** showed where the flinch comes from:
  - The enemy receives the same two events (0x16, 0x17) from a real blow and from ours.
  - A real blow first runs the game's hit processing (HitProc 0x8504D0). That composes the message and calls Jin's attack
    components: events 0x11 / 0x12 / 0x14 / 0x15 on his weapon, and a mark in the target's hit ring (+0x9C8). The mod
    skipped this step, so the enemy took the damage without the "I was struck by an attack" part.
- **The fix (`cbt_mode 2`, the default):** the first real blow Jin lands in a session is kept as a template (its hit
  record). Every katana blow then runs the game's own HitProc on a copy of that record, with the target swapped to the
  enemy you hit and the contact point moved to it. Damage, the reaction, sound and blood all come from the game.
  - Until Jin has landed one real blow in the session, blows use the 0.18.52 message path (damage only, no flinch). The
    log says `hit record taken from Jin's own blow` when the template is ready.
  - `cbt_mode 1` brings back the 0.18.52 behaviour if anything misbehaves.
- **Resolve on the wrist gauge.** The resolve finder pointed at one value: hero+0x43A4, a float counted in pips (3 → 3.65
  at a kill, 4.885 → 6.185 at a parry, falling at a heal). The gauge now shows it as pips.
  - Its maximum is not known yet; `wrist_resolve_max` (default 10) sets how many pip slots to draw.
  - Every 5 s the log writes a `VITALS` line with health, resolve and the fields around it, to find the maximum.
- The `EVENT` diagnostic now also logs the events of the mod's own blows (the logging window opened too late before).

## 0.18.52 / GoTVRHost 0.15.1: the wrist gauge (always there); deeper flinch diagnostics; the resolve finder reports candidates

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe`.

- **The wrist HUD shows whenever you look at your forearm,** in and out of combat.
  - The game fades its own health bar out of combat, so the cropped game art was often empty.
  - A new gauge is painted from the game's own values: an ivory health bar (like the game's) with the health just lost
    flashing red. Below 30 % it turns red and the plate pulses with the heartbeat.
  - Resolve pips appear above the bar as soon as the resolve finder has found it.
  - `wrist_hud_mode`:
    - 1 = the game's own art only;
    - 2 = our gauge only;
    - 3 (default) = our gauge, with the game's own health / resolve art above it whenever the game shows it.
- **Session 8 (flinch):**
  - The game's whole delivery does exactly the same to the enemy for a real blow and for the mod's, so the flinch is
    decided before the delivery, on Jin's attack side.
  - New flat diagnostics: the game's own hit processing (`HITPROC`: the attack components' callbacks, the hit record,
    the caller chain), and every script event a blow fires at a person, real vs ours (`EVENT` lines).
- **Resolve finder:** instead of long lists it keeps per-field statistics: what rose at gains (kills, parries, staggers,
  Ctrl+Shift+R) and fell at spends (resolve heals, revives). After every event it logs the best `RESOLVE?` candidates.

## 0.18.50: closer to Jin's own blow; tools to find the flinch and resolve; ready for the VR test

**Install:** `xinput1_4.dll` (the host is 0.15.0 from 0.18.48).

- **Session 7:** still no flinch through the game's delivery. The mod's message differed from Jin's own in three places:
  - +0x168 (the attacker, always Jin) was empty and its flag cleared;
  - +0x1A0 (the combo step, 1 / 2 on real blows) was 0;
  - +0x50 (a class-24 object, probably the weapon) was empty, because the light template came from gotvr.ini.

  All three are filled now; +0x50 comes from Jin's latest real blow of any kind.
- **If enemies still don't flinch,** two tools find out why (flat play):
  - **Ctrl+Shift+W** next to an enemy, then hit it with the game's own attack. Hardware watchpoints catch every write to
    that enemy's reaction fields and log the writing code with its call chain (`DBW` lines). That is the code the mod has
    to reach. Press Ctrl+Shift+W again, or wait 90 s, to stop.
  - `DLVDIFF real / ours`: what the game's whole delivery changes on the enemy, for a real blow and for the mod's.
- **Resolve finder:** fields that are busy all the time (animation, position, timers) are now left out. Only what changes
  at the moment shows: resolve heals, revives, Jin's kills, and **Ctrl+Shift+R** (a mark: press it right after resolve
  changes, e.g. after a parry).

## 0.18.49: the katana's blows delivered the game's way (the flinch fix); finding resolve

**Install:** `xinput1_4.dll` (the host is the same as 0.18.48's).

- **Why there was no flinch:** session 6 showed the game's HandleHit does exactly the same thing for a real blow and for
  the mod's (`HHDIFF real` = `HHDIFF ours`). The flinch, sound and blood come from the delivery around it.
  - The game's delivery (0x85A770, through 0x85A4F0) marks the hit delivered, keeps it among the latest 64 hits, fires a
    "hit received" event on the enemy and a "hit dealt" event on Jin, and only then calls HandleHit.
  - The mod now delivers its blows through that same path, exactly as the game sends its own messages (the resolve heal
    is sent the same way). `cbt_deliver = 0` goes back to the 0.18.48 direct call.
- **Finding resolve (flat play):** Jin's state is snapshotted every 250 ms. When you heal with resolve, revive, or kill an
  enemy, the log lists what changed (`RSCAN` lines). The field that drops at every heal / revive and rises at kills is
  resolve.

**Flat test:**
1. Land a light and a heavy attack.
2. Ctrl+Shift+H / K: do the enemies flinch, bleed and die with the death reaction now?
3. Heal twice with resolve, kill two or three enemies, and revive once if you get knocked down.

## 0.18.48: combat stage 1b (the design doc's attack parameters) and the wrist HUD (W1)

**Install:** `xinput1_4.dll` AND `GoTVR\GoTVRHost.exe` (the shared block changed to v16, so the two must match).

**Combat**
- **Enemies are whoever the game counts as hostile.** The mod asks the game's own attitude query (the one its threat system
  uses), so every Mongol in a camp can be cut from the first swing. Allies and villagers are never struck.
  - Session 5 refused most strikes because an enemy only counted once it had traded a hit with Jin.
  - Ctrl+Shift+J now also lists each character's attitude toward Jin, and a hotkey refusal is always logged.
- **Heavy attacks:** with both hands on the katana, hold the right trigger and keep your hands still for
  `sword_heavy_charge_ms` (moving more than `sword_heavy_charge_move_cm` starts the charge over).
  - A rising hum, then a thump in both hands, tells you it's charged.
  - The next blow is a heavy one (the game's heavy hit, copied from one heavy attack of Jin's; until then a light blow ×1.6).
  - The charge stays while the trigger is held and is gone 0.25 s after you let go.
- **Lunge:** a blow moves Jin toward the enemy struck.
  - Light `sword_lunge_cm`, heavy `sword_lunge_heavy_cm`, longer the faster the swing (`sword_lunge_speed_mod`).
  - Never within `sword_lunge_cooldown_ms`, and it stops `sword_lunge_stop_cm` short of the enemy.
  - The playbook's references warn that moving the camera can be uncomfortable. `sword_lunge = 0` turns it off.
- **Swing rumble:** a recognised swing ticks the right hand (`sword_swing_rumble`); a charged heavy swing thumps both
  hands (`sword_swing_rumble_heavy`).
- **Katana weight** (`sword_weight`, 0 = rigid, try 1): the blade lags your hand a little one-handed, a third as much
  two-handed.
- **Diagnostics for the missing hit reaction**, flat play only. The first 6 real blows and the first 6 injected ones log
  `HHDIFF` (what HandleHit changed on the enemy), `MSG` (the whole message) and the real blow's caller chain.
  - `cbt_msg_state = 1` (default) sends the message in the state real ones arrive in. That may already bring the flinch back.

**The design doc's attack parameters → gotvr.ini:**

| Design doc | Key | Default |
|---|---|---|
| Minimum Speed | `sword_hit_min_speed` | 2.2 m/s (closing speed at the contact) |
| Minimum heavy attack charging time | `sword_heavy_charge_ms` | 450 |
| Maximum hand movement during heavy attack charge | `sword_heavy_charge_move_cm` | 10 |
| Minimum Travel Distance | `sword_hit_min_travel_cm` | 18 |
| Travel Distance Modifier | `sword_lunge_speed_mod` | 0.5 (the lunge grows up to 1.5× at 8 m/s) |
| Player travel distance | `sword_lunge_cm` | 35 |
| Player travel distance Heavy | `sword_lunge_heavy_cm` | 60 |
| Player travel distance cooldown | `sword_lunge_cooldown_ms` | 600 |
| Attack Cooldown | `sword_hit_cooldown_ms` | 220 |
| Rearm Time | `sword_hit_rearm_ms` | 120 |
| Contact Tolerance | `sword_hit_contact_cm` | 6 |
| Rumble feedback on hitting enemies | `sword_hit_rumble` | 0.9 |
| Rumble feedback light attack | `sword_swing_rumble` | 0.35 |
| Rumble feedback heavy attack | `sword_swing_rumble_heavy` | 0.7 |
| Rumble feedback low health | `wrist_hud_heartbeat_gain` | 0.6 |
| Katana weight | `sword_weight` | 0 (off) |
| Parry / perfect parry window multipliers, parry parameters | stage 2 (block / parry) | |

Also: `sword_lunge_stick_speed` 4.5 (m/s at full stick, which turns a lunge distance into a stick push), `sword_lunge_stop_cm` 70,
`sword_two_hand_damage` 1.0, `sword_swing_rearm_speed` 0.8.

**The wrist HUD, W1** (docs/WRIST_HUD.md):
- The game's own health / resolve corner of the HUD is shown on a small panel on the **outside of your left forearm**.
  - It appears when you look at it, stays on below 30 % health and for 2 s after a hit, heal or resolve change, and is cut
    out of the room HUD panel so it never shows twice.
  - Hold your left hand up at your head and press its trigger for the whole HUD, objectives included (control scheme).
  - With no tracked left hand, everything stays as before.
- **Settings:**
  - `wrist_hud` 1
  - `wrist_hud_rect` 0.015,0.865,0.235,0.125: the HUD corner as fractions of the UI, measured on your flat HUD
  - `wrist_hud_width_m` 0.10
  - `wrist_hud_from_hand_m` 0.11
  - `wrist_hud_out_m` 0.045
  - `wrist_hud_roll_deg` 0
  - `wrist_hud_glance` 1
  - `wrist_hud_cut` 1
  - `wrist_hud_panel_at_head_only` 0: 1 shows the room HUD panel only with the hand at the head
  - `wrist_hud_low_hp` 0.30
- **Low-health heartbeat** in the left hand: 60 → 130 bpm below 30 % health, lub-dub, only within 20 s of the last hit
  (`wrist_hud_heartbeat`, `wrist_hud_heartbeat_gain`, `wrist_hud_heartbeat_mute_s`).
- **Not yet:**
  - A floating panel still draws over the world. The world-space version, hidden behind your hand, the katana and walls,
    is W1b, next.
  - Resolve as numbers (W0) isn't done yet; the game's own resolve pips are shown.

## 0.18.47: combat stage 1 - the katana cuts

**Install:** `xinput1_4.dll` (the host is unchanged).

- **A blow when your blade meets an enemy fast enough.** Only the cutting edge counts (the last `sword_hit_blade_cm` of the
  blade, never the hilt). The speed that counts is the **closing speed** at the contact point: blade against enemy, with your
  own walking taken off (the playbook rule). Walking into an enemy with the blade held out does nothing; an enemy running onto
  your held blade gets cut.
- **Fast swings don't slip through.** The tip's path since the last frame is tested too, so a fast swing still connects.
  Each enemy can be hit once per pass, and only again after the blade has left them (`sword_hit_rearm_ms`). There is also a
  cooldown between blows (`sword_hit_cooldown_ms`).
- **The blow is the game's own.** The mod sends the same hit message the game sends for Jin's ordinary light attack (damage,
  posture, reaction, sound, blood), copied from a real blow of Jin's. Damage scales from **x0.75 at the minimum speed to x1.10
  at 8 m/s** (the Bloodborne VR curve). A blow that would kill is sent the way the game sends Jin's killing blows.
- **Only proven enemies can be hit:** anyone who has hit Jin, or whom Jin has hit. Villagers and allies are never struck
  (`sword_hit_any = 1` lifts this).
- **Rumble:** strong on a blow (both hands when you hold the katana two-handed), a light tap when the blade touches an enemy
  too slowly to cut.
- **One-time setup:** the game's hit message is copied from a real blow. Land one ordinary light attack with the game's own
  attack, flat or in VR before you draw. The copy is saved in gotvr.ini (`cbt_tpl_light`), so after that it works from the start.
- **Flat test keys:** **Ctrl+Shift+H** strikes the nearest hostile enemy, **Ctrl+Shift+K** kills it, **Ctrl+Shift+J** lists the
  enemies the katana can hit. The log shows `STRIKE` lines and a `HITS/s` line (contacts, too slow / too short, blows, peak
  closing speed).
- **Settings (gotvr.ini):**
  - `sword_hit` 1
  - `sword_hit_min_speed` 2.2 (m/s closing)
  - `sword_hit_min_travel_cm` 18
  - `sword_hit_contact_cm` 6
  - `sword_hit_cooldown_ms` 220
  - `sword_hit_rearm_ms` 120
  - `sword_hit_blade_cm` 70
  - `sword_hit_damage_slow` 0.75
  - `sword_hit_damage_fast` 1.10
  - `sword_hit_fast_speed` 8
  - `sword_hit_rumble` 0.9
  - `sword_hit_rumble_ms` 70
  - `sword_hit_any` 0
- **Not yet:**
  - Enemies can't guard against these blows yet; the game normally sends a "guarded" variant.
  - No heavy attacks (trigger charge), lunge, block / parry or hit-stop yet. Those are stage 2.

## 0.18.46: session 3 decoded; the enemy watcher follows real hits only

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Parry and perfect parry** give the same result message (Jin +12.5 health). The game sorts them by how close to the impact block
  was pressed (22 ms before = perfect, 64 ms = normal). The VR parry presses block at blade contact and lets the game decide.
- **Standoff state:** hero+0x1070 = 1 (1.5 s after X). Traversal: the beam / rope / ledge plane at hero+0xB60 changes as Jin steps
  on. Details in docs/COMBAT_FINDINGS.md, "Session 3".
- **The enemy watcher** no longer jumps to enemies as they spawn.

## 0.18.45: the enemy watched too (for the VR parry), more of the hero watched

**Install:** `xinput1_4.dll` (the host is unchanged).

- **What your second session decoded** (docs/COMBAT_FINDINGS.md, "Session 2"):
  - **The hit messages:** Jin's sword hits, the enemies' hits, kills, parries (Jin +12.5 health), the parried enemy's stagger, perfect
    parry counter kills, chip damage, fall damage, resolve heals, and the crawling (finisher) state.
  - **The attacker:** every message carries the attacker's character.
- **The enemy watcher (ENEMY/s):** the enemy last in a hit with Jin (hit by him, or the one who hit him) is watched like the hero,
  so the next session shows his attack wind-up and his stagger.
- **The hero watch covers 32 KB** (was 16).
- **The COMBAT/s count is relabelled:** it was "attacks incoming", but those entries are paired actions (finisher, mount,
  assassination).

## 0.18.44: flat-play capture for the whole design doc

**Install:** `xinput1_4.dll` (the host is unchanged).

- **23 action tags** (they replace 0.18.41's five): numpad 0 - 9, Ctrl+numpad 0 - 9, numpad / * -. Each tag and what to do is in
  docs/DATA_CAPTURE_PLAN.md.
- **Keyboard / mouse presses are logged** with their time in each second (INPUT/s: `LMB+@312` = pressed 312 ms into that second,
  `-` = released; WASD as "moving"). Keyboard / mouse play now gives usable data.

## 0.18.43: combat observer from your flat session, right trigger = Focus

**Install:** `xinput1_4.dll` (the host is unchanged).

- **What your flat session showed** (details: docs/COMBAT_FINDINGS.md):
  - **Health and its one writer:** Jin's health is a decimal value on the hero (+0x1974; max at +0x1970). The game changes it in ONE
    function, SetHealth (rva 0x69F3D0), for every character: damage, heals, regen.
  - **Where hits are processed:** every hit goes through the game's HandleHit (rva 0x694080, a virtual of the character classes). It
    reads a hit record: an id, flags, damage-like values. That is where a physical katana's hit can be handed to the game.
  - **The incoming-attack list on Jin** (+0x3918, count at +0x39A0): an enemy's attack is registered there as it starts, about 1 s
    before your parry marks. That is the parry window.
- **The new combat observer logs** every hit (whose, the hit record) and every health change, with your tag, plus a COMBAT/s line with
  the attacks incoming on Jin. It is observe-only and checks the game's code first: if the build differs, it switches itself off and
  the rest of the mod runs.
- **Right trigger = Focus (Focused Hearing):** it sends the touchpad press, which is View on an Xbox pad (Fextralife: "Press the
  Touchpad ... to focus"). `right_trigger` in [controls] changes it. Once kunai / items exist, holding one will take the trigger back.

## 0.18.42: the sheath mouth at your left hand, a wider catch, the game's draw / put-away fully hidden, play_mode in the ini

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Where the sheathing catches:**
  - While your left hand holds the sheath, the mouth is where your left hand is (within 10 cm of the setting's place). Otherwise it is
    the left fist's place from `sword_left_hand_cm`. `sword_sheathe_shift_cm` (0) moves it up (+) or down (-) the sheath.
  - **Wider catch:** 12 cm off the line (`sword_sheathe_tip_cm`), 55 deg (`sword_sheathe_deg`), and 12 cm short of the mouth. In the
    0.18.41 log your misses were right at the mouth but 37 - 46 deg off the sheath's line, while it only took under 35 deg.
- **The sheathe animation that still showed now and then:**
  - The game's own put-away time (16.3 s after its draw) is saved in gotvr.ini (`sword_game_autosheathe_ms`), so from the very first
    one it is hidden from 1.5 s before it is due. It stays hidden 1.6 s after the sword is in.
  - The game's draw animation (ours, and the background redraw after the game's put-away) is hidden until 1.8 s after the sword
    appears, instead of a fixed 1.8 s from the request.
- **Flat play:** gotvr.ini now has `play_mode=vr`. Set it to `play_mode=flat` for flat-screen play with the logger (and back to `vr` for
  the headset).
  - The logger now also records decimal values on Jin that change only now and then (health, resolve, posture-like values), with old
    -> new value. Before, it recorded only small whole-number flags.
  - Numpad tags as before (1 sword combat, 2 archery, 3 climbing, 4 grappling, 6 cutscene) and F8 for a numbered MARK. Tag each fight
    and press F8 at each parry, hit you land or hit you take, so they can be lined up in the log.

## 0.18.41: sheathing catches the real tip, the game's own auto-sheathe hidden, the thumb works again, numpad log tags

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Sheathing never caught: the mod's tip was about 45 cm too deep.** It used the full 72 cm blade length from the rail's origin, but
  the blade clears the sheath mouth after about 45 cm of rail (that is where your draws come free).
  - The 0.18.40 log has it exactly: every time you lined the blade up with the sheath (6 - 12 cm off the line, 15 - 20 deg), the mod's
    tip was 45 - 48 cm inside the mouth, past its 25 cm window.
  - Now the tip is the real one: `sword_blade_out_cm` (45) = how far the blade travels before its tip leaves the mouth. If the catch
    feels early or late, change that.
  - SWORD/s now logs the closest the tip came to being caught in each second, and the limits it has to meet.
- **The sheathing animation that still played was the game's own automatic sheathe.** The game puts its sword away by itself a while
  after drawing it (18.3 s each time in your log), with its blood flick.
  - Now the game's sword heading for its scabbard by itself is hidden on Jin until it is through. The time it took is learned, and from
    then on the hiding starts 1.5 s before it is due, so the flick is covered too.
  - If you hold the katana, the game's sword is drawn again in the background, hidden. While it is briefly away (about a second) the
    sheath stays on the hip instead of flying into your hand with its sword.
  - **Walking:** whenever an animation is hidden, Jin's legs keep the game's walk. Only his body above the hips is held.
- **The thumb follows your thumb again** (on the sticks, the buttons or the thumb rest). Since 0.18.31 a comment put in the middle of a
  line had cut off the thumb's update. The build had been warning about the unused value all along.
- **Flat-play log tags (NumLock on):** numpad 1 sword combat, 2 archery, 3 climbing, 4 grappling, 6 cutscene.
  - A key writes a "TAG ... BEGIN" banner, the same key again "TAG ... END" (with how long it lasted), and another key switches.
  - With `play_mode=flat`, every STATE/s line carries the tag, so the log can be cut by action (search "TAG grappling").

## 0.18.40: sheathing is yours alone - no sheathe animation, no repeats

**Install:** `xinput1_4.dll` (the host is unchanged).

- **The game is never asked to sheathe its sword any more.**
  - Once drawn (in the background, its animation hidden), the game's sword stays "out" as far as the game is concerned. When you put
    the katana away, the mod slides that sword home along the sheath with your hand, and it stays there in the scabbard.
  - Next time you draw, it simply slides out again, with no game animation at all.
  - If the game puts its sword away by itself (getting on the horse, climbing, an interaction), the next time you take the hilt it is
    drawn again in the background, hidden.
- **Why the sheathe animation kept repeating (0.18.39):**
  - Once the game had put its sword away it stopped moving it, and the mod read its own last placement of the sword as "still drawn".
    So it asked again every 2.5 s, and each request toggles: draw, sheathe, draw ...
  - Now a sword the game no longer animates counts as put away.
- **Sheathing by hand:**
  - Bring the tip to the sheath mouth: where the guard sits less `sword_mouth_below_guard_cm` (4.5), about where your left hand holds
    the sheath. The blade has to be roughly lined up with the sheath (within 35 deg).
  - The tip is caught anywhere from 8 cm before the mouth to 25 cm inside it, so the blade's exact length does not matter. A tick on the
    controller tells you it is close. The blade is taken onto the rail exactly where it is, with no jump.
  - Then push it home. Pulling 6 cm back out frees it again.
- **The katana stays in Jin's hand until you sheathe it.** Letting go of the grip no longer sends it back to the sheath; the fingers
  stay round the hilt. `sword_release_home=1` brings back the old return.
- **A blade let go of half in the sheath stays where it is.** Take the hilt again to push it home or draw it. `sword_auto_home=1` makes
  it slide home by itself as before.
- **Kept from 0.18.39:** with the game's sword out, standing still shows Jin's own pose from before the draw. Pad input (0.3 s) or a
  real change of his head height (a crouch) shows the game's pose through. Walking uses the game's legs.

## 0.18.39: only the sword leaves the sheath (the game's draw animation hidden on Jin), two-handed steering halved

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Only the sword comes out; the sheath stays on the hip.**
  - The sheathed katana is part of the sheath's model, so the separate sword (joint 637, the bone you confirmed) only exists while the
    game has its sword drawn.
  - The mod asks the game to draw in the background the moment your right hand takes the hilt, so the sword is there by the time you
    pull. It asks the game to put it away 1.5 s after the katana is home and your hand has left the hilt.
- **The draw / sheathe animation is hidden on Jin.**
  - While it plays, Jin keeps his own pose from just before: body, head, the sheath on the hip. Only the game's sword is left to the
    mod, so the camera does not move either.
  - While the game stands in its drawn-sword stance, standing still also keeps that pose. Walking with the katana out still uses the
    game's legs for that state.
- **The sword is placed by the mod, not by the game's hand.**
  - It sits where the sheath model's sword would be: your grip, your captures, the hand placement settings and hotkeys all apply.
  - **First draw:** the mod learns where the game's sword sits in the scabbard while the game draws it. This is saved as
    `sword_game_home_*` and logged as "learned where the game's sword sits in the scabbard". Until it is learned, that first draw looks
    like before.
- **No more double draws or sheathes:** the touchpad swipe toggles, and 0.18.37's repeats made the game draw again or sheathe on its
  own. Now a request is only sent when the game's state is known (where it parks its sword while sheathed), never repeated within
  2.5 s, and only for draws the mod asked for. Drawing with an attack button is left alone.
  - `sword_use_game_blade=0` turns all of this off (sword and sheath move as one again).
- **Two hands:** the second hand now turns the katana half as much (`sword_two_hand_steer`, 0.5; 1 = as before).

## 0.18.38: no game draw / sheathe animations, the 0.18.37 TAA change removed, the katana fixed in the fist while you move, hand placement that works

**Install:** `xinput1_4.dll` (the host is unchanged).

- **The game is never asked to draw or sheathe its sword any more.** Drawing and sheathing are yours alone, so Jin does not animate
  either. The 0.18.37 game-sword path (its separate sword in your hand, the sheath going empty, its grip) is gone. The katana is the
  sheath model's own sword again, held in your hand. The leftover settings (`sword_use_game_blade`, `sword_game_grip_*`) are removed
  from gotvr.ini automatically. Ctrl+F9 no longer asks the game to draw.
- **The 0.18.37 per-eye TAA is removed** (`taa_per_eye` is deleted from gotvr.ini). Your culling preset 7 was never touched. Ctrl+F11
  is plain recentre again. The see-through copy of the arm and katana is still open.
- **The katana and the hands on it no longer trail the body while you move.** The sheath's position the game thread used was the one
  from the previous animation frame. Walking or running, the body had moved on by then (about 9 cm at a run), so the sheath rail, the
  hands locked to it and the drawn katana all sat that far behind, on the side you came from. Now the sheath is placed with the body's
  current position, and the drawn katana's sheath frame comes from the same frame as Jin's hand.
- **Placing the hands on the katana works now.** The rail's origin (the katana joint) is about 16 cm below the guard, not at it. The
  old settings were measured from that joint and clamped to the wrong part of the sword, so the right fist could never go further
  down than just under the guard. Now:
  - `sword_guard_cm` = auto: where the guard is, worked out from your captured left grip (Ctrl+F6, thumb on the guard).
  - `sword_right_hand_cm` (9) = how far below the guard the right fist sits.
  - `sword_left_hand_cm` (4.5) = how far below the guard the left fist sits on the sheath.
  - **Live, while you play:** Ctrl+PageDown / Ctrl+PageUp move the right hand 1 cm down / up the hilt. Ctrl+End / Ctrl+Home move the
    left hand 1 cm down / up the sheath. Each change is saved to gotvr.ini right away, and a capture (Ctrl+F5 / Ctrl+F6) sets them too.
  - The grab areas follow: the hilt can be taken anywhere between the guard and the pommel, and the sheath grab is centred where your
    left hand goes.
  - `sword_grip_lower_cm` and `sword_sheath_hand_cm` are removed.
- **The body follows the headset again.** Since 0.18.31 it held still whenever a hand was within 25 cm of the sword, which with your
  hands at your sides was most of the time. Now it only holds while a hand actually grips the sheathed katana, or the tip is at the
  sheath mouth.

## 0.18.37: the game's own sword in your hand, no ghost, no body teleport, faster stand-up, firmer two hands

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Your sword bone (joint 637) is used:**
  - The game's drawn sword is its own model on 637, and it only exists while the game has drawn its sword. The sheathed sword is part
    of the sheath's model.
  - When the blade starts coming out, the mod asks the game to draw (in the background; the attack buttons stay off). Once the game
    has drawn, its sword is shown in Jin's VR fist, held exactly the way the game's own hand holds it.
  - The sheath stays on the hip, empty.
  - When you put the blade home, the mod asks the game to sheathe. If the game does not follow within 2.5 s, it asks once more.
  - **Learning the grip:** the first time, it learns the game's grip from the game's own animation while the game's sword is drawn.
    That takes about half a second of the game standing still with it, and is saved in gotvr.ini (`sword_game_grip_*`). The quickest
    way: draw once with an attack, stand still a moment, then try your own draw.
  - Until the grip is learned it works as before: sword and sheath as one piece.
  - `sword_use_game_blade=0` switches all of this off.
  - Your VR arms are no longer handed to the game while the game's draw or sheathe animation raises its hands.
  - **Expect for now:** the game's draw / sheathe animation still plays on Jin's body, its sheathe includes the blood flick, and the
    model swap takes a moment. Next: timing it (Ctrl+F9) and possibly speeding it up.
- **See-through copy of the arm and katana:** this is the playbook's "temporal trap". With the game's TAA in sequential stereo the
  history is the other eye's image, so near objects (arms, sword) leave a ghost offset differently in each eye. The mod's per-eye TAA
  history (the TAA half of preset 6, which removed ghosting in 0.10) is now on top of your culling preset, which is unchanged.
  - `taa_per_eye` (1). **Ctrl+F11** toggles it live, so you can compare.
- **Body teleporting when you move back, right or left:**
  - Moving backward, Jin's body is drawn turned 180°, which puts the run's forward lean on the other side. The correction needed was
    about 40 cm, right at an old safety cut-off.
  - So it flipped on and off every frame. It is now limited smoothly at 70 cm instead.
- **Getting up from a crouch:** the camera height smoothing only calms small bobbing now. A real height change (crouch / stand, more
  than 4 cm) follows in about 0.06 s, so the headset no longer lags the body.
- **Two hands:** once the left hand has the hilt, only letting go of it (or your controllers more than 90 cm apart) ends the hold.

## 0.18.36: katana held by the fist, left thumb up the sheath, steadier two hands, no idle while holding, blade probe with the game's sword drawn

**Install:** `xinput1_4.dll` (the host is unchanged).

- **The katana stays in Jin's fist:**
  - It used to sit where your controller is, so with your arm out further than Jin's can reach, or with his hand turned less than
    yours, it floated past his hand or wandered around it.
  - Now the katana is carried by his fist as it actually ends up: same place, same line. It cannot leave the hand.
  - SWORD/s shows how far his fist is from where your controller wanted it.
- **Left hand on the sheath:** kept exactly where you captured it, but now turned so the index is toward the hilt and the thumb runs
  straight up the sheath toward the hilt.
- **Two hands:** once the left hand holds the hilt, only letting go of its grip (or pulling the hand far away) ends the hold. Your log
  showed it dropping with the grip still fully pressed.
- **No idle animations while you hold the katana:**
  - Your index finger resting on the trigger sent the game R2 / L2 and counted as "pad input". That made the idle hold let go, so
    Jin's idle animations played.
  - While your hand is on the hilt or the blade is out, the triggers are held off like the attack buttons.
- **The blade probe with the game's sword drawn (your finding):**
  - Your log matches what you saw: the game had its sword drawn (from the attack) from about +0 to +18 s of your third probe run, and
    the joint that lifted the sword alone was during that window.
  - Every Ctrl+F7 line now says whether the game has the sword drawn.
  - **Please repeat it:** draw with an attack, then Ctrl+F7 until only the sword lifts, and press **Ctrl+F8** while it is lifted.
  - With the game's sword drawn, Ctrl+F8 saves that joint as `sword_drawn_blade_joint` (your sheath setup is left alone). The next
    build will hold that sword in your hand.

## 0.18.35: capture the drawn grip, captured hands shown as captured, no katana lag, the original sword probe

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Fix the drawn katana's angle with a capture:**
  1. Draw the katana.
  2. Press **Ctrl+F5**.
  3. Within 3 seconds, point your right controller straight ahead the way you would hold the sword, and look straight ahead.
  4. At 3 s (strong pulse) the mod sets the blade along your view, edge down, and keeps that angle relative to your controller from
     then on (saved in gotvr.ini).
  - With the katana sheathed, Ctrl+F5 still captures the hand on the hilt.
  - Ctrl+Shift+F5 clears both right-hand captures. If the blade's curve bends the wrong way, set `sword_edge_flip=1` and capture
    again.
- **Left hand from your capture:** your Ctrl+F6 capture (saved: 13 cm from the mouth, 4 cm off the sheath's line) is now used
  exactly as you placed it. Jin's hand is no longer re-snapped elsewhere, and the grab radius widens so you can take the sheath at that
  spot.
- **Katana lag in swings:**
  - The hand-tremor filter smoothed the rotation, and a 72 cm blade multiplies any rotation lag at its tip.
  - While the katana is drawn, or held with two hands, that hand's filter reacts almost instantly to fast movement (it still calms
    tremor when you hold still).
- **The original sword-bone test:** Ctrl+F7 now also lifts the joints from your first test (645, 646, 647, 652-654, 593, 594,
  586-590), right after 637 / 644 / 651.
  - **645** is the one to watch: it is the parent of 646 / 647 and was not in the last lists.
  - If only the sword lifts on one of them, press **Ctrl+F8**.

## 0.18.34: does the sword have its own bone? (probe + timing test)

**Install:** `xinput1_4.dll` (the host is unchanged).

1. **With the sword sheathed, Ctrl+F7.** The first press lifts the katana with 592 held still (as before). The next presses lift
   **592, then 637, 644 and 651** - the joints the game's own draw moved - then the old candidates. Watch your hip: if only the sword
   lifts (the sheath stays) on one of them, press **Ctrl+F8** to keep it.
2. **Ctrl+F9 twice** (draw, wait about 3 s, sheathe). The log samples the katana joints, 637 / 644 / 651 and the right hand every
   50 ms for 2.5 s, using the game's OWN animation, not the VR arms. It records:
   - how long the draw and the sheathe take
   - when 637 starts moving and where it settles
   - how the game's own hand holds it
   It also logs where 637 sits relative to the sheathed katana before the draw.

## 0.18.33: capture your own grips, the drawn katana follows your controller, steadier two hands, settings in gotvr.ini

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Capture your grips:**
  - **Right hand on the hilt:** press **Ctrl+F5**, then within 3 seconds hold your right controller exactly where and how Jin's hand
    should hold the hilt. You get a tick each second; at 3 s a strong pulse means it is saved.
  - **Left hand on the sheath:** the same with **Ctrl+F6** and your left controller.
  - From then on every grab uses your capture: where along the hilt or sheath the hand sits, and how it is turned. The captures are
    saved in gotvr.ini.
  - **Ctrl+Shift+F5 / Ctrl+Shift+F6** clears a capture. The katana must be in its sheath while you capture.
- **Drawn katana:**
  - The katana is now steered from your right controller and Jin's fist is drawn on it, rather than the katana hanging off the fist.
  - In the sheath, the hand holds the hilt the way you took it (the grip you said looked right in 0.18.30).
  - When the blade comes free, it turns over a quarter second to run along your controller's forward, like the wrist turning the
    blade forward after a draw. Holding the controller forward holds the katana forward, with the knuckles to the side.
  - Settings: `sword_blade_pitch_deg` tilts it, and `sword_blade_sign=-1` flips it if it ever points backward.
- **Two hands, no more jitter:**
  - The hold kept taking and dropping because it checked against the katana as drawn, which the left hand itself was steering. It
    now checks against the right controller alone.
  - Take the hilt 4-30 cm behind your right hand, toward the pommel. You can let go and grab again.
- **Settings now in gotvr.ini:** the katana settings are written in with their defaults the next time the game starts:
  - `sword_grip_lower_cm` (now 10): how much lower on the hilt the right fist sits than your controller, when no capture is saved
  - `sword_sheath_hand_cm` (now -3): how far below the sheath mouth the left fist sits
  - `sword_blade_pitch_deg`, `sword_draw_pull_cm`, `sword_free_min_frac`, `sword_free_arm_reach`, `fp_index_tween_s` and the others
- **Game-draw census:** the Ctrl+F9 test from 0.18.32 is still to do. It finds the game's own separate blade.

## 0.18.32: blade along your controller, lower right grip, left hand up the sheath, two-handed grip, game-draw census

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Blade along your controller:**
  - The blade's direction in your hand is now out of the front of your fist (where the controller points), not along the handle.
  - Jin's wrist turns his fist to match, knuckles to the side, so holding the controller forward holds the katana forward.
  - The same direction is used when you take the hilt in the sheath.
  - If the blade should point a little higher or lower, use `sword_blade_pitch_deg` (+ = up).
- **Right hand lower on the hilt:** the fist now sits 6 cm further toward the pommel than your controller (`sword_grip_lower_cm`). The
  rail from the two katana joints moved the mouth about 8 cm up, which pushed your grip toward the guard.
- **Left hand up the sheath:** the fist sits 6 cm below the mouth (`sword_sheath_hand_cm`, closer to 0 = higher). Its thumb is still
  laid along the sheath to the guard. 0.18.30's "as far as the whole thumb reaches" put it too far down.
- **Two-handed grip:** with the katana drawn in your right hand, bring your left hand to the hilt behind the right fist and squeeze.
  - You feel a tick when you are close and a pulse when it takes hold.
  - Jin's left hand then holds the hilt behind his right one.
  - The katana's line becomes the line between your two controllers, so the left hand steers. It follows 1:1 up to 55° and gently
    less beyond, never blocking.
  - The right hand still owns the katana: letting go of the left returns to one hand, and letting go of the right still puts it away.
  - This follows the playbook's two-handed hold, taken from FEAR VR.
- **Sheathing uses the katana as you see it:** the mod now publishes where the drawn katana really is, and sheathing and the
  two-handed grip check against that.
- **The separate blade (Ctrl+F9):**
  - Your probes show the sheathed katana is one piece of the model with its sheath. A separate blade only exists when the GAME draws
    it.
  - To find where the game keeps that blade, press **Ctrl+F9** once with the sword sheathed (the game draws), wait 2 seconds, then
    press Ctrl+F9 again (the game sheathes).
  - Each press logs the joints of Jin's right hand and which joints appeared, disappeared or moved.
  - Expect the game's own draw and sheathe animation to play during this test; it is the only way to see where the game puts the
    blade. Then send me the log.

## 0.18.31: hands on the real hilt, blade along your controller, sheath probe, body still while reaching

**Install:** `xinput1_4.dll` (the host is unchanged).

- **The hands on the drawn hilt and sheath:**
  - The rail line came from where your controllers were at calibration, which was a few cm beside the drawn hilt. That is why the
    right fist sat to the side of the hilt and the left hand under the armour.
  - The katana joint (591) has one child joint (592), about 25 cm away. If 592 lies along your calibrated line, which the log
    reports, the rail now runs through those two joints, the drawn hilt itself. The end nearer your calibrated mouth is the mouth.
  - Look for `katana: joint 591's child 592 is ... -> rail FROM THE TWO JOINTS` in the log. `sword_rail_from_joints=0` goes back to the
    calibration.
- **Blade along your controller:**
  - Jin's fist used to be about 90° off from your real fist: his hand is mapped with the fingers along the controller, so a sword
    through his fist pointed up.
  - While you hold the hilt, Jin's wrist now bends so the hilt runs along your controller the way a real handle would. The knuckles
    are on top, and the drawn blade points where your controller points. `sword_blade_forward=0` turns this off.
- **Only the sword moving:**
  - None of the 16 joints you tried lifted the sword alone. The katana joint has just one joint under it (592).
  - The blade probe (Ctrl+F7) now first lifts the whole katana while holding 592 still. If the sheath stays and only the sword lifts,
    press **Ctrl+F8**. That saves the katana as the blade and 592 as the sheath, held still during the draw.
  - If that also moves both, the sword and sheath are one piece of the model. The separate drawn katana would then have to come from
    the game itself; see the ROADMAP.
- **Body no longer slides while you reach for the sword:**
  - Your log shows your head 57 cm from Jin's while reaching for the katana, standing still. Jin's body followed your head (the "body
    under the headset" feature) and took the sheath with it.
  - Now, while a hand is near or on the katana (and while the tip is at the sheath mouth), the body stays where it is. It follows
    again afterwards. It still follows while you fight with the blade drawn. `sword_hold_body_still=0` turns this off.
  - `UNDER/s` now logs where the offset points (forward / left of Jin) and the biggest move in a second, in case it happens elsewhere.
- **Slightly longer draw:** the blade now comes free at 60 % out (was 50 %) with the arm at 93 % of its reach (was 90 %). Settings:
  `sword_free_min_frac`, `sword_free_arm_reach`.
- **Faster index finger:** the index finger has its own, faster ease between up, resting on the trigger and pressed: about 0.07 s
  instead of 0.15 s (`fp_index_tween_s`).
- **Fewer false draw starts:** the "the blade is coming out / back in" events now need 1.5 cm out and 0.5 cm back in, so they no
  longer flicker at 1 cm.

## 0.18.30: Jin's own fist on the sword, the thumb on the guard, one-handed draw, start-up turn kick

**Install:** `xinput1_4.dll` (the host is unchanged).

- **The hands sit where Jin's fist really is.**
  - Your photos show the hands placed by the controller's grip point. Where Jin's fist ends up relative to that point depends on your
    hand trims, so it missed the hilt and the sheath.
  - Now the mod measures Jin's own closed fist every frame: the channel his curled fingers wrap around. It then moves and turns the
    hand so that channel lies exactly on the hilt (right hand, where you took it, kept clear of the guard) or on the sheath (left hand).
- **Left hand at the mouth, thumb on the guard:** the left hand goes as far up the sheath as its thumb allows. The thumb is laid
  straight along the sheath's surface with its tip touching the guard. Settings: `sword_thumb_gap_cm` (0.3) and
  `sword_sheath_radius_cm` (1.6).
- **Katana in the fist:** once drawn, the katana is placed so its hilt runs through Jin's fist, at the spot where you gripped it. Your
  third photo showed it about 25 cm off the hand.
- **One-handed draw:** you can now draw with just your right hand on the hilt (`sword_need_sheath_hand=0` is the default). The blade
  comes free once it is at least half out and your arm is at 90 % of its reach (`sword_free_arm_reach`), or at 66 cm as before.
- **Movement forward at start:**
  - About 1.2 s after you get into gameplay (at game start, and after any menu or load of 8 s or more), the mod gives the game camera
    a short left-then-right turn, the same way a snap turn does in the background. Your view does not move.
  - It runs the same forward reset as a snap turn, so walking should face your headset right away.
  - It is skipped if you are using the right stick at that moment. The log line is `controls: start-up turn kick`.
- **Still to do:** the drawn katana in your photo is the whole katana with its sheath. Next time, please pick the blade joint
  (Ctrl+F7 / Ctrl+F8, see 0.18.29) so only the sword comes out.
- **Logs (`SWORD/s`):** how often each fist was snapped, the finger circle's radius, the thumb placed on the guard, the katana held
  through the fist, the right arm's reach, and the start-up turn kicks.

## 0.18.29: the physical katana - hands locked on, only the blade moves, shorter draw, sheathing, no game attacks

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Hands locked on the sword:**
  - When you grab the hilt or the sheath, Jin's hand is fixed to it, both in position and rotation. The line through your fist is
    turned onto the sword's line, and the centre of the fist sits on it. The hand stays like that until you let go, so it can no
    longer turn freely on the hilt.
  - It is also pushed 2 cm toward the palm, so the handle sits inside the fist instead of just above it. Adjust with
    `sword_hand_into_grip_cm`; a negative value moves it the other way.
- **Left hand at the sheath mouth:** the left hand now always sits right at the mouth, 3 cm below it, wherever on the sheath you take
  it. If it still looks low, raise `sword_sheath_hand_cm` (for example to 0 or +2). Positive values are toward the hilt.
- **Only the sword moves (one-time pick, please):**
  1. The katana joint (591) carries the sheath as well. The log now lists every joint that could hold only the blade.
  2. **Ctrl+F7** lifts the next one 15 cm; look at Jin's hip.
  3. When only the sword lifts and the sheath stays, press **Ctrl+F8** to keep it (saved as `sword_blade_joint`).
  - **Ctrl+Shift+F7 / Ctrl+Shift+F8** still pick the katana joint itself.
  - Until a blade joint is picked, it works as before: sword and sheath move together.
- **Shorter draw:**
  - The blade is free after **40 cm** of pull (`sword_draw_pull_cm`), about an arm's extension, instead of the whole blade length.
  - The blade slides out faster than your hand moves, like the real draw where the left hand pulls the sheath back while the right
    pulls the sword out. Jin's hand rides with the hilt.
  - Once the blade is free, his hand settles back onto your controller over a quarter second.
  - Letting go mid-draw now slides the blade home instead of snapping it back.
- **Sheathing:**
  - Bring the **tip to the sheath mouth**, lined up with the sheath (within 30°). You get a light tick when it is close and a click
    when it catches.
  - Then push it home along the rail, with the same rumble as the draw. The hand locks to the hilt again.
  - Pull back out and it is free again. Let go part-way and it slides home.
  - Letting go of the drawn katana anywhere else still puts it back in the sheath, for now.
  - Settings: `sword_sheathe_tip_cm` (7), `sword_sheathe_deg` (30).
- **No game attacks:**
  - The game's draw from 0.18.28 is off (`sword_game_draw=0`).
  - While your hand is on the hilt or the blade is out, the game's light and heavy attack buttons (pad X / Y) are held off, so no
    attack animation can start. Set `sword_block_game_attacks=0` to turn this off.
  - The physical combat itself (your swings doing damage) is the next big step. See the ROADMAP.
- **Logs (`SWORD/s`):** blade joint, the tip's top speed each second, how far the tip is from the sheath mouth (along, off-line and
  angle), and how many attack presses were held off.

## 0.18.28: holding the katana, drawing it with the game, body squares up when you stop

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Left trigger is free again.** The temporary left-trigger calibration is gone. Ctrl+F10 still calibrates, and your saved calibration
  is kept.
- **Hands on the sword:**
  - **Right hand on the hilt:** all four fingers wrap the handle, the index too (a katana has no trigger finger), and the thumb closes
    over them like a fist.
  - **Left hand on the sheath:** the fingers wrap the scabbard at the mouth and the thumb stays almost straight along it, toward the
    guard. That's the thumb Jin uses to push the guard and free the blade.
  - Both ease in over about 0.1 s when you grab and ease out when you let go.
  - You can now grab the hilt anywhere along it: near the guard, in the middle or at the pommel, within 10 cm.
- **Drawing the sword:**
  - While you hold the sword, Jin's hands are drawn **on** it. The right hand slides along the hilt's line and the left stays on the
    sheath by the mouth. Your controllers no longer have to sit exactly on the handle for it to look right.
  - When the blade comes free, the mod sends the game's own draw: the touchpad swipe right, which is key 5 on PC. Jin is then really in
    his sword stance for combat.
  - When the katana goes back in the sheath (for now, by letting go of it), the same swipe sheathes it in the game.
  - To have only the visual draw, without the game's, set `sword_game_draw=0`. Please tell me if you ever see two katanas after the
    game draws.
- **Body squares to the headset when you stop:** after moving, as soon as you stand still (0.2 s under walking speed), Jin turns to face
  your headset, using the same eased turn. This works even when you were looking less than 20° to the side. Set
  `fp_body_square_on_stop=0` to turn it off.
- **Logs:**
  - `SWORD/s` now counts the frames your hands were held on the sword and the frames with the hold finger poses, and shows whether the
    game has the sword drawn.
  - `katana: asked the game to DRAW / SHEATHE` marks each toggle.
  - `BODY/s` counts the turns that happened because you stopped.

## 0.18.27: performance back, katana calibration on the left trigger

**Install:** `xinput1_4.dll`.

- **Performance:** yes, 0.18.26 did it. Your log shows about 27 stereo frames a second against about 60 in 0.18.25. The new "Jin's
  own sounds" check looked at all ~2100 sound sources a second and asked Windows about their memory five times each, which is a
  slow system call, on the game's audio update. It now only compares one pointer ("is this Jin's?") and does nothing more for
  anyone else's sounds.
- **Calibrating the katana rail:** hold the **sheath mouth with your left hand** and the **end of the hilt with your right**, both grips
  held, and pull the **left trigger**. You get a pulse in both hands when it is saved. It only works with both hands within 60 cm of
  the katana and the blade in the sheath, so the trigger does nothing anywhere else. Ctrl+F10 still works too.
- Your Ctrl+F7 / Ctrl+F8 choice is saved: the katana is joint **591**.

## 0.18.26: katana draw (stage 1), Jin's own sounds centred, dodging off, smoother hands, arm stretch

**Install:** `xinput1_4.dll` (the host is unchanged).

- **Drawing the katana (first stage):**
  - Grip your **left** controller near the top of the sheath on Jin's left hip (a light tick tells you when you are there), and grip the
    **hilt** with your **right** hand. Then pull: the blade slides out along the sheath as if on rails, with a light rumble that grows
    with the speed. Near the end of the blade it comes free in your hand (a strong pulse).
  - Letting go of the hilt mid-draw slides the blade home. Letting go of a drawn katana puts it back for now; real sheathing comes in
    the next stage, and so does the game knowing the sword is out, for combat.
  - **Finding the katana (please do this once):** I know the katana is one of a few gear joints at Jin's hip, but not which.
    1. **Ctrl+F7** lifts one candidate 15 cm. Look down at Jin's hips and see what jumps; press Ctrl+F7 again for the next one.
    2. When the **katana** jumps, press **Ctrl+F8** to keep it (saved to gotvr.ini).
    3. Then hold your left hand at the sheath mouth and your right hand at the end of the hilt, both grips held, and pull the **left
       trigger** to calibrate the rail (0.18.27; Ctrl+F10 also works; saved too).
  - Until then it uses joint 646 and a guessed direction.
  - The log records everything while you hold the sheath or the hilt: grabs and releases, how far the blade is out, and all buttons,
    triggers, grips and sticks of both hands (`katana:` and `SWORD/s` lines).
- **3D audio:** Jin's own sounds close to your head (his voice, the gear at his head) are now heard centred, 25 cm in front of you, so
  they no longer swing from ear to ear when you turn your head. His footsteps, his hip gear and everyone else keep their real place.
  Settings: `audio_self_centre`, `audio_self_radius_cm`, `audio_self_ahead_cm`.
- **Physical dodging off:** Jin's body is drawn moving under your head again, without moving the character itself. The code stays;
  `fp_roomscale=1` turns it back on.
- **Started in third person:** you loaded in crouched (Jin's head at 99 cm). The mod only accepted a standing height (1 m and up),
  so it waited 6 s for him to stand. Crouched now counts too.
- **Hand smoothing:** a small "soft deadband" on top of the filter. Tremor smaller than 1.5 mm / 0.25 deg is held completely still,
  and anything bigger passes straight through. Rotation is filtered a little more than position, because small wrist wobble shows the
  most. Settings: `fp_hand_stab_dead_mm`, `fp_hand_stab_dead_deg`.
- **Looking down at yourself:** the body follows your head only past **30 deg** while you look down (20 deg otherwise).
  `fp_body_head_deadzone_down_deg` sets it.
- **Arm stretch:** if your arms are longer than Jin's, his upper arm stretches up to 10% (about 5 cm) so his hand still reaches yours,
  spread through the bicep up to the elbow. `fp_arm_stretch` (0.10) sets it; 0 turns it off.
- **Shadows / ghosting around the hands:** not fixed yet. Please send a headset photo and tell me if it is in one eye or both and
  whether it trails behind the hand when you move it.

## 0.18.25: 3D audio, head back in pauses / cutscenes, no camera orbit, faster room-scale, eased body turn

**Install:** `xinput1_4.dll` (the host is unchanged).

- **3D audio:** the game's sound "ears" now sit on your head in first person. Turn your head and a voice on your left moves to your right
  ear, the same way God of War VR does it. It uses the game's own audio engine (Wwise) and mix: the mod only changes where the
  listener is, so all the game's distance and occlusion effects stay. Use headphones. Cutscenes and third person keep the game's
  camera as the listener. `audio_head=0` turns it off.
  - Not done yet: Jin's own sounds (his voice) may still pan a little when you turn your head. God of War VR centres the player's own
    sounds in front of you, and that is the next audio step.
- **Jin's head in pauses / cutscenes:** when the game freezes on a frame (pause menu, map, the first frame of a cutscene) it stops
  animating Jin and keeps drawing the last pose the mod made, with the head hidden. The mod now puts the game's own pose back for those
  frames.
- **Camera orbiting when Jin walks:** when Jin walks to follow you, the game turns him to face the walking direction. His head is a few
  cm in front of his hips, so the view slowly swung round with it. The view is now tied to Jin's drawn body (the one facing your
  headset), so turning him to walk no longer moves it. `fp_view_on_drawn_body=0` brings back the old behaviour.
- **Room-scale catches up faster:** Jin walks faster to get under you and starts at 12 cm (was 15). His drawn body follows your
  head in 0.1 s instead of 0.5 s (`fp_body_under_follow_s`).
- **Body turn past the threshold:** it now eases in and out over about 0.35 s instead of snapping (`fp_body_turn_s`, 0 = at once).

## 0.18.24: see-through arms fixed properly, hand stabilisation, room-scale Jin, flat play with logging, new controls

**Install:** `xinput1_4.dll` (the host is unchanged since 0.18.23).

- **See-through arms:** what you saw explains it:
  - At load the menu said AO was off, but the arms were still see-through. 0.18.23 only wrote "AO off" into the game's settings
    (which the menu displays); the game's renderer only takes settings when the game *applies* them, so AO was really still on.
  - Your first toggle (screen-space shadows off) made the game apply everything, which turned AO off for real. The arms were still
    see-through, so **screen-space shadows OFF shows the same problem**.
  - Turning screen-space shadows back on applied again with **AO off + SSS on**, and the arms were fixed.

  So the mod now sets exactly that, **AO off and screen-space shadows on**, through the game's own settings apply (the same path the
  menu uses), once, when first person starts in gameplay. The game saves it like a menu change. F5 (third person) or Shift+F6 put your
  own settings back; `fp_ao_off=0` leaves them alone. If the game ever refuses, the log says so and you can set SSAO Quality off
  in the menu.
- **Hand stabilisation:** when you hold a hand still, the jitter is smoothed out; when you move it, the filter lets go at once so
  swings don't lag. It is a One Euro filter, the standard for this; FEAR VR's own code isn't public. In the test its jitter is
  about 3.5x smaller with ~1 cm of lag at 1 m/s. It resets on recenter, tracking loss and jumps. Tune it with `fp_hand_stab_min_hz`
  (1.2: lower = steadier) and `fp_hand_stab_beta` (10: higher = less lag when moving). `fp_hand_stab=0` turns it off.
- **Body turn threshold:** 20 deg (`fp_body_head_deadzone_deg`).
- **Room-scale:** step away from Jin (more than 15 cm) with the left stick at rest, and the stick walks Jin under your head. His
  real collider moves, so you can sidestep an attack for real. The view stays where your head is while he catches up.
  - It is off while riding, climbing, jumping and in menus.
  - If he can't get there (a wall), it pauses for 1.5 s.
  - `fp_roomscale_start_cm` (15) / `fp_roomscale_stop_cm` (6) set the distances; `fp_roomscale=0` turns it off.

  This uses the game's own walking, so he catches up in a fraction of a second rather than instantly.
- **Controls (your new scheme):**
  - **X (hold)** opens the item wheel: pick with the right stick, let go of the stick to use the option, or let go of X to close it.
  - **Left menu button (three lines)** opens the game menu.
  - **Left trigger** is unbound for now (it will fire the held item later).
  - Everything else is as before: Y quick attack / assassinate, A jump, B dash, stick clicks run / crouch, hand at head + trigger
    for wind / HUD.
- **Flat play with logging:** put `play_mode=flat` in `[gotvr]` of `gotvr.ini` to play on the monitor with your normal camera and pad,
  no headset. The log then records Jin's state every second plus every change of the game's internal "flag" values for Jin, with
  their old and new values. Press **F8** to drop a numbered MARK line at a moment worth looking at ("climbing now"). Climb, do
  takedowns, swim, use the bow, get hit, then send me `gotvr.log` with what you did after each MARK. Delete the line (or set
  `play_mode=vr`) for VR again.

## 0.18.23: solid arms (AO), arms through landings, eye position, faster body, headset recenter, forearm twist

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host now passes on the headset's recenter).

- **See-through arms:** you found it: the game's **ambient occlusion** (SSAO Quality) printed the grass behind the arm onto it. AO is now
  held **off** while first person runs in VR, and your menu setting comes back when you leave first person (F5) or quit. **Shift+F6**
  turns it back on to compare; `fp_ao_off=0` leaves it alone. AO off also saves GPU time. If the arms are still see-through with AO
  on in the menu, the game isn't taking the mod's change, so set SSAO Quality to Off in the menu yourself and tell me.
- **VR arms at the end of a jump:** the "jump" window that stops the game taking over the arms lasted 1.2 s from the button press, so
  longer falls landed after it. The landing animation lifts the hands, which the mod took for a climb. The window now lasts while
  Jin is in the air (up to 4 s) plus 0.6 s after touching down. A jump can't switch on the "riding" state any more either.
- **View position:** 3 cm to Jin's right (`fp_eye_left_cm=-3`) and 2 cm higher (`fp_eye_up_cm=-1`).
- **Body turning:** Jin's body turns to the headset about 3x faster (0.08 s, up to 1440 deg/s), so big turns don't show his body.
- **Movement direction:** the mod already turns the stick to where the headset looks on every frame, with no smoothing. What you feel on
  big turns is **the game turning Jin** at its own run-turn speed (wide turns and pivots above ~45 deg). Removing that needs the
  game's lock-on / stance strafing, the route the Bloodborne VR mod uses, which is on the roadmap. The faster body turn hides most of it.
- **Meta / Oculus recenter button:** holding it now recentres the game view exactly like F11 (also Virtual Desktop's reset).
- **Forearm twist:** when you turn your hand (palm up / palm down), the forearm now turns with it the way a real one does: nothing at
  the elbow, more toward the wrist, all of it at the wrist. Before, the whole roll sat in the wrist joint, which pinched the wrist.
  `fp_forearm_twist` (1.0) scales it; 0 turns it off.

## 0.18.22: movement always follows the headset, body turns to the headset past a small threshold

**Install:** `xinput1_4.dll`. This changes the 0.18.21 movement and body behaviour; the 0.18.21 fingers and the Shift+F6 test stay.

- **Movement:** the left stick's forward is now **always where the headset looks**, every frame, also while you are already moving.
  Look right while running and you run right. `[controls] move_follows_head=0` brings back the 0.18.21 behaviour (forward taken
  when you start moving, snap turn or leave a menu).
- **Body:** Jin's body and sword turn to match the headset once you look more than **7.5 deg** away, standing **and** moving. The turn
  is smooth (0.25 s) and now ends square to your view (within 0.5 deg instead of 2). `fp_body_head_deadzone_deg` sets the threshold
  (5 to 10 is the range you asked for); `fp_body_head_follow_s` sets the turn speed (this ini key was being ignored before; fixed).

## 0.18.21: forward only when you start moving, fingers fixed (fist and thumb), fingers ease into place, arm shading test

**Install:** `xinput1_4.dll` (the host is unchanged since 0.18.20).

- **Movement forward:** it is now set **only** when you start moving from rest (and still at snap / smooth turns, the start, menus
  and Recenter). The stop capture from 0.18.19 is gone. 0.18.20 also re-set forward on every frame of a turn past the threshold while
  moving; that is gone too. While moving, looking more than 45 deg away still turns Jin's body and sword to your view, but the
  direction you walk stays where it was until you next start from rest or snap turn.
- **Grip pose:** each finger joint used to take its bend axis from the bone **after** the joints before it had bent. Once the knuckle
  had turned ~80 deg, that bone pointed straight into the palm and the axis became noise, so the middle joints bent sideways (your
  photo). Each finger now bends on **one hinge**, worked out once from Jin's straight reference finger, for all three joints
  including the fingertip joint, which never bent before. Fist angles are 80 / 100 / 60 deg (knuckle / middle / tip). Checked on
  Jin's own hand data, both hands: all four fingertips land on the palm, each finger in its own plane.
- **Thumb on the stick:** the thumb now curls the way it does in a fist: it swings across the palm and bends at all three joints
  (15 / 40 / 60 deg), so its tip lies over the curled index and middle fingers instead of just dipping down.
- **Fingers ease into place:** every finger moves on a smooth spring, about 0.15 s from open to closed. It eases in and out with no
  overshoot, so pressing grip, touching the stick or the trigger no longer snaps the pose. `fp_finger_tween_s` (0.15) sets the speed.
- **Index:** it still follows the trigger (rests a third closed when your finger is on it, closes fully when pulled). Grip alone closes
  middle, ring and little finger.
- **See-through arms, a new test (Shift+F6):** your crop shows dark, grass-shaped streaks **on** the forearm skin, and it changes with
  where you look. Those are the signs of a screen-space shadow or occlusion effect: it works from the depth image, and for an arm a
  few cm from your eyes it samples the grass behind it and darkens the arm in the grass's shape. **Shift+F6** cycles:
  1. Screen-space shadows OFF.
  2. Plus ambient occlusion OFF.
  3. Back to your own settings.

  The log line `NEAR/s` shows what the game holds. If Shift+F6 changes nothing, the game may only read these settings when its menu
  applies them. Then please switch **Screen Space Shadows** (and Ambient Occlusion) off in the game's graphics menu while in VR and
  tell me if the grass goes. Either way, the answer pins it down.

## 0.18.20: grass over the arms, fingers fixed, thumb and index touch, forward when you start walking, body turns while moving

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host now reads thumb and trigger touch).

- **Grass over the arms:** your photo shows it in the game's own image, so it's in the game's rendering, not the headset. It
  started with 0.18.11, when the mod moved the near clip from the game's 10 cm to 2 cm. Grass appears to be drawn with the game's own
  10 cm, so grass up to about five times farther away than your hand counts as nearer than the arm and is drawn on top. The near
  clip is back to the game's 10 cm (`fp_near_cm=0`). The cost: an arm closer than 10 cm to your eyes gets cut open again.
  **Shift+F7** still switches to 2 cm so you can compare.
- **Fingers:** the thumb was mistaken for the ring finger and the bend direction came from the controller rather than Jin's hand, which
  caused the odd pose. Now:
  - the thumb is the shortest finger chain;
  - the fingers curl toward Jin's own palm;
  - **grip** curls middle, ring and little finger;
  - **trigger** curls the index (touching the trigger closes it a third);
  - **thumb on the stick, a face button or the thumb rest** puts the thumb down; lift it and the thumb comes up.
- **Movement forward:** set the moment you **start** moving from rest, to where the headset looks then, so you can look around while
  standing and walk off where you look. Snap turns, the game start, menus and Recenter still set it too.
- **Body while moving:** looking more than the threshold (45 deg) away from the body while moving turns the body and the sword with
  your head, as when standing, and the direction you walk turns with it.
- **Gesture text removed:** the little "HUD" / "GUIDING WIND" text that appeared when you raise a hand is off by default
  (`gesture_indicator=1` brings it back).

## 0.18.19: arms keep working through jumps, body turn threshold respected, forward reset when you stop, fingers (second try), HUD test key

**Install:** `xinput1_4.dll`.

- **Arms stop at the start of a jump / body turning before 45 deg:** the mod's "is Jin on the horse?" check was fooled by jumps. A jump
  lifts gear he carries on his back above the height the check used, and for that moment the mod thought you were riding. It
  switched the VR arms off and snapped the body to the game's facing, which is the turn before 45 deg you saw. Now two
  horse markers must be up (on the horse both are), for a quarter of a second. Also, a jump press no longer hands the arms to the
  game.
- **Forward when you stop:** when you stop moving, forward is set again to where your headset looks.
- **Fingers:** 0.18.18 found only 3 fingers per hand (Jin's hand has extra helper joints the finder didn't expect). The finder now
  works from each fingertip back to the hand, ignoring helper joints. The log also writes out the full joint layout of both hands
  once (lines under `fingers:`), so if it still fails I can fix it exactly. Please send `gotvr.log`.
- **Arm see-through, next test:** the HUD is drawn by the headset as a separate panel **on top of** the game image, fixed in the room in
  front of you. That fits everything you've seen: only in the headset, never in the monitor window, solid when you look down and to
  the left (the panel is out of the way there), and "like an added texture". **Shift+F4** cycles:
  - HUD as before;
  - HUD with straight alpha (colour where the HUD is empty can't be added);
  - HUD hidden.

  If the arms are solid with it hidden, the HUD panel is the cause. Also tell me whether it happens in **both eyes** or one: close
  each eye in turn.

## 0.18.18: VR fingers, walking forward = where you looked at the last snap, menus keep your direction

**Install:** `xinput1_4.dll`.

- **Fingers:** Jin's fingers no longer come from the game's animation. Each finger is set from the character model's own neutral
  hand pose on your VR hand. **Grip closes a fist** (a light squeeze is already a full fist, as in the God of War VR mod;
  `fp_grip_fist_full`). The **trigger also curls the index finger**. Every finger bend is a per-finger, per-joint angle, so grips
  that wrap a sword hilt or other handles can be added later. If the fingers ever bend backwards, set `fp_finger_bend_sign=-1`.
  The log's `fingers:` line says whether it worked (please send the log either way).
- **Walking forward:** forward on the left stick is now where your **headset** pointed at the moment of your last snap turn
  (also set once when the game starts, after menus and after Recenter). Looking around while walking no longer changes it. While
  walking, the body faces that same direction.
- **Menus:** you come out of a menu facing the same way you went in. The game camera quietly catches up behind you.
- **Body:** the turn threshold is now **45 deg** (`fp_body_head_deadzone_deg`). Following your headset sideways is smoothed over half a
  second (`fp_body_under_follow_s`).
- **Grass seen through the arms (headset only):** the monitor never shows it, so it's added after the game renders, in the
  headset's own processing. That points to **Virtual Desktop's Synchronous Spacewarp (SSW)**. The game runs at ~58 frames per second
  per eye, so SSW makes up the in-between frames. Without depth information it guesses motion from colour, and close objects
  (your arms, your body when you look down) then show the background through them in grey ghost shapes and wobble. Please try
  **SSW = Disabled** in Virtual Desktop's settings (Streaming tab). If the arms are solid then, that's the cause. The proper
  fix is sending the game's depth to the headset, so Spacewarp knows what's near.

## 0.18.16: even 35 deg threshold, body faces where you look when moving, body under your headset, steadier view

**Install:** `xinput1_4.dll`.

- **Body turning too easily:** the body's facing is now kept as a direction in the world. Before, it was kept relative to Jin's
  game character, which the game turns on its own (toward the camera after a snap, toward where he runs), and those turns dragged
  the drawn body along. Standing, it now stays put until you look more than **35 deg** away from where his hips face, the
  same both ways. It then turns to your view and stops. Change the angle with `fp_body_head_deadzone_deg` in `gotvr.ini`. A snap
  turn still brings him round.
- **Moving in any direction:** while walking or running, the body always faces where your headset looks, including strafing
  and backing up. The legs run in that direction too, so no more looking into his shoulder. `fp_body_move_follow_s` (0.15)
  sets how quickly it follows.
- **Body under your headset:** step or lean sideways in your room and Jin's drawn body (and his sword) moves with you, up to 60 cm
  (`fp_body_under_max_cm`; `fp_body_under_hmd=0` turns it off). The game still decides where Jin really is; beyond that you're
  leaving his body.
- **Jitter when looking down:** the view's sideways position followed every small twitch of Jin's animated head. It's now smoothed
  over 1 s (`fp_lean_smoothing_s`). If near objects still wobble, check Virtual Desktop's Synchronous Spacewarp: at this mod's
  ~58 fps per eye it can make close objects (your own body) wobble.
- **Arm overlay:** the TAA test made no difference, so it's removed. **Shift+F7** now switches the near clip between 2 cm and the
  game's own 10 cm. If the overlay disappears at 10 cm (the arms get cut off very close instead), the near clip is involved. A
  screenshot of the monitor window while it happens would help a lot.

## 0.18.15: hips square to your view, the last three transparency attempts removed, a TAA test key

**Install:** `xinput1_4.dll`.

- **Removed:** 0.18.14's culling change (the black screen edges), 0.18.13's dither-circle change and 0.18.12's environment
  near-clip change. The 2 cm near clip from 0.18.11 and the near fade switched off in 0.18.10 stay.
- **Lopsided body follow:** Jin's standing stance has his hips turned about 32 deg to the right (the log shows it). The body follow was
  centred on the direction his whole model faced, not on his hips, so it turned the body with only a little look to the right
  but needed a big look to the left. Standing, the hips are now turned to face exactly where the body follow points. The turn
  is symmetric, and the right shoulder is no longer pulled back. It fades out when you walk, so his gait is left alone.
- **After a snap (or smooth) turn** the body always comes round so the hips face where you look, even within the dead zone.
- **"Background drawn on top of the arm":** that looks like the game's anti-aliasing (TAA) blending in the wrong part of the previous
  frame. **Shift+F7** now cycles the anti-aliasing on its own, leaving culling and everything else as they are:
  0 = as now, 1 = each eye uses its own previous frame, 2 = no history (jagged edges, a test only).
  Please tell me which of the three makes the overlay go away. Also try **Shift+F10** (VR arms off) to see whether it only happens
  with the IK arms.

## 0.18.14: arms no longer culled, Jin stands square

**Install:** `xinput1_4.dll`.

- **Transparent arms (new diagnosis):** what you noticed points to the game deciding not to draw Jin's arm and hand
  meshes. Those meshes vanish whenever you look straight ahead, but are solid whenever you look down-left, wherever your hands are.
  - The game decides from where its *animation* puts the arms. In the standing stance his left hand rests on the katana at the
    left hip, down-left of your eyes, and those meshes are only drawn while that spot is in view.
  - Where they're in view for one eye only, the arms look half see-through.
  - In first person, the area the game checks before drawing now starts 1 m behind your eyes, so nothing of Jin gets dropped.
    Far away it is barely wider, so the cost should be negligible.
  - (Removed in 0.18.15.) Shift+F7 switched this off and on. Please try it once: if the arms go transparent again with it
    off, that confirms the cause.
- **Standing stance:** when the idle pose is taken, the angle of his hips and shoulders is measured and the pose is turned to
  face straight forward (the log's `IDLE/s` line shows how much). `fp_stance_square=0` turns this off.

## 0.18.13: arms solid wherever you look, Jin's idle animations held still

**Install:** `xinput1_4.dll`.

- **Arms transparent depending on where you look:** this was the game's third-person "see the hero through whatever is between
  him and the camera" effect, which cuts a dithered circle around where Jin is on screen. In first person Jin is at the camera,
  so the circle moved with your head, not your hands. That circle is now switched off in first person (`fp_focus_stipple_off=1`).
- **Idle animations:** once Jin has stood still for half a second, his pose is kept as it is and the idle fidgets (weight
  shifts, looking around) no longer move the body or the view. Your arms still follow the controllers on top of it.
  - It lets go smoothly the moment you press anything, move a stick, walk, get hit, or the game takes the arms (flute,
    climbing, takedowns).
  - `fp_idle_hold=0` turns it off; `fp_idle_settle_s` sets how long he stands before the pose is taken.
  - The log line `IDLE/s` says whether it is holding and, if not, why.

## 0.18.12: smooth snap turn, hands pivot on the wrist, arms solid up close, view 3 cm back

**Install:** `xinput1_4.dll`. Culling, pop-in and shadows are untouched.

- **Snap turn wobble:** after a snap the mod turns the game's own camera after your view in the background. Each degree it
  turned was subtracted from the view one controller poll late, so the view overshot a little and came back. It's now subtracted
  in the same frame the camera turns, so the view stays perfectly still while the game camera catches up.
- **Hands:** the controller's grip point is the centre of your fist, which sits on the palm side of your wrist. The wrist was
  placed only 7 cm back from it, so the hand hung a few cm low and turned around a point below your real wrist. It now also
  goes **3 cm toward the back of the hand**. If it still isn't on your wrist, **Ctrl+F1 / Ctrl+F2 / Ctrl+F3** move the wrist
  point 1 cm back / up (toward the back of the hand) / toward the thumb; **Ctrl+Shift** moves it the other way. Every change is
  saved like the F1-F3 angles. The old Ctrl+F1/F2/F3 functions are now **Ctrl+Alt+F1/F2/F3**.
- **Arms transparent-ish close up:** the game takes its near clip distance from its environment settings (10 cm). 0.18.11
  changed only the camera, so the anti-aliasing still worked out depth as if the near clip were 10 cm. An arm 20 cm away was
  treated as if it were 1 m away, and the anti-aliasing pulled in the background from behind it. The environment now says 2 cm
  too, so everything agrees.
- **View:** 3 cm further back (`fp_eye_forward_cm` 4 -> 1).

## 0.18.11: each eye culls for itself, view 5 cm back, hand angle keys, arms no longer cut open up close

**Install:** `xinput1_4.dll`.

- **Objects missing in one eye:** each eye now decides what to draw from its own view (new F6 setting **7**, the default).
  Setting 4 checked every object against what the *other* eye saw on its last frame, so anything that eye couldn't see (behind
  Jin's arm, behind a nearby grass clump or post) was dropped from this eye too. Setting 7 drops that check. Nothing is culled
  in one eye only, at some GPU cost. If the left-eye streaks/pop-in come back, press F6 four times (7 -> 1 -> 2 -> 3 -> **4**)
  and tell me. 7 is setting 4 without the cross-eye check, so a return of the pop-in would point at that check itself.
- **View position:** 5 cm further back (`fp_eye_forward_cm` 9 -> 4).
- **Hand angles:** **F1** pitch, **F2** yaw, **F3** roll, 5° per press for both hands (Shift+F1/F2/F3 = the other way). Every
  press is saved to `gotvr.ini` (`fp_hand_pitch_deg` / `_yaw_deg` / `_roll_deg`), so whatever you settle on is the default next
  time. The log line `NEAR/s` shows the current values. The old F1 / F2 / F3 (HUD layer A/B, render size, letterbox crop) are
  now **Ctrl+F1 / Ctrl+F2 / Ctrl+F3** (0.18.12: Ctrl+Alt+F1 / F2 / F3).
- **Arms see-through very close up:** the game clips everything closer than 10 cm to the camera. The near-plane change from
  0.18.9 is back at 2 cm, made consistent this time: some ticks used to keep the camera saying 10 cm while the projection said
  4 cm, which is the likely source of 0.18.9's one-eye shadows. If shadows show in one eye again, set `fp_near_cm=0` and tell me.

## 0.18.10: setting 4 by default, Jin no longer see-through, hands, grip free for the sword

**Install:** `xinput1_4.dll`.

- **Pop-in:** F6 setting 4 (the game's own cross-eye culling) is now the default, as you asked. If you had `history_mode` in
  your `gotvr.ini`, remove it. Shadows in one eye were probably 0.18.9's near-plane change, which is reverted.
- **See-through Jin:** the game fades its hero out when the camera comes within 1-2 m (a third-person feature). That fade is now
  switched off while you are in first person.
- **Hands:** what looked right with the controller pointing down is now what you get holding it forward.
- **Stand tall** is off by default; **right grip** is unbound - it will drive the physical sword.

## 0.18.9: left-eye pop-in (new fix), hands the right way up, arms not see-through, standing tall, game-animated flute / climbing

**Install:** `xinput1_4.dll` (host unchanged: 0.14.2).

- **Left-eye pop-in:** your F6 test pointed at the per-eye occlusion (hidden-object culling). It assumed which of the game's two
  depth buffers belonged to which eye; now the mod records it every frame, so each eye only tests against its own last frame
  with the camera that drew it. This is the new default (F6 setting 1). Please check pop-in and the "missing in one eye" objects
  again. F6 setting 6 is the old method, for comparison.
- **Hands:** turned the right way up.
- **See-through arms:** the near clip plane is now 4 cm in first person (`fp_near_cm`).
- **View height:** 5 cm higher (`fp_eye_up_cm` = -3).
- **Sword stance:** when the game crouches Jin a little, he is drawn standing tall with his feet planted (`fp_stand_tall`). A
  deep sneaking crouch is left as the game plays it.
- **Game animation owns the arms** (smooth hand-over) during: the flute (from the wheel, until you move or dodge), takedowns /
  quick attack (Y, 2.5 s), and whenever the animation raises a hand to head height (climbing, ledges, grappling).

## 0.18.8: VR arms found, history reset on F11

**Install:** `xinput1_4.dll` (host unchanged: 0.14.2).

- **VR arms:** 0.18.7 found Jin's skeleton but not his arms (the items he holds hang far below his hands and confused the
  search). The search now follows the bone chain itself. The log lines starting `arms:` say what it found; if it fails it
  writes the whole skeleton into the log so I can fix it from there.
- **Left eye:** your TAA switch worked (the per-eye TAA is running for the first time since 0.10). To find what still differs
  in the left eye, please try this in one session and tell me what changes:
  1. **F6** cycles five "history" settings (the number shows as white squares): 1 = default, 2 = the game's own TAA, 3 = TAA
     off, 4 = everything shared between eyes, 5 = everything reset every frame.
  2. **F11** (or the wheel's Recenter view) now resets everything like the pause menu does - press it when the far objects
     look wrong in the left eye.

## 0.18.7: VR arms and body (full-body IK, stage 1), five-option wheel, view centred

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host 0.14.2; they must be the same build).

- **The streaks in one eye:** not motion blur (your setting was already 0). Since we tried DLSS (0.11), the game's own TAA has
  been switched off in every session - the game's **Upscaling** setting is still doing the anti-aliasing, and an upscaler
  keeps one history for both eyes. Please set the game's Upscaling method back to **Off / TAA** (Settings > Display/Graphics).
  The log then shows the per-eye TAA working again ("TAA/s ... restored" above 0) and no "AA WARNING".
- **Wheel:** Heal, Photo mode, Call horse, Play flute, Recenter view (five sectors, clockwise from the top).
- **View:** 3 cm higher and centred on Jin's head (it was 3-4 cm toward his left shoulder). Fine tuning: `fp_eye_up_cm`,
  `fp_eye_left_cm` (positive = toward his left) under `[gotvr]`.
- **Full-body IK, stage 1:** standing still, Jin's body turns to where you look once you look more than 35 degrees away; his
  arms follow your controllers (shoulders shrug for far reaches, elbows bend down and out, hands take the controllers' angle,
  the sword in his hand follows). **Shift+F10** turns the VR arms on/off. If a hand angle is off, tell me which way - trims:
  `fp_hand_pitch_deg`, `fp_hand_yaw_deg`, `fp_hand_roll_deg`, `fp_hand_back_cm`. Not yet: attacks / climbing still let your
  controllers own the arms.

## 0.18.6: the left-eye streaks are motion blur, sword turns with the body, HUD gesture

**Install:** `xinput1_4.dll`.

- **Streaks on fast head turns (the old "left-eye pop-in"):** your Shift+F9 test proved it belongs to the eye the game draws
  first in each pair. That eye's "previous frame" is the other eye from a moment earlier, so the game thinks the whole view
  moved a lot and its motion blur smears terrain and trees across the screen. The mod now holds the game's motion blur at 0
  while in VR (your setting comes back when you quit). If you still see streaks, set Motion Blur to 0 / Off in the game's
  graphics settings - the log line starting "motion blur:" tells me which case it is. Shift+F9 is still there; leave it off.
- **Sword when running backwards:** carried items (sword, scabbard, bow...) now turn with the body.
- **HUD gesture:** the game only shows the objective when it sees "hold D-pad right, then flick the right stick up"; the mod
  sent both at the same instant. It now does them in order, like a player. Note the game itself hides most of its HUD out of
  combat - the gesture brings up what the game shows for the objective.

## 0.18.5: running backwards, HUD gesture, left-eye pop-in test

**Install:** `xinput1_4.dll`.

- **Running backwards:** the game has no backpedal - Jin turns round and runs at the camera. Now, when he faces away from your
  forward direction, his body is drawn turned round to face forward, so his run plays forwards while you move back. Looking
  around with your head doesn't turn him; it's off on the horse. `fp_body_follow=0` under `[gotvr]` turns it off.
- **HUD gesture:** left hand at the head + trigger now puts the normal-size HUD panel (room-locked) straight in front of you,
  and the game shows your objective on it.
- **Left-eye pop-in:** Shift+F7 is gone. New test: **Shift+F9** swaps which eye the game renders first. Please tell me whether
  the pop-in moves to the RIGHT eye after Shift+F9, or stays in the LEFT eye. Press Shift+F9 again to go back.

## 0.18.4: starting on the horse no longer breaks first person

**Install:** `xinput1_4.dll`.

- When the game loaded with Jin on the horse, the mod took a piece of riding gear 40 cm above his head for the head. That put
  you above his head, left his real head visible, and dropped you to the floor once you got off (that gear sits on the ground
  then). The head is now found by its shape (the dense cluster of face joints), so lone gear can't win.
- Your 0.18.2 eye test also started on the horse, so the eye position was never really tested - the eyes stay at 0.18.1's
  place (9 cm forward). The log line "first person: hero ... head = joint ..." should now say joint 22 or 123.

## 0.18.3: eye position put back

**Install:** `xinput1_4.dll`. The eyes are back where 0.18.1 had them (`fp_eye_forward_cm` 9); the 0.18.2 move made it worse.
Everything else from 0.18.2 below is still in.

## 0.18.2: left-eye pop-in (new fix), pause menu back on the screen, HUD gesture you can see, eyes moved back (xr=4)

**Install:** `xinput1_4.dll` (host unchanged: 0.14.1, but copying both is fine).

- **Left-eye pop-in:** yes, the occlusion change covered the terrain too: every pass that asks for that occlusion data
  (grass, terrain tiles and six others) had it switched off, and you cycled all three Shift+F8 modes. So you were right, the
  pop-in is not that occlusion. The new suspect: the left eye's camera was set *during* the game's update, after the systems
  that decide what to draw (culling, detail levels, streaming) had already run with the previous right-eye camera. 0.18.2 sets
  the left-eye camera *before* the update. **Shift+F7** turns this on/off so you can compare (log: "pre-compose ON/OFF").
- **Pause menu not on the screen:** your log shows F10 was pressed twice (always -> never); "never" turns the screen off for
  every menu. F10 now only switches between auto and always.
- **Left hand at head + trigger:** the HUD now appears straight in front of you for 6 seconds, and the game's swipe-up is
  pressed (it shows your current objective). Right hand at head + trigger is still the guiding wind; please tell me whether it
  blows.
- **Too far in front of Jin:** the eyes moved back 7 cm (`fp_eye_forward_cm` 9 -> 2) - reverted in 0.18.3 (worse).

## 0.18.1: no left-eye pop-in, one-step snap turn, item wheel you can see (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host 0.14.1; they must be the same build).

- **Left-eye pop-in:** the setting you found with Shift+F8 is now the default (`foliage_occlusion = 2`).
- **Snap turn:** one instant 30° step. The view turns immediately; the game's own camera catches up behind it without you seeing
  it, and the left stick always moves you where you are looking. `turn = smooth` in `[controls]` is now smooth too.
- **Item wheel:** holding the left trigger now shows the wheel above your left hand; the right stick highlights an option, letting
  the stick go uses it.
- **Head gesture test indicator:** when a hand is at your head, a small panel low in your view shows which hand (green), and the
  gesture's name flashes when it fires ("GUIDING WIND", "HUD"). `gesture_indicator = 0` in `[controls]` turns it off.
- **Third person after the map / pause menu:** fixed - the game's fade after a menu no longer counts as a cutscene for the camera.

## 0.18.0: VR controllers, left-eye pop-in attempt 3 (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host changed: 0.14.0; they must be the same build).

**Controllers.** Works with any OpenXR controller: Quest Touch / Touch Plus / Touch Pro, Index, Vive wands, Windows Mixed Reality,
HP Reverb G2, Pico 4. The game sees an Xbox controller, so its own button prompts and control settings apply. A real gamepad
still works alongside. Default layout (your CONTROL_SCHEME.txt; Quest names, other controllers use the same positions):

| Control | Does |
|---|---|
| Left stick / click | move / run |
| Right stick / click | snap turn 30° (or smooth) / crouch |
| A (right lower) | jump |
| B (right upper) | dodge |
| Y (left upper) | quick attack / assassinate |
| X (left lower), left menu | game menu |
| Right trigger | fire / blowgun |
| Left trigger (hold) | item wheel: right stick picks Heal (up) / Photo mode (lower right) / Call horse (lower left), let the stick go to use it; let the trigger go to cancel. You feel a tick on each option. No picture yet. |
| Right hand at your head + right trigger | guiding wind |
| Left hand at your head + left trigger | show / hide the HUD |
| Right grip | block / parry (temporary, until the physical sword exists) |
| Left grip | nothing yet (grab, later) |

The game's rumble plays on the controllers. Every binding can be changed in `gotvr.ini` under a `[controls]` section, e.g.
`right_secondary = b`, `turn = smooth`, `snap_turn_deg = 45`, `wheel_1 = down`, `right_hand_at_head_trigger = dpad_right+rs_up`
(outputs: `a b x y lb rb l3 r3 start back up down left right lt rt rs_up rs_down rs_left rs_right ls_up ... wheel hud none`,
several joined with `+`).

**Left-eye pop-in, attempt 3.** The two earlier attempts are switched off again (they cost a little and did not help). The grass and
foliage culling read data the mod had rewritten for a different purpose; it now gets the game's own data again, at no cost.
If the left eye still pops, press **Shift+F8** twice (mode 2): if that makes it go away we know exactly where it is, otherwise it
is not culling at all. Either way please send the log.

## 0.17.4: left-eye pop-in and left-eye stutter (xr=4)

**Install:** `xinput1_4.dll` (the host is unchanged since 0.17.3).

- **Why only the left eye:** the game builds each frame's GPU work on a separate thread while the next tick already runs. After a
  left-eye frame comes the short right-eye tick, which re-aims the camera and rewrites the culling data for the right eye while
  the left frame is still being built, so grass and objects in the left eye were culled with right-eye data. The right-eye tick
  now waits until the left frame is finished (`render_sync`, 1). This can cost a few frames per second; the `POSE/s` log line
  shows how long it waits.
- **Left-eye stutter:** your eye position was taken from Jin's body before his animation for that tick ran, while the right eye
  used the animated body. Both eyes now use the same body pose.

## 0.17.3: the head is hidden, left-eye foliage pop-in, black edges on fast head movement (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host changed: 0.13.2; they must be the same build).

- **The head is gone in first person:** 0.17.2 hid joints in the skeleton copy that the game uses for gameplay, but the graphics card
  draws Jin from a second copy (the "render pose") that the game writes right after animating him. The mod now hides the head in that
  copy, right after Jin's animation and before the game sends it to the GPU, the same way the game hides armor pieces you don't wear.
  The whole head (face, hair, headgear on the head) collapses onto the neck. Your hands, the body and the gameplay skeleton are not
  touched. The head comes back in cutscenes, menus and when you press F5 for third person. It also has no shadow now.
- **Left eye pops grass/flowers/objects in and out when looking side to side:** the game's grass and foliage culling uses the previous
  frame. For the right eye that is the left eye of the same moment; for the left eye it is the right eye of the previous moment, so
  only the left eye's culling data is out of date when your head turns. The left eye now discards that data at much slower head turns
  (`fast_turn_deg_left`, 0.6; the right eye keeps 2.0).
- **Stutter and black edges when looking down and then up fast:** the headset program now predicts your head pose for the moment the
  picture really reaches the display (it measures that delay), instead of the moment it was asked for, so the picture needs much less
  warping. Windows also lets the headset streamer and GoTVRHost use the GPU before the game (`game_gpu_priority_below_normal`, 1).
  New log lines: `POSE/s` in gotvr.log and "pose lead" in host.log.

Please send `gotvr.log` and `host.log` again; the `FP/s` line should show "head hidden in" about as many jobs as "hero animation jobs".

## 0.17.2: first person engages again, left-eye pop-in fixed (xr=4)

**Install:** `xinput1_4.dll` (host unchanged since 0.17.1).

- **First person never turned on in 0.17.1:** the mod counted only the first 8 of Jin's joints, because the game keeps gear it
  doesn't show (hats, masks, unused weapons) as invisible joints in the middle of the skeleton, and the count stopped at the
  first one. They are now counted, so the whole skeleton is found; the head and the hidden-head set use only visible joints.
- **Objects popping in and out in the left eye when you turn your head fast:** the mod's fast-turn check (which stops the game
  from using stale culling data during fast turns) couldn't see head turns on left-eye frames. It now also measures the
  headset's own rotation.

Please send `gotvr.log` again; the lines starting with "first person:" show what it found.

## 0.17.1: first person at eye height, smoother head turns (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host changed: 0.13.1).

- **View at hip/chest height fixed:** the object the mod used for Jin was his "game logic" object, which only has a few attachment
  points (the highest at chest height), not his skeleton. The mod now finds Jin's actual body model and puts your eyes just below
  the top of his head. The log says which model it found ("skinned model ... joints").
- **Judder and black edges when turning your head:** the headset program was being starved of GPU time at 2560 x 2560, so
  Virtual Desktop dropped to half rate and showed old frames shifted (the black edges). Its copies now run at high GPU priority,
  and the default size is one step lower, **2304 x 2304** (F2 still cycles; `render_size` overrides).
- Tuning in `gotvr.ini` if the height is still off: `fp_eye_up_cm` (default -11 = 11 cm below the top of the head) and
  `fp_eye_forward_cm` (default 9).

Please send `gotvr.log` and `GoTVR\host.log` again (Shift+F5 once while standing still helps).

## 0.17.0: first person (first try) (xr=4)

**Install:** `xinput1_4.dll` (host unchanged since 0.16.2). The game must be in Windowed mode.

- **First person is on by default.** Your eyes sit where Jin's eyes are; the game's camera still turns you (right stick /
  mouse) and moves you, and your headset adds looking around and leaning on top.
- **No head bob:** the view follows Jin's body, not his animated head, so running and attacking don't shake the view. Climbing
  stairs, jumping and crouching follow smoothly.
- **Jin's head is hidden** in first person (his shadow has no head for now).
- **Third person comes back automatically** for cutscenes, the start screen, menus and videos, and whenever the game moves its
  camera far away from Jin (scripted shots).
- **F5** switches first person on/off. **Shift+F5** writes Jin's skeleton into the log (please press it once while standing still).
- Settings in `gotvr.ini` (all optional): `first_person`, `fp_eye_up_cm` / `fp_eye_forward_cm` (where your eyes sit relative to
  Jin's head), `fp_hide_head`, `fp_hide_radius_cm`, `fp_vertical_smoothing_s`, `fp_follow_lean`, `fp_max_camera_distance_m`.

Please tell me: is the view at the right height and position (not inside the chest, not floating in front)? Can you see
any part of the head, hair or hat? Does anything stretch or fly around when you raise your hands to your face (drinking,
wiping the sword)? Then send `gotvr.log`.

## 0.16.5: 2560 x 2560 is the default headset size (xr=4)

**Install:** `xinput1_4.dll` (host unchanged since 0.16.2). The game must be in Windowed mode.

- F2 works without crashing since 0.16.4. The highest size, **2560 x 2560 per eye**, is now the default
  (`render_size` in `gotvr.ini` still overrides it; F2 still cycles 1536 -> 1792 -> 2048 -> 2304 -> 2560 -> the game's own).

## 0.16.4: F2 "fatal error making a swap chain" fixed (xr=4)

**Install:** `xinput1_4.dll` (host unchanged since 0.16.2). The game must be in Windowed mode.

- F2 now reaches the game's own size-change path, and this time the game rebuilt its whole swapchain (the part of
  Windows that shows its picture). The mod was still holding on to the old one, and Windows allows only one per window,
  so the game could not create the new one. The mod no longer holds on to it, and lets go of everything that pointed to
  it just before the game rebuilds. The headset picture pauses for a moment and then comes back at the new size.

## 0.16.3: F2 crash fixed for real, videos no longer squashed (xr=4)

**Install:** `xinput1_4.dll` (the host `GoTVR\GoTVRHost.exe` is the same as 0.16.2). The game must be in Windowed mode.

- **F2 crash:** your crash dump showed the real cause. The mod asked the game to change size from the wrong place, so the game
  kept drawing while its screen buffers were being rebuilt. The game itself changes size from its main loop, with drawing
  paused. F2 now hands the change to the game's main loop at exactly that point (the same path the game uses when you resize its window).
- **Pre-rendered cutscenes squashed:** videos use a different size setting than the menus. They now fill the image the same way
  as the menus, so they show at 16:9 with their subtitles.

## 0.16.2: F2 crash fixed, HUD / menus / pause screen back to 16:9 (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host changed: GoTVRHost 0.13.0; keep them together).
**The game must be in Windowed display mode** for the square render.

- **F2 crash:** when the game rebuilt its swapchain for the new size, the mod still held pointers to the old (freed) back
  buffers and used one on the very next frame. The mod now lets go of them just before the game resizes, and picks up the
  new ones afterwards. F2 should now change the size in about half a second without crashing.
- **HUD, menus and pause screen square / stretched in height:** the game lays out its UI for the shape of the image, so a
  square image gave a square layout. The mod now tells the game "16:9" while the image stays square: the UI, menus, pause
  screen, videos and the start screen are drawn for 16:9, and the headset shows them at 16:9 again, like before. The 3D view
  in the headset is unchanged (each eye still uses the full square image).
- `ui_aspect` in `gotvr.ini` (new; default 1.7778 = 16:9, 0 = square like the image).
- Note: the desktop window shows the square image, so in menus it looks squeezed sideways there. That is only the monitor copy.

## 0.16.1: F2 freeze fixed (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).

- **F2 froze the game** because the mod asked the game to rebuild its render targets from the middle of a frame. It now
  asks the way the game does it for Alt+Enter: through the game window's message handling, between frames.
- **Your game is in fullscreen mode, where the square size cannot work** (the game then sizes itself to the monitor).
  Set *Display mode = Windowed* in the game's settings; the square 2048 x 2048 size is then used from the start, and F2
  changes it. In fullscreen F2 does nothing (the log says so).

## 0.16.0: square headset render size, adjustable (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).
**The game must be in Windowed display mode.**

- The game now renders each eye as a **square 2048 x 2048** image (it was a 2560 x 1440 picture stretched into the
  2560 x 1600 window, which wasted pixels sideways and lacked them vertically). The size no longer depends on the desktop
  window: the window on your monitor just shows a scaled copy.
- **F2** cycles the size: 1536 -> 1792 -> 2048 -> 2304 -> 2560 -> the game's own -> 1536. The change takes a moment
  (the game rebuilds its render targets). `render_size` in `gotvr.ini` sets the start size (0 = the game's own).
- A 720 x 720 desktop window is not needed: the headset size is now set separately, and a smaller window only saves the
  scaled monitor copy, which costs next to nothing.
- If the game keeps re-applying its size (stutter), the mod turns this off by itself and says so in the log.

## 0.15.6: after the pause menu the HUD is large for 1 s (was 3 s)

## 0.15.5: whole cutscenes with the large HUD, recentre at start, large HUD after pause (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).

- **Cutscenes:** the mod now follows the game's cutscene camera. Once bars (thin or thick) are seen, the HUD stays large
  for as long as the cutscene camera is in use - it keeps its own field of view for the whole scene - and goes back to
  the panel when the game hands the camera back to you. Thin-bar shots are also recognised directly from the camera.
- **Recentre:** automatic once the headset session starts and again when you first reach 3D gameplay.
- **Pause menu:** after closing it, the HUD is large for 3 s.

## 0.15.4: launcher visible, thick-bar cutscenes (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).

- **Launcher:** the headset session now starts only when the game draws its first frame, so while the launcher is up you
  see Virtual Desktop's (or SteamVR's) normal view of your desktop and can click Play. `host_start=0` restores the old way.
- **Thick (32:9) bar cutscenes:** the bars are not perfectly black (film grain), which made the detector lose them. It now
  accepts near-black bars and only returns to the small HUD after 3 s without bars.

## 0.15.3: cutscenes keep the large HUD, steady interact prompts (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).

- **Cutscenes:** in-engine cutscenes always have black bars in the 3D picture. Once they are seen the mod now treats it as
  a cutscene and keeps the HUD large for the whole scene, through dark shots and camera cuts, until there have been no bars
  for 1.5 s. (0.15.2 dropped back to the small panel whenever a shot was dark.)
- **Interact prompts no longer jitter:** prompts like "E to mount" are placed by the game where the object is on screen, and
  the mod renders left and right eyes on alternate frames - so the prompt moved by the eye distance every frame. The HUD
  image now comes from one eye only.

## 0.15.2: cinematic bars and fades (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).

- **Thin black bars** are part of the 3D picture, not the HUD, so the mod now also watches the 3D picture for them.
- **Thick black bars and fades to/from black** are part of the HUD; the detector now measures them as bands from the
  screen edges and accepts semi-transparent black (it was far too strict and never triggered).
- **Whenever bars or a HUD fade are present, the whole HUD goes large over your view and the small panel disappears.**
  When they are gone, the HUD returns to the stationary panel.
- The game's own fade to black now hides the HUD panel earlier (above 30 % instead of 50 %).

## 0.15.1: HUD back on its panel (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.15.0; keep them together).

- **The HUD was always huge in 0.15.0** because the new sharp layer never actually received the HUD: the game sets up
  its drawing target in one step that the mod was hooking one moment too late, so the HUD stayed in the 3D picture at
  full size. Fixed. The HUD (objectives etc.) should now be on the room-fixed panel, sharp; only cinematic bars and fades
  cover the whole view.
- **Safety net:** if the layer ever fails to receive the HUD for half a second, the mod falls back to the 0.14 panel by
  itself (the log says so). F1 switches between the sharp layer and the 0.14 panel.

## 0.15.0: sharp HUD on its own layer, cinematic bars and fades over the whole view (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the host changed; the two must match).

- **Sharp HUD:** the game now draws its HUD into a separate image instead of into the 3D picture. The headset shows it as
  its own flat panel at the HUD's full resolution (2560x1440), so it is no longer squeezed into the 3D image's pixels.
  The panel still stays put in the room and follows only when you look away (F4 switches to fixed-to-view).
- **Cinematic black bars and fades:** detected on that HUD image. During them the HUD image covers your whole view, so the
  bars sit at the top and bottom edges of what you see and fades cover everything. Subtitles are at the bottom of the view.
- **Stretch fix is now the default:** the letterbox crop starts off (F3 still toggles it).
- **F1:** switch the HUD between the new sharp layer and the old 0.14 panel, for A/B. `hud_layer=0` in `gotvr.ini`
  turns the new layer off for good.

Known limit: semi-transparent HUD parts may look a little too faint on the new layer. The log measures it, so this can be fixed next.
Next big step for overall sharpness: square render aspect (design doc).

## 0.14.5: second try at the stretch, start screen and loading screens (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe`.

The 0.14.4 log showed why nothing changed: the start-screen rule caught a half-second screen before the real start
screen, the loading screen you saw happens after the world has loaded (so it did not count as "loading"), and the
world-marker fix never triggered.

- **World stretching when you look up/down:** the most likely cause is a fix from 0.14.1 that cut 80 pixels off the top
  and bottom of the picture (assuming the game draws black bars there). If the game actually stretches its picture to
  fill the window, that cut magnifies the world vertically, which makes it swim exactly like you describe. The mod
  now measures whether black bars are really there and only cuts them if they are (see "letterbox check" in the log).
  Two A/B keys if it is still there: **F3** = cut off/on, **F4** = HUD panel fixed in the room / fixed to your view.
  Please tell me which combination looks right.
- **Start screen:** recognised by the game's fixed start-screen camera now -> on the still screen.
- **Loading screens:** whenever the game's screen is fully black (its own fade), it goes on the still screen, including
  the loading screen after a load.

## 0.14.4: world markers back in the world, start screen and loading screens on the still screen (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe`.

- **World things no longer move with the HUD:** markers, prompts and icons that sit on things in the world are drawn
  by the game together with the HUD; 0.14.3 squeezed them onto the HUD panel, so they stretched and moved with it when
  you looked up/down. They are now drawn where they belong in the 3D world (in stereo); only the real HUD goes on the
  panel. If you still see stretching, press **F3** (turns the letterbox crop off/on) and tell me which looks right.
- **Start screen:** shown on the still screen like the pause menu (fixed in the room, does not turn or follow).
- **Loading screens:** also on the still screen.
- ini: `menu_theater=0` / `loading_theater=0` switch those two back off.

Please test: does the world still stretch when you look up/down (also try F3)? Start screen and loading screens on the
still screen? Quest markers/prompts sitting in the world correctly? Send `gotvr.log` + `host.log`.

## 0.14.3: 3D back, HUD and start screen stay put in the room (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe`.

- **3D is back.** The texture I treated as "the HUD" turned out to be the game's finished picture with the HUD drawn on
  top of the 3D scene, so copying it put the whole game on the panel. The separate HUD layer is switched off for now
  (it needs a different technique); the HUD is drawn into the 3D picture on its panel again, as in 0.13, keeping
  0.14's fixes: full-screen fades and cinematic bars, the letterbox crop and the faster start.
- **HUD and start screen stay put:** the panel no longer moves with your head. It stays in place in the room (always
  upright, at the same distance if you lean or walk), so you can look around it. If you look more than 35° away for
  half a second, it glides back in front of you. F11 re-centres it immediately. `hud_follow=0` in the ini = old
  behaviour (fixed to your view); `hud_follow_deg` / `hud_follow_speed` tune the follow.

Please test: 3D normal again? Does the start screen menu stay put while you look around? Does the gameplay HUD feel
better staying put, or would you rather it stay fixed to your view (`hud_follow=0`)? Send `gotvr.log` + `host.log`.

## 0.14.2: black flicker and jumping HUD fixed (xr=4)

**Install both files:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (host unchanged since 0.14.1, but keep them together).

0.14.1 moved the HUD to its own layer by swapping the game's "draw the UI here" target on the fly. The game makes a new
such target reference for every draw batch, so the mod kept grabbing the wrong ones: the 3D picture went into the HUD
layer and was wiped (black), and the HUD jumped between full-screen and the small panel. 0.14.2 leaves the game's UI
drawing completely alone and simply copies the finished HUD into the layer once per frame (then clears the game's
copy so it isn't drawn twice). Fades still cover the whole view.

Please test the same as before: picture stable? HUD sharp and steady? Start screen still in the room? Cutscene bars
at the edges? Send `gotvr.log` + `host.log`. If anything is wrong with the HUD, `hud_layer=0` in `gotvr.ini` brings
back the 0.13 HUD while I look at the log.

## 0.14.1: frame rate back, start screen and sharp HUD for real (xr=4)

**Install both files again:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe`.

- **Frame rate:** 0.14.0 did a slow Windows memory check on every render-target switch the game makes (thousands per
  frame, on all render threads), which halved the frame rate. Removed.
- **Start screen / sharp HUD:** in your last run the game drew a 2560×1440 picture inside a 2560×1600 window, and
  0.14.0 refused the HUD layer because the sizes differed. The layer now uses the UI's own size, so the sharp HUD and
  the start screen that stays in the room actually switch on.
- **Letterboxed window:** that same 16:9-in-16:10 setting also put black bars into the 3D picture and squashed it a
  little vertically in the headset. The mod now sends only the picture part to the headset. (Setting the game back to
  2560×1440 windowed, as before, is also fine.)
- **Intro videos:** the headset program took ~30 s to start every time and the mod waited for it, so the intro videos
  were over before VR was live. It now starts in the background; the log says how long Windows took to start it.

Please test: frame rate back to normal? Start screen still in the room and sharp? Story videos on the screen? Send
`gotvr.log` + `host.log`.

## 0.14.0: sharp HUD, full-screen cinematics, start screen fixed in the room (xr=4)

**Install both files again:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe`.

- **Fades to/from black** now cover the whole view again (the game drew them into the HUD's rectangle).
- **Cinematic bars:** during letterboxed scenes the game's UI is drawn over the whole frame again, so the black bars
  sit at the top and bottom of your view, with the subtitles near the bottom bar, like on a monitor.
- **Sharper HUD and menus:** the HUD is no longer squeezed into the 3D picture. It goes to the headset as its own
  layer at the game's full 2560-pixel UI resolution (about twice as sharp), same place and size as before (F9 sizes).
- **Start screen:** its menu now stays still in the room (placed in front of you when it appears), with the 3D scene
  behind it. F11 re-places it.
- The monitor window shows no HUD while the HUD layer is on (the headset has it). `hud_layer=0` in the ini goes back to
  0.13's HUD; `hud_layer_premultiplied=0` if HUD edges look too bright/dark or translucent panels look wrong.

Please test:
1. Start screen: still in the room? Sharp? Does it switch to the normal HUD once you are in the game?
2. Gameplay HUD and prompts: sharper than before? Anything missing, doubled, too faint or too bright?
3. A cutscene with black bars: bars at the top/bottom of your view? Fades covering everything? Subtitles readable?
Send `gotvr.log` + `host.log` (look for `UI mode ->`, `UI layer:` and `UILAYER/s`).

## 0.13.0: pause menu on the still screen (xr=4)

**Install both files again:** `xinput1_4.dll` and `GoTVR\GoTVRHost.exe` (the protocol between them changed).

- **Pause menu, map, gear, options (every menu that pauses the game):** now shown like the videos, on the still screen
  in a black room, placed where you look when it opens. While it is open the game draws a normal flat picture (its
  own camera, right shape), so the menu and the paused scene behind it look exactly as on a monitor. Closing the menu
  goes straight back to 3D. F11 re-centres the screen. `theater_flat_render=0` puts the stereo image on the screen
  instead.
- **Logging for the next two steps** (no visible change): which texture the UI is drawn into (for the sharp UI layer)
  and which game states change when you open menus (to recognise the start screen).

Please test, in this order, and send `gotvr.log` + `host.log` afterwards:
1. Start the game and stay on the **start screen** for ~10 s (note whether it is on the still screen or follows you).
2. Load the game. Open the **pause menu**, wait 3 s, close it. Then the **map**, the **gear** menu, and **photo mode**,
   each the same way. Tell me the order you did them in.
3. Is the pause menu on the still screen, the right shape and sharp? Does 3D come back cleanly when you close it?

## 0.12.0: HUD and menus at the right size, videos on a screen, DLSS off (xr=4)

**Install both files this time:** `xinput1_4.dll` (game folder) and `GoTVR\GoTVRHost.exe` (the headset program
changed too; an old host refuses to start with the new DLL and says so in `host.log`).

- **DLSS is off by default; set the game to TAA.** In stereo the game renders two full frames per headset frame,
  so it is limited by the CPU and DLSS (which only saves GPU work) cannot speed it up, while a second DLSS for the
  right eye adds work. It also jitters because each eye only gets every other DLSS jitter step. `dlss_per_eye=1`
  in the ini brings the old experiment back.
- **HUD and menus:** the game draws its UI for a flat 16:10 screen, and that was spread over your whole lens (huge
  and stretched tall). The UI now lands on a flat panel in front of you at its real aspect ratio, 55° wide, about
  1.6 m away (both eyes converge on it, so it has a clear depth), slightly below eye level. **F9** cycles the panel
  size 40° → 55° → 70° → 85° → off. The world around it is still full stereo and tracks your head.
- **Pre-rendered videos (and loading/menu screens with no 3D world):** shown on a big flat screen that stays still
  in the room (3.2 m wide, 3 m away, black around it), placed where you look when the video starts. **F11**
  re-centres it (and your head tracking). **F10** cycles the screen mode: auto → always (everything on the screen,
  handy for menus) → never.
- ini keys (optional): `hud`, `hud_width_deg`, `hud_distance`, `hud_down_deg`, `theater`, `theater_distance`,
  `theater_width`, `theater_height`, `theater_backdrop` (1 = game world behind the screen), `theater_crop`
  (1 = cut a video's black bars away).

Please test and tell me:
1. Normal play: is the HUD readable and the right shape (circles round)? Does it fuse into one image (no double
   HUD)? Which F9 size feels best?
2. A pre-rendered video (e.g. the intro or a story video): does it appear on the still screen with the right shape,
   subtitles on it? Does normal 3D come back right after it?
3. Pause menu, map and inventory: readable on the panel? Anything left over around the panel, or anything that
   should have stayed full-screen?
Log lines: `UI:` (each switch between HUD panel and theatre screen), `UI/s` (every second), and `theatre:` in `host.log`.

## 0.11.1: per-eye DLSS, second attempt (xr=4)

The 0.11.0 log showed the game does not run DLSS through NVIDIA Streamline at all (the mod's hooks there saw zero
calls, which is why F9 did nothing): it talks to NVIDIA's DLSS library (NGX) directly. With one DLSS for both eyes,
DLSS blended the two views into each other, which is the jitter/wobble of the whole picture and of your head
position you saw. 0.11.1 hooks the direct DLSS calls and gives the right eye its own second DLSS instance (its own
history). F9 cycles: 1 own DLSS + own motion vectors (default), 2 own DLSS only, 0 shared (the jittery one).

Test with DLSS Quality: is the picture stable now (no wobble)? Then compare 1 and 2 for ghosting on moving things.
Log line `DLSS(NGX)/s` shows whether the right eye runs on its own DLSS ("right-eye on its own feature" ≈ frames).

## 0.11.0: per-eye DLSS (first version) + wider render margin (xr=4)

**Very fast turns:** each eye is now rendered 3 degrees wider than the lens on every side (the headset crops it), so
a very fast turn shows world that was actually rendered instead of an edge or culled objects. `fov_margin_deg` in
the ini (0-15) adjusts it; more = safer for fast turns, slightly less sharp.

**DLSS:** select DLSS in the game's graphics settings (any quality mode). DLSS keeps one history per "viewport"; the
mod now gives the right eye its own viewport, so each eye has its own DLSS history, plus that eye's own camera
motion and motion vectors (the same three parts that fixed the anti-aliasing). **F9** cycles:
1 history + camera + motion vectors (default), 2 history + camera, 0 off (both eyes share DLSS, like before).
Please test DLSS with 1 and 2: ghosting? smearing? one eye blurrier than the other? Then send the logs (`DLSS/s`
line: "right-eye viewport ready=1" means it is active; 0 means the game never gave the mod its DLSS options and both
eyes still share — tell me if you see 0). Known limit: each eye gets half of DLSS's jitter positions for now.

## 0.10.3: objects vanishing during FAST head turns (xr=4)

Each eye's culling uses that eye's previous frame. During a fast head turn that frame points somewhere quite
different, and the game wrongly hides objects that just came into view until you slow down. The mod now measures how
far each eye turned since its previous frame; above 2 degrees it handles the turn specially:

**F8** cycles how (log names it): **1** culling memory switched off for those frames (default; everything is
drawn — if the light flicker comes back, it would only be during fast turns), **2** the game's own culling memory
during the turn, **0** no special handling (= 0.10.2). Please try fast turns in 1 and 2 and tell me which is better.
`fast_turn_deg` (default 2.0, lower = kicks in earlier) and `fast_turn_mode` in the ini make it permanent. The log
line `OCC/s` shows "fast-turn frames" and the last turn angle.

## 0.10.2: distant shadows/objects vanishing when you look around (xr=4)

The game keeps a second copy of its camera that several systems use to decide what to draw far away (distant
objects, distant shadows). That copy still pointed where the GAME camera points, not where your head looks, so
things outside the game's view stayed invisible until you looked forward again (the modding guide's "cull camera
ownership" problem). The copy now follows your eye camera.

**F7** switches this on/off so you can compare (log: `cull camera follows head=1/0`); `cull_follows_head=0` in
the ini turns it off permanently. Please check: distant shadows/objects visible in every direction you look? And
does walking/running still go where you expect (if movement direction now follows your head instead of the
camera, tell me — that would mean a gameplay system reads the same copy)?

## 0.10.1: frame-rate collapse fixed (xr=4)

The per-eye anti-aliasing kept making new copies of the picture whenever the game switched between its two
internal copies (529 times in your session), and the old copies could not be freed safely fast enough — the graphics
memory filled up and the frame rate fell to 20. The mod now recognises the two as the same kind of picture and keeps
one pair of copies for the whole session (and if anything still tried to churn, it stops and falls back to the
game's anti-aliasing). Settings unchanged (1 = per-eye anti-aliasing). Send the logs (`recreations=` should stay 1).

## 0.10.0: per-eye anti-aliasing done (xr=4)

Your 0.9.9 test found the broken part: the per-eye "motion image" (setting 4 ghosted; 3 and 5 were clean). That part
is now gone, and setting 1 is what was setting 3: each eye's culling AND each eye's anti-aliasing (its own previous
picture and its own camera motion) use that eye's own previous frame.

F6: 1 per-eye culling + per-eye anti-aliasing (default), 2 per-eye culling + game anti-aliasing (the "great" one),
3 anti-aliasing reset, 4 everything cross-eye, 5 everything reset. Please confirm 1 has no ghosting and no faint
double edge on very close objects (compare with 2 if unsure). `history_mode=2` keeps the previous favourite.

## 0.9.9: the "great" setting is the default; per-eye anti-aliasing split into parts (xr=4)

Setting 1 is now the one you called great (per-eye culling, the game's own anti-aliasing). The per-eye
anti-aliasing has three parts (its own previous picture, its own camera motion numbers, its own motion image); the
0.9.8 log proves all three ran, so one of them is wrong. Settings 2-5 switch them on separately:
1. per-eye culling, game anti-aliasing (default, "great")
2. per-eye anti-aliasing, all three parts (0.9.8 setting 1, outlines left behind)
3. picture + camera numbers
4. picture + motion image
5. picture only
6. everything cross-eye, 7. everything reset

If you have a minute: look at 3, 4 and 5 while turning your head and tell me which (if any) has no outlines left
behind and no double edge. Then go back to 1. Send the logs (new `TAA camera:` line).

## 0.9.8: main-menu crash fixed (xr=4)

0.9.7 crashed the graphics driver about half a minute in: the game sometimes swaps the picture its anti-aliasing
keeps, and each time the mod threw away its own two copies and made new ones while the graphics card was still
using the old ones. Now the copies are kept (only replaced if the picture size/format changes, and old ones are
freed seconds later). The headset program also no longer writes a 46 MB log when the headset connection fails.
Install both files (`xinput1_4.dll`, `GoTVR\GoTVRHost.exe`). Settings and tests as in 0.9.7 below.

## 0.9.7: per-eye anti-aliasing, second attempt (xr=4)

The 0.9.6 log showed the "previous picture" swap still never ran (I pointed at the wrong part of the game's
record), while the per-eye camera motion did — the ghosts that moved with your head. Fixed, and the per-eye pieces
are now all-or-nothing: if this eye's previous picture is not in place, the game's normal behaviour runs (your
"effectively perfect" setting), so this mismatch cannot happen again.

F6 settings (renumbered; squares show the number):
1. per-eye culling + per-eye anti-aliasing (new default)
2. per-eye culling, normal anti-aliasing = the "effectively perfect" one (was 3)
3. per-eye culling, anti-aliasing reset
4. everything cross-eye
5. everything reset
`history_mode=2` in `GoTVR\gotvr.ini` makes 2 the start setting if you want to keep that one.

Test 1 against 2: ghosts gone? faint double edge on very close objects gone in 1? Send the logs (`TAA/s`:
"history restored" and "saved" should be about the frame rate).

## 0.9.6: shadows moving with your head fixed (xr=4)

The 0.9.5 log showed the per-eye anti-aliasing was only half on: the "previous picture" swap never ran (a lookup
bug), so the anti-aliasing lined up the OTHER eye's old picture using THIS eye's camera motion — things drifted as you
moved your head. Fixed. Also, the camera-motion image the anti-aliasing uses is shared with other effects (shadows),
so those now keep the normal one and only the anti-aliasing gets the per-eye version.

F6 settings (squares show the number): 1 per-eye culling + per-eye anti-aliasing (default), 2 same but the
motion image shared (diagnostic), 3 per-eye culling only (0.9.4), 4 per-eye culling + anti-aliasing reset,
5 everything cross-eye, 6 everything reset. Test setting 1: shadows still, double edge gone, no ghosting on moving
things. Send the logs (`TAA/s` must now show "history restored" and "saved" ≈ frames per second).

## 0.9.5: per-eye anti-aliasing, single image on the monitor (xr=4)

- **Double edge on close objects:** the anti-aliasing blends each frame with the previous frame, and the previous
  frame was the other eye (6 cm away, which matters most for close objects). It now gets this eye's own previous
  frame and this eye's own previous camera (the modding guide's "per-eye history bank"), like the culling in 0.9.4.
  Use the game's normal TAA anti-aliasing setting (not DLSS/FSR/XeSS) for this to apply.
- **Monitor:** the game window showed every frame the game rendered, and those alternate left eye / right eye, so you
  saw two views flickering over each other. The window now shows the left eye only (`monitor_left_eye=0` in the
  ini restores the old behaviour).

F6 settings (squares in the headset show the number):
1. per-eye culling + per-eye anti-aliasing (new default)
2. per-eye culling, cross-eye anti-aliasing (0.9.4 setting 1)
3. per-eye culling, anti-aliasing reset every frame (no double edge, more shimmer)
4. everything cross-eye
5. everything reset

Test setting 1: double edge on very close objects gone? Any new ghosting/smearing on moving things (NPCs, grass,
your horse), especially in one eye? Light flicker / pop-in still gone? Monitor shows one clean image? Send the logs
(`TAA/s` and `MONITOR/s` lines).

## 0.9.4: culling per eye without the light flicker (xr=4)

Your 0.9.3 results pinned it to one system: the game's occlusion culling (hiding objects behind other objects using
a depth picture of the PREVIOUS frame). That previous frame was the other eye, so things got hidden that this eye can
see (pop-in). Turning its memory off fixed the pop-in but caused the light flicker. 0.9.4 gives each eye its own
memory instead: the culling now uses this eye's own previous depth picture and this eye's own previous camera.

F6 now has 4 settings (squares in the headset show the number):
1. PER-EYE OCCLUSION (new default) — should have neither pop-in nor light flicker
2. occlusion memory off (0.9.3 setting 3: no pop-in, light flicker)
3. cross-eye occlusion (0.9.3 setting 5: good lights, pop-in)
4. everything reset

Test setting 1: (a) light flicker? (b) objects/shadows popping per eye? (c) the faint double edge on close
objects still there? Then compare with 2 and 3 if anything is off. Send the logs (`OCC/s` lines).

## 0.9.3: true pairs only, clearer test settings (xr=4)

**Install both files this time** (`xinput1_4.dll` and `GoTVR\GoTVRHost.exe`; the old host refuses the new DLL).

- **Pairs fixed:** the headset program used to show each eye's newest image on its own, so for a moment it could
  show the left eye of the next game moment next to the right eye of the previous one (the AFR mistake). It now
  only ever shows a left and right image that come from the same game moment.
- **You can see the setting now:** after each F6 press (and at start), a row of white squares appears in the
  middle of the view for 3 seconds. The number of squares is the setting number.
- Your 0.9.2 test showed that forcing the "visibility" group causes the light flicker, and leaving it alone causes
  the per-eye pop-in. That group had three parts, so they are now separate settings:
  1. both occlusion parts fresh per eye (new default)
  2. view occlusion only
  3. occlusion setup only
  4. light lists only (expected to flicker: this confirms the cause)
  5. everything keeps history (= 0.9.2 setting 4: good lighting, pop-in)
  6. everything reset

Test: for settings 1, 2, 3 tell me (a) light flicker yes/no, (b) objects popping in/out per eye yes/no. Send the logs.

## 0.9.2: objects in one eye only, light flashes, double edges (xr=4)

Your 0.9.1 comparison pinned it down (BUG-23 in `docs/BUGLOG.md`). The game asks one yes/no question, "did the
camera just cut?", in about 20 places, and each place needs a different answer in VR:
- **What is visible** (occlusion culling, light lists) reuses the last frame's view. That view was the OTHER eye,
  so things the other eye could not see got dropped from this eye → objects/shadows in one eye only. These now get
  "cut" every frame: each eye works out its own visibility.
- **Lighting / fog / exposure** build up over several frames. Resetting them every frame gave the flashing lights,
  so they now keep their history.
- **Anti-aliasing** keeps its history too (the faint double edge is the last known cost; a per-eye version is next).

**F6** now steps through 5 settings (the log names each one). Starting setting = 1:
1. HYBRID (new default): visibility fresh per eye, anti-aliasing + lighting keep history
2. HYBRID-B: like 1, but anti-aliasing reset (no double edge, but more shimmer)
3. HYBRID-C: like 1, but lighting reset
4. KEPT: everything keeps history (0.9.1 default)
5. RESET: everything reset (0.9.1 after F6)

Test: keep Motion Blur / Depth of Field off. Start with 1 and check for objects or shadows in one eye only, flashing
lights, double edges, frame rate. Then press F6 to go to 2 and 3 and compare. Tell me the best number and what is
still wrong in it. `history_mode=N` in `GoTVR\gotvr.ini` makes N the starting setting.

## 0.9.1: flicker and black streaks fix (xr=4)

Cause (BUG-22 in `docs/BUGLOG.md`): 0.9.0 told the game "the camera just cut" on every single frame, to keep the
two eyes' anti-aliasing apart. That leaves every effect that builds up over several frames stuck in its first frame
forever: anti-aliasing never accumulates (fine detail, grass, hair, fading objects flicker) and effects like
reflections, ambient shadowing and fog never converge (dark streaks). In xr=4 that reset is not needed: the game
already remembers the last frame's camera, which IS the other eye, and its history images are that same frame, so
it lines the two up correctly by itself (the modding guide's headset result: anti-aliasing is fine this way; the
one thing that breaks is MOTION BLUR, which would read the eye distance as movement).

1. In the game's graphics settings turn **Motion Blur OFF**, **Depth of Field OFF**, **Film Grain 0** (VR has no
   camera lens; motion blur breaks with two eyes).
2. Play with xr=4 as before. The log line `SYNC/s` now ends with `temporal history kept`.
3. **F6** switches between the new behaviour and the old one (`RESET every frame`) so you can compare in one
   session. Tell me which is better, and for the remaining issues whether you see them on the **monitor** too or
   **only in the headset** (that separates a game-rendering problem from the transfer to the headset).
   Things to look for with the new default: fine detail stable (no shimmer), streaks gone, and no faint
   double edge / ghost on close objects when you move your head sideways.

## 0.9.0: xr=4 — synchronized stereo (recommended)

The "render twice inside one frame" approach (xr=3) keeps hitting the same wall: this engine keeps one set of
per-frame state for a single view, so the second render inherits leftovers from the first (double images, the blue
tint, the black left eye). The modding guide documents the same wall in another big engine and the route that
avoids it: **render each eye as a normal, complete frame, and freeze the game for the right-eye frame**, so both
eyes show the exact same moment of the game (same positions of everything, same lighting). That is NOT alternate-
eye rendering (xr=2): in xr=2 the right eye was a later moment, which caused the ghosting on anything moving.

- Each eye gets a complete, normal frame → no blue tint, no double image, both eyes correct.
- The game runs at normal speed (it simulates once per eye pair).
- Each eye updates at half the game's frame rate (e.g. 110 fps → 55 per eye); the headset smooths head motion.
- (0.9.0 only) Anti-aliasing was reset every frame; 0.9.1 keeps it (see above).

Test: set `xr=4` in `GoTVR\gotvr.ini`, start VDXR/SteamVR, start the game, load a save, press F11 looking ahead.
Check: depth correct, each eye different (close one, then the other), no ghosting on moving things (grass, NPCs,
your horse), normal game speed, controls responsive. Send the logs (look for `SYNC/s` lines) and what you saw.

## 0.8.2: blue tint test that works while you play; F1 fixed

- **F1** showed two images because the game's anti-aliasing blended the moved second camera with the first
  camera's picture. F1 now turns that blending off while it is on, so you see only the moved camera.
- **F2** is new: it compares the first and second render of the SAME frame (they should be identical), so moving
  around doesn't matter. It checks the second render's steps one by one (~45 s), finds the one that adds the blue,
  makes the second render reuse the first render's result for that step, checks the fix, and SAVES it in
  gotvr.ini (`right_eye_reuse=`), so it's applied automatically from then on. You only run it once.

Tests:
1. **xr=0:** F5, then F1: the whole picture should move half a metre to the right, one image only. F1 again.
2. **xr=0:** press **F2** once and keep playing normally for ~1 minute (no menus/map). Then look: is the blue tint
   gone with F5 on? (The log says `F2 RESULT ...` and `fix confirmed and saved`.)
3. **Headset, xr=3:** both eyes should show the game with depth (left eye and right-eye fixes from 0.8.1 were not
   tested yet). F11 to recentre.

## 0.8.1: stereo fixes (right eye double image, left eye black)

- Right eye: the game draws everything relative to the camera position it had when it prepared the frame, so the
  second camera now keeps that position and gets the eye offset another way (inside its view matrix). F1 should
  now move the whole picture half a metre to the right, together.
- Left eye: the copy used the wrong game object (a settings block), so it never happened. Fixed; the copy is also
  checked against the image actually shown each frame.
- Blue tint: still there; F2 now tests only the steps that can be a real fix (26–58).

Tests, in this order:
1. **Monitor, xr=0:** F5, then F1: everything should move to the right together (near things more). F1 again.
2. **Monitor, xr=0, F2:** stand still at night, press F2, don't touch anything for ~2½ minutes (the picture will
   change by itself, including brief odd frames). At the end it applies what it found (F5 stays on).
3. **Headset, xr=3:** as before (F11 to recentre). Both eyes should show an image, with depth.
Then close the game and tell me what you saw.

## 0.8.0: native stereo (xr=3) — both eyes from the same game frame

`xr=3` renders every frame twice: left eye, then right eye, each with its own position and lens frustum, from the
same moment of the game. No alternating eyes. Expected limitations of this first version: the right eye is a bit
brighter/bluer at night (the known second-render issue, still being fixed), anti-aliasing is reset every frame
(more shimmer, no ghosting), the fps is lower than xr=2 (two full renders per frame), the HUD is still part of
the 3D image.

**Test 1 (monitor, xr=0, 1 minute):** press **F5**, then **F1**. The picture should jump as if you stepped half a
metre to the right: Jin and nearby things move more than far hills, everything moves together. Press F1 again (back).
Tell me: did EVERYTHING move together, or did some things (grass, Jin, effects, shadows) stay in place?

**Test 2 (headset, xr=3):** set `xr=3` in `GoTVR\gotvr.ini`, start VDXR/SteamVR, start the game, load a save,
press F11 looking straight ahead. Check: real depth (near things clearly closer), close one eye then the other
(different views, not swapped), smooth head turning (both eyes update every frame), anything wrong in one eye only.
Send `GoTVR\gotvr.log` and `GoTVR\host.log` (the `STEREO/s` lines show whether both eyes are captured).

## 0.7.8: automatic darkness test (F2)

0.7.7 made both renders draw exactly the same things (the log proves it), yet the second one is still darker, so
the cause is something that changes when the game renders twice, most likely the automatic exposure (the game
also tints dark scenes blue, which matches what you see). 0.7.8 measures the picture's brightness itself and
finds the cause automatically:
1. `xr=0`. Load a save, ideally at night, stand somewhere quiet (no wind-blown grass right in front, no NPCs).
2. Press **F2** once and don't touch anything for about **2 minutes** (the picture will switch between normal
   and dark by itself; F5 is controlled by the test). Pressing F2 again cancels.
3. When it's done the fix is applied automatically (F5 stays on). Look: is the picture now the same as F5 off?
   Then close the game and tell me; I read the results in the log (`F2 ...` lines, including fps per step).

## 0.7.7: F5 darkness fix (second attempt, now with proof)

Your F3 result showed the first render is fine and the second render (the one you see) is the dark one. The log
then showed why: some objects (probably lights/glows and a few effects) are drawn only into the first render. 0.7.7
also draws them into the second. **F4** switches this fix off/on (A/B). Test (`xr=0`, same spot):
1. Press **F5**: the picture should now look the same as without F5. Press **F10** once.
2. Press **F4** a few times: off = dark, on = normal. Tell me if it matches.
3. Note fps with F5 off and on. Close the game and tell me; I read the log myself.
(F3 still exists for testing; leave it on "complete", the default after start.)

## 0.7.6: F3 fixed (it blacked out the game)

0.7.5's F3 had a timing bug: pressing it at the wrong moment wiped part of the game's list of render steps, for
every frame and menu, until the game was restarted. That was GoTVR's fault, not yours. 0.7.6 keeps an untouched copy
of that list, restores from it, and checks the list every frame (and repairs it if anything is off). **Restart the
game** before testing. Then run the F3 test below exactly as described.

## 0.7.5: find what makes F5 darker (F3 test; use 0.7.6)

The 0.7.4 light-list idea was wrong (F4 changed nothing, so that's ruled out). 0.7.5 finds the cause by elimination:
**F3** makes the second render stop early, at a later point with each press. Test (`xr=0`, same spot as before):
1. Press **F5** (darker look). Press **F10** once.
2. Press **F3**, look 2–3 s: normal colours or dark? Note it. Repeat: F3 has 6 states, the log names each one
   (`skips passes 14..68`, `30..68`, `45..68`, `59..68`, `65..68`, `complete again`).
   Example answer: "14 normal, 30 normal, 45 dark, 59 dark, 65 dark, complete dark". fps for each also helps.
3. Send `GoTVR\gotvr.log` (after closing the game, or it will be the next session's log).

## 0.7.4: light-list fix for the F5 colour shift (did not help: ruled out)

The second render took the frame's list of local lights (torches, lanterns, fires) away from the first render, so
the first render had no local lights, and the anti-aliasing blended that darker picture into what you see. 0.7.4
gives both renders the same light list. **F4** switches this fix off/on while F5 is on, to compare (A/B).

Test (`xr=0`, monitor), ideally at night near lanterns or a campfire:
1. Note fps with F5 off. Press **F5**: colours should now look the same as without F5. Note fps.
2. Press **F4** a few times: off = the 0.7.3 darker look should come back, on = normal. (Tell me if you see no
   difference at all.)
3. Press **F10** once with F5 on. Play a few minutes, F5 off, then send `GoTVR\gotvr.log`.
If you can, also tell me your GPU usage % before F5 (e.g. from the NVIDIA/AMD overlay or Afterburner) and which
upscaler is set in the game options (DLSS / FSR / XeSS / TAA / DLAA).

## 0.7.3: second F5 freeze fixed

0.7.2 rendered the second copy once, then froze at the end of the frame. The engine hands its render jobs to its
job system with one function that goes over **all** of the frame's jobs from the start; running it again for
the second render counted the first render's jobs twice, so the frame waited for a count that could never come
out right. 0.7.3 hands over only the second render's own jobs. The test is the same as for 0.7.0 below.

## 0.7.2: F5 freeze fixed (second render now "activated" like the main one)

0.7.1 rendered the second copy once, then froze because the engine never switched the second view on, so
the frame waited for it forever. 0.7.2 switches it on. If it still freezes, wait ~5 s before killing the
game: the log then records where the engine is stuck (`R1 STALL` lines).

## 0.7.1: F5 crash fixed (first one), host exit crash fixed

0.7.0's F5 crashed because one per-frame engine table only has room for one render; 0.7.1 resets it between
the two renders. There may be more such tables further along: if F5 still crashes, the log now records exactly
where (`R1 FAULT` lines), so please just send `GoTVR\gotvr.log` again. The headset helper also no longer crashes
when the game exits.

## 0.7.0: the "render twice" test (F5) — step 1 of true same-frame stereo

True stereo means the game renders the full scene twice every frame (left eye, then right eye). 0.7.0 tests
whether the engine survives that, with both renders still using the same camera, so the picture should look
**unchanged**. Test on the monitor with `xr=0`:
1. Load a save outdoors. Note the fps (enable an fps counter, e.g. Steam overlay).
2. Press **F5**. Expected: the image looks the same, the fps drops to roughly half. Play/walk/ride ~5-10 min.
3. Press F5 again (back to normal).
Tell me: crash or not, any flicker/garbage/missing objects, the fps before/after. Send `GoTVR\gotvr.log` (and a
crash dump if it crashes). F5 only acts in gameplay (not menus).

## 0.6.3: ghosting test (xr=2)

The flicker/ghosting in xr=2 comes from the game's anti-aliasing history being shared by both eyes (each frame
is blended with the previous frame, which is the other eye). Test: in xr=2, look at Jin or grass, press **F6**
(toggles "reset history every frame"). Expected with F6 on: ghosting gone, image more jagged/shimmery. Tell me
which you saw, then press F6 again to turn it off. (`history_reset=1` in the ini makes the reset the default; xr=4 defaults to 0 since 0.9.1.)

## Install (over 0.5)

1. Replace `xinput1_4.dll` in the game folder with `install\xinput1_4.dll`.
2. Copy `install\GoTVR\GoTVRHost.exe` into the game's `GoTVR` folder (next to `openxr_loader.dll`).
3. **Delete `GoTVR\gotvr.ini`** once so the new options are written.

Game settings for VR: **Motion Blur off, Frame Generation off, HDR off**, Depth of Field off if available.
Start Virtual Desktop (runtime **VDXR**) or SteamVR before launching the game.

## Modes (`xr=` in `GoTVR\gotvr.ini`)

| xr | What you should see |
|---|---|
| 0 | flat game only (the mod just logs) |
| 9 | **headset test**: red left eye, blue right eye, white square in each. The game is untouched. |
| 1 | **mono 6DoF**: the game camera follows your head (turn, look, lean); the image is the same in both eyes. Turning your head should feel stable (the headset re-projects each image to where it was rendered). |
| 2 | **stereo 6DoF (alternate-eye)**: as 1, plus real depth. |

If Frame Generation is on, the mod detects it and refuses to send images (log: `FRAME GENERATION is on`).
Advanced ini options: `units_per_m` (world scale, default 100), `level_camera` (1 = your head owns pitch/roll),
`head_position` (0 = rotation only), `widen_tangents` (set 0 if lighting/shadows look wrong at the screen edges
in VR; send a log either way).

Keys: **F11** recentre (look straight ahead, press). F7–F10 are the old test keys (F8 = left-half test,
F9 = 5° turn), only useful with `xr=0`.

## Test order (please stop at the first one that fails and send the logs)

1. `xr=9` for a couple of minutes on the main menu and in game. **Does it still crash?**
2. `xr=1`: load a save, stand still, press F11. Turn your head slowly in all directions, lean, look at Jin.
   Then walk/ride for a few minutes, including one fast travel.
3. `xr=2`: same. Check: do near things (Jin, grass, fences) look closer than the hills? Close one eye, then
   the other: each should show a slightly different view, not swapped. Is the horizon level?

Expected limitations in 0.6 (not bugs): each eye updates at half the game frame rate (some judder, more on
fast head turns); some ghosting at object edges (temporal anti-aliasing sees the eyes alternate); the HUD and
menus look far away; the camera is still the third-person camera (first-person comes later); the right
stick still turns the game camera (use it gently).

Send after each run: `GoTVR\gotvr.log`, `GoTVR\host.log`, and the newest crash dump in
`%LOCALAPPDATA%\CrashDumps` if it crashed.

## Build

`build.sh` cross-builds `xinput1_4.dll` and `GoTVRHost.exe` with MinGW-w64 (MinHook source + OpenXR SDK
headers). Licences in `licenses/`. Self-test logs in `receipts/`.
