# GoTVR roadmap (updated at 0.18.23)

Done: synchronized native stereo 6DoF (xr=4), per-eye occlusion/TAA history, square render with 16:9 UI, theatre screen,
HUD layer, first person anchored on the body with the head hidden in the render pose, pose-lead prediction, VR
controllers for any OpenXR controller (0.18.0: virtual pad, snap turn, item wheel logic, head gestures, rumble).

Rules for every phase (playbook ch03 / ch08): own one channel at a time, prove it flat (self-test / log) before the
headset, let the game's own systems do the work, keep every addition behind its own switch, and never ship a cost that
did not buy a measured fix.

## Phase 0 - accept first person and controllers (now)
- Streaks in the first eye of each pair (BUG-74): the game's Upscaling setting (DLSS/FSR/XeSS with one history for both
  eyes) has replaced its TAA since the 0.11 DLSS test; set it back to Off/TAA (0.18.7 logs AA/s and warns). Per-eye DLSS stays
  out (0.11-0.12 measured it jittery); revisit only with per-eye NGX features and same-phase jitter (playbook ch19).
- Camera policy (owner, 2026-10-19): first person on foot, on the horse, climbing and grappling; third person for
  cutscenes (done), photo mode and takedowns / assassination animations. Photo mode and takedowns need a state signal:
  the game's input-owner stack (names at 0x1417B5300: SneakAttack, PhotoMode, CutScene, HandsOff, ManualCamera ...) is the
  lead - find where the active owner is stored and hand first person off while SneakAttack / PhotoMode own input.
- Controls: confirm every binding in the headset (log lines "controls:" and INPUT/s), especially guiding wind (D-pad
  right + right stick up) and the wheel's D-pad outputs.

## Phase 1 - custom item wheel with the game's own art (owner design, CONTROL_SCHEME.txt)
- Left trigger opens the wheel; right stick picks; release the stick to confirm, release the trigger to close. Options:
  Heal, Photo mode, Call horse (0.18.0 has the logic and haptic ticks, but no picture yet).
- Draw it in GoTVRHost as its own layer anchored at the left hand, like bloodborne-vr does (its findings 17 and 26: the
  bridge renders the quick ring / weapon wheel itself on its own thread into a transparent layer, with the GAME'S OWN icon
  atlas and SDF menu font extracted from the user's install, and the pick becomes the game's own button presses).
- For Ghost of Tsushima: find the HUD/menu texture atlas and font the game loads (resource names in the strings table,
  the UI pass 60/61 draws), extract them at run time from the user's own files into the GoTVR folder cache (never into the
  repository - no game files in git), and render icons + labels with a small D3D12 UI renderer in the host.
- Read the game state the wheel needs (resolve / heal charges, equipped consumables) from memory, read-only and guarded;
  show counts; grey out what cannot be used.

## Phase 1b - body (0.18.5 / 0.18.6)
- Body drawn facing your forward when Jin runs back (FEAT-02: render pose + joint world matrices, so carried items follow).
- 0.18.7 FBIK stage 1 done (docs/FBIK.md): body follows the head past 35 deg standing; VR arms with clavicle reach, BetterVR
  pole, grip-frame hands, held items following. Next: headset tuning of the hands (live grip adjust), arm ownership gate for
  attacks / climbing / takedowns, torso twist and lean, then the physical sword.
- (design notes kept) a VR-owned body yaw, using the
  0.18.6 write point (render pose + joint world array turned after the hero's animation job). Playbook ch12 rules: body heading
  from the game while it moves; while standing, the body follows the HEAD's yaw only past a dead zone (~40-50 deg) and then
  turns smoothly to it (never immediate HMD yaw: looking aside must not swing the shoulders); once hands are tracked, both
  hands' positions vote on the torso direction (bilateral inference); never inherit head roll/pitch. Off while riding.

## Phase 2 - visible tracked hands and arms (ch12, HAND-005)
- The Anim_Job hook that hides the head is the write point for the arms: after the hero's animation, set the hand joints
  in the render pose from the controller GRIP poses (already in the shared block) and solve shoulder-elbow-wrist with a
  two-bone IK that uses the game's own elbow as the pole, biased down (FRIK clavicle reach, BetterVR body-space pole).
- Render-only first (the gameplay skeleton untouched); finger poses from grip / trigger / thumb rest (design doc).

## Phase 3 - combat rework (design doc COMBAT DESIGN; HAND-004, ch18) - NEXT
- 0.18.10: right grip unbound (the sword takes it); stand tall off (the player's own swings replace the game's stance).
- 0.18.11: each eye culled by its own frustum (preset 7); near plane 2 cm, consistent per tick; F1/F2/F3 hand trims saved to the ini.
- 0.18.12: snap turn steady during the catch-up; wrist point from the grip (Ctrl+F1-F3); near clip set in the environment; eyes 3 cm back.
- 0.18.13: focus-object stipple circle off in first person; idle animations held still while standing (fp_idle_hold).
- 0.18.14: standing stance squared (the culling apex change was removed in 0.18.15).
- 0.18.15: hips squared to the body follow while standing, after every snap; Shift+F7 TAA A/B for the arm overlay (no effect, removed).
- 0.18.16: body heading in world yaw (even dead zone), moving faces the headset, body under the headset (60 cm), smoother eye anchor.
- Later (bldbrne-vr core/vr/bloodborne_facing.h): real strafe / backpedal animations by making the game believe Jin is locked on to
  a point 50 m ahead along the VR heading (Bloodborne patches LockTgtMan's per-frame lock state) - GoT's stance strafing is the analogue;
  needs RE of GoT's lock-on / stance state. 0.18.16 turns the drawn pose instead (the legs run in the view's direction while he slides).
- 0.18.17: movement forward = headset forward at snap / start / menu exit / recenter; menus keep the view direction; threshold 45.
- 0.18.18: VR fingers (bind pose + per-finger curl; grip = fist, trigger = index). NEXT: grip kinds per held item (katana hilt, bow,
  tools) as per-finger/per-joint angle sets in the ini (bldbrne-vr grip.<kind>.f<finger>.<joint> + spread + oppose), chosen by
  what the hand holds (physical sword, Phase 3).
- 0.18.20: fingers with thumb / trigger touch; near clip back to 10 cm (grass drew over the arms at 2 cm). TODO: find the grass
  pass's projection so the 2 cm near can come back without grass over the arms.
- 0.18.21: forward only at move start; one hinge per finger (no sideways bends), fist thumb, finger tween; Shift+F6 screen-space
  shadows / AO test for the see-through arms (BUG-80). If it is screen-space shadows: keep them off in first person automatically, or
  find the pass and stop near pixels from marching into the background.
- 0.18.22: movement always follows the headset; body follows the head past 7.5 deg standing and moving.
- 0.18.23: AO held off in first person (BUG-80); arms through landings; forearm twist; headset recenter -> game recenter.
- NEXT for movement: instant direction changes = lock-on / stance strafing (bldbrne-vr bloodborne_facing.h: the game believes Jin is
  locked on to a point ahead along the VR heading) - needs RE of GoT's lock-on / stance state. Then AO could come back if its pass can
  be told to reject near pixels.
- Depth submission to the runtime (XR_KHR_composition_layer_depth) so Spacewarp can reproject near geometry (arms, body).
- Later: room-scale beyond 60 cm should move Jin himself (feed the offset to the game as movement), not only the drawn body.
- TODO (owner, 0.18.14): no attack HIT-REACTION animations in first person (Jin flinching / staggering when struck moves the body
  and the view) - to do with the physical sword / combat rework: detect the reaction (the idle hold's 'animation moving' signal is
  the hook) and keep the body / eye anchor steady, damage and feedback via haptics instead.
- Build order (playbook ch18 / HAND-004, bldbrne-vr's weapon-bone grip + live grip adjust): (1) the katana follows the VR right hand
  (it already rides the IK hand through the world matrices) with a grip-adjust trim; (2) a testable swing detector (blade tip
  speed, travel, cooldown, re-arm - the design doc's numbers, all ini) that emits the game's own light / heavy attack at the
  right moment and gates every synthetic input on the drawn sword; (3) draw / sheathe by grabbing at the left hip; (4) blocking /
  parry from blade-to-blade contact; then bow, kunai, blowgun.
- Sheathed katana on the left hip, drawn on rails; swings detected by a testable decision core (minimum speed, travel,
  cooldown, re-arm, contact tolerance - the design doc's parameters, all in the ini) emitting the game's own attacks;
  blocking / deflecting from blade-to-blade contact; bow over the left shoulder, arrows over the right; kunai on the wrist
  (throw release velocity from the hand's recent peak, HIGGS); blowgun on the chest; heavenly strike / dance of wrath from
  re-sheath + trigger / grip; tanto assassinations. Gate every synthetic input on the equipped item.
- Interim (0.18.0): right grip = block/parry (LB) so combat stays playable until the physical sword exists.

## Phase 4 - forearm HUD and gestures
- Health and resolve on the left forearm (the HUD layer already arrives as its own image); the full HUD with objectives
  when the left hand is held at the head (0.18.0 toggles the HUD panel; move it to the forearm/head split here).

## Phase 5 - polish
- Physical crouch (head height below a threshold = crouch), 8D audio, hand/head collision, VR settings panel, DLSS,
  smaller desktop window, culling improvements (design doc TECHNICAL / Performance).
