# GoTVR roadmap (updated at 0.18.26)

Done: synchronized native stereo 6DoF (xr=4), per-eye occlusion/TAA history, square render with 16:9 UI, theatre screen,
HUD layer, first person anchored on the body with the head hidden in the render pose, pose-lead prediction, VR
controllers for any OpenXR controller (0.18.0: virtual pad, snap turn, item wheel logic, head gestures, rumble).

## CURRENT TASK LIST (reset at 0.18.26 - VR IK + 3D audio done, combat begins; updated 0.18.46)

1. Katana (0.18.41): RE target from the flat-play tags - the game's auto-sheathe timer (keep its sword out with no input) and the hit
   path for physical combat. 0.18.40: player-driven - the game's DRAW may run in the background (masked), its SHEATHE never (owner, firm); the game's sword
   stays drawn and is put home by hand. Check the real-mouth catch, the sticky grip, the drawn-stance legs while walking. Earlier: (0.18.39) the game's draw only
   (owner: no visual effect on Jin). Check: the scabbard place learned on the first draw, the mask covering the whole animation, walking with
   the game drawn (its drawn-stance legs), the 2.5 s toggle guard. Check: the katana stays in the fist
   while moving, the hands' places (Ctrl+PageUp / PageDown / Home / End), the body under the headset. Open: the see-through copy of the
   arm / katana per eye (the 0.18.37 per-eye TAA did not help - look for the cause elsewhere: velocity / previous-bone matrices of the
   edited pose, not a costly TAA change). Earlier (0.18.33):
   Katana in the headset: capture both grips (Ctrl+F5 / Ctrl+F6), check the drawn katana along the controller, two
   hands. Then (0.18.32): blade along the controller, lower grip, left hand up the sheath, two hands; Ctrl+F9 census of
   the game's draw (where its separate drawn blade lives). Earlier list (0.18.31):
   - check the rail from the two joints (hands on the drawn hilt / sheath), the blade-forward wrist, and the body held while reaching
   - blade probe with 592 held still (Ctrl+F7 / Ctrl+F8)
   - if the sword and sheath are ONE mesh: the separate drawn blade must come from the game's own in-hand katana. Find the hand-socket
     joint and the model swap the game's draw makes (flat-play observer + a game draw), then show that model with our physics
     instead of the game's draw animation.
   Logs: SWORD/s, katana:, UNDER/s.
2. Katana stage 2: two-handed grip (left hand on the tsuka behind the right, HAND-017), a grip-adjust trim, dropping / throwing
   instead of auto-return, the blade following the saya's curve (sori), blood flick (chiburi) gesture.
3. PHYSICAL combat (owner, 0.18.29: "free flowing combat, a new physical base system, all the game's attack animations disabled"):
   a. Blade segment every frame (guard -> tip, done in 0.18.29 for sheathing) swept against enemy bodies: needs the enemies' joint
      positions (the same skeleton / pose path as Jin's: the render pose per character) -> capsules per bone.
   b. (0.18.43: HandleHit 0x694080 / SetHealth 0x69F3D0 found and logged - docs/COMBAT_FINDINGS.md; next: the hit record's damage /
      attacker fields from a tagged flat session, then a captured template replayed on contact.) Damage without the game's attack animations: RE the game's hit path (what a sword attack's hit window calls: hit / damage /
      stagger / dismember events) and call it with our hit (attacker Jin, victim, bone, impulse, our tip speed as strength).
      Fail closed until found; flat-play observer logs of a normal hit help find it.
   c. Blocking / parry from blade contact with the enemy's weapon joint; stagger windows; enemy attacks still the game's.
   d. Stance / Ghost tools stay the game's (bow, kunai) behind their own bindings.
3b. Traversal (owner 0.18.44): ledges / ropes / logs / beams = first person with Jin's own animation owning the body and arms (a
   traversal flag from the Ctrl+numpad 9 capture); wall climbing likewise; grapple prompt in the air.
4. Hit reactions in first person: keep the body / eye anchor steady, damage as haptics (owner TODO 0.18.14).
5. Shadows / ghosting around the hands (owner 0.18.25 / 0.18.37 photo: a copy of the arm and katana offset differently per eye; per-eye TAA did NOT fix it, removed in 0.18.38): right-eye object motion vectors vs per-eye history, or
   screen-space shadows on near geometry.
6. Camera policy for takedowns / photo mode (input-owner stack, below) - the flat-play observer (play_mode=flat) logs help.
7. Audio stage 3: HRTF (Wwise binaural / Steam Audio plug-in if shipped), positional UI sounds.
8. Room-scale that moves the real character (off since 0.18.26; needs the character controller's position write, not the stick).
9. Later: bow, kunai, blowgun; climbing / grappling hands; mounted combat.

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
- 0.18.24: AO / SSS through the game's apply (BUG-80); hand stabilisation; room-scale via the stick (Jin's collider follows you);
  flat play observer (play_mode=flat) to find climbing / takedown / stance flags from the owner's logs.
- NEXT room-scale: an instant version needs the character controller's position write (RE: who moves the hero's collider); the stick
  drive is the safe first step.
- 0.18.25: 3D audio phase 1 (listener = head). NEXT audio: Jin's own emitters (voice, sword, gear) centred in front of the head
  (God of War VR D-162: after the game's emitter update, one more SetPosition 0.25 m ahead of the listener for the player's own
  emitters) - needs GoT's emitter update / owner RE (Wwise SetPosition 0xF12BF0 callers); then HRTF (playbook #hrtf-in-the-mixer:
  Wwise's own binaural / a Steam Audio plug-in if the game ships one).
- Then: combat rework (physical sword).
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
- 0.18.47 COMBAT STAGE 1 (live test pending):
  - The blade's contact with proven-hostile enemies (melee.h: closing speed, swept tip, re-arm, cooldown) sends the game's own
    hit message, copied from Jin's real blow, straight into HandleHit. Speed-scaled damage, game-style killing blows, rumble.
  - Damage is delivered through the game's hit function (MELEE_REFERENCES.md rec. 8), not by pressing the attack button.
- NEXT, combat stage 1b:
  1. Verify live in flat: Ctrl+Shift+H / K / J, reactions, nothing kept in the pool.
  2. Read the +0x170 payload and fill in the real contact point and direction.
  3. Hit-stop and blade bite (BB 0.09 s; blade lags 50-100 ms on contact).
  4. The guarded variant (find the enemy's guard state → 0x19C00841 chip).
  5. Swing charges (2, refilled at the game's recovery) so flailing never beats vanilla DPS.
  6. Notify the AI on wide / heavy swings so enemies can block / dodge.
- NEXT, combat stage 2:
  1. Heavy = right trigger held while charging two-handed (template from one game heavy, flag 0x100).
  2. Lunge (invisible root ≤ 0.5 m, snapped back).
  3. Block / parry: blade against the enemy weapon during the wind-up, pressing the game's block; the game's parry window decides.
  4. Parry variants 1 / 2.
  5. Standoff on the left trigger.
  6. Finisher on crawlers only (0x2000).

## Phase 4 - forearm HUD and gestures
- Health and resolve on the left forearm, plus the full HUD at the head. Plan: docs/WRIST_HUD.md (after the bldbrne-vr Blood
  Gauge and playbook ch04). The decision core src/vitals.h is done and self-tested.
  - W0: find resolve (follow the heal message's caller; the binary's resolve stat names).
  - W1: the game's own health / resolve art cropped from the UI layer onto a quad on the left wrist; glance to show, forced
    on at low health; the full HUD at the head.
  - W2: our own gauge from the values, plus the low-health heartbeat rumble.
  - W3: placed on Jin's sleeve, with occlusion.

## Phase 5 - polish
- Physical crouch (head height below a threshold = crouch), 8D audio, hand/head collision, VR settings panel, DLSS,
  smaller desktop window, culling improvements (design doc TECHNICAL / Performance).

## Standing references (owner: use them without being asked; the VR modding playbook first)
- VR modding playbook (the project docs): patterns HAND-004 / 005 / 012 / 013 / 017, INPUT-010, ch02 guided insertion, ch14 hazards.
- BotW-BetterVR (https://github.com/Crementif/BotW-BetterVR), owner 0.18.45:
  - the BOW (draw / nock / release from tracked hands) - read before building archery;
  - its body-space elbow pole (already used);
  - and generally (weapons, hands, first-person body).
- God of War VR mod (https://github.com/adore11s/God-of-War-VR-Mod, MIT): combat design, general.
- bldbrne-vr (https://github.com/bldbrne-vr/bldbrne-vr): full-body IK, combat design, UI.
- bloodborne-vr (https://github.com/laughtt/bloodborne-vr): the weapon wheel.
- FRIK (Fallout 4 VR body): clavicle reach. HIGGS / SteamVR Interaction System: throw release velocity (kunai).
- fear-vr: docs only.

## Notes (2026-10-22, owner)
- Archery: the left trigger is also the bow's slow-down time while holding the bow (LT = Focus only when nothing is held).
- Wrist: HUD on the outside / top of the left forearm (game art only, locked to the forearm, no wrist roll); kunai on the inner forearm.
- Hit feedback: the game's hit UI shown full view for now; a custom red vignette later.

