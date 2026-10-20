# GoTVR full-body IK (FBIK)

Status: stage 1 in 0.18.7 (body yaw, VR arms, hands); 0.18.9: hands flipped (grip +Y), stand tall (leg IK), arm ownership gate. Render-side, fail-closed, every part behind its own switch.

## Sources and credits
- VR modding playbook ch12 (torso calculations and ergonomics): body heading from the game while it moves; the head's yaw
  only past a dead zone; never inherit head roll / pitch into the torso; arms end at the calibrated GRIP pose; elbow pole
  biased down and out.
- bldbrne-vr (github.com/bldbrne-vr/bldbrne-vr), public body code and tests: write the body right after the game's pose copy
  so the game rebuilds what follows from it; facing dead zone 35 deg (15 moving), follow tau 0.25 s, settle 2 deg; torso
  dead zone 25 / max 75 deg; analytic two-bone IK with a body-space pole table; 10% max stretch; 1-cos arm blend; hands at
  eye anchor + (controller - head); both eyes of a pair use the same hand sample.
- FRIK (Fallout 4 VR body, rollingrock): the clavicle reaches toward far targets - reach fraction 0.15, downward damping 0.4
  (fArmShoulderReachFraction / fArmShoulderDownwardDamp), credited by bldbrne-vr.
- BetterVR (BotW for Cemu, Crementif): the elbow pole in body space, out to the side, down and back ((+-1, -1, -0.5) turned
  with the body), credited by bldbrne-vr.

## How it is built in Ghost of Tsushima (0.18.7)
- Write point: the hero's animation job (Anim_Job 0xB2DBC0) post-hook, the same place as the head hiding: the job has just
  written the MODEL-SPACE render pose that the skin palette job reads; GoTVR edits it there (and the joint WORLD matrices of
  what it moved, so held items - attached from the world array, RE 0x57E0A0 - follow). The next job rewrites everything:
  nothing accumulates. Order: head hidden -> body turned -> arms solved.
- Skeleton: the parent table is found at run time (int16 table reachable from the job's rig / anim object / node whose
  parents precede children and whose bone lengths fit the live pose); arm chains are found by shape (fbik::findArms). Fail
  closed: no table or not two chains -> animated arms, logged ("arms:" lines).
- Body yaw: moving - Jin's own facing, except running backwards (drawn facing your forward, 0.18.5); standing - follows the
  HEAD's yaw past 35 deg, tau 0.25 s, settles at 2 deg; off while riding (riding markers) and above 8 m/s.
- Arms: wrist target = controller grip pose composed exactly like the eyes (anchor + tracking offset), 7 cm behind the grip
  along the fingers; clavicle shrug for far targets; two-bone IK with the BetterVR pole turned with the drawn body; the hand
  takes the grip frame (fingers = grip -Y, thumb = grip -Z), the fingers keep their animated curl; out of reach the hand
  stops short (no stretch); 0.25 s 1-cos blend when tracking starts / stops. Trims: fp_hand_pitch/yaw/roll_deg,
  fp_hand_back_cm. Shift+F10 / fp_arms toggles.

## 0.18.9 additions
- Stand tall: shallow game crouches (the sword stance) are given back - render pose raised, legs re-planted by two-bone IK, eyes
  raised by the same amount; deep crouches (sneaking), riding, fast motion and animation-owned moments are left alone.
- Arm ownership: the game's animation owns the arms (eased 0.3 s) for the flute (until you move / dodge), takedowns / quick
  attack (2.5 s), and whenever the animation lifts a hand to head height (climbing, ledges, grappling).

## Next stages
1. Headset tuning of the hand frame (trims -> a live "grip adjust" like bldbrne-vr), arm ownership gate (attacks, climbing,
   ledges, takedowns keep the animation's arms with a blend; bldbrne-vr gates by animation class).
2. Torso twist (spine joints, 25 deg dead zone / 75 deg max), lean so the eye sits over the neck, duck.
3. Physical sword (combat rework) on the solved hand; near-camera body fade.

## Fingers (0.18.18, fingers.h)
References: bldbrne-vr core/vr/bloodborne_body.cpp (fingers start from the skeleton's reference pose; a grip = per-finger, per-joint
bend angles + spread + thumb opposition, tunable per grip kind, bend-sign tunable) and God of War VR source/src/fp/arms.cpp (MIT;
finger chains below the wrist, thumb vs long fingers, bend = relaxed + (max - relaxed) * curl about the joint's side axis, maxima
80 / 95 / 65 and thumb 30 / 45, index on the trigger, light squeeze = fist on Quest).
- Reference pose: the skin's bind pose, from the skinning palette's inverse-bind matrices (node+0x2F8 -> +0x660 -> +0xA8).
- Every frame after the arm IK: finger joints = IK hand x (bind hand^-1 x bind finger); other joints under the hand keep their
  animated offset; each finger link bends toward the controller palm normal by its curl.
- ini: fp_fingers, fp_grip_fist_full, fp_finger_index_trigger, fp_finger_bend_sign, fp_finger_relaxed_deg.

## Fingers 0.18.21: one hinge per finger, fist thumb, tween

- Each finger bends about ONE axis, analysed once from the straight bind finger: (knuckle -> end joint) x palm normal, in the hand's
  own frame. 0.18.18-20 recomputed the axis per joint from the already-bent bone; past ~80 deg that cross product vanishes and the
  middle joints bent sideways.
- Joints that bend: long fingers knuckle / middle / end joint (the metacarpal > 5.5 cm stays), 80 / 100 / 60 deg at full curl.
- Thumb: base / middle / end, 15 / 40 / 60 deg about thumb direction x (cos 15 across-the-palm + sin 15 palm normal): it lies over the
  curled index and middle, like a thumb in a fist.
- Curl values tween on a critically damped spring (fp_finger_tween_s 0.15). The future per-item grips (katana, bow, tools) only change
  the angle tables and curl targets.

## Forearm twist (0.18.23)

- A real wrist does not twist: pronation / supination is the radius rolling round the ulna, nothing at the elbow, all of it at the
  wrist. The IK aims the forearm rigidly, so the controller's roll sat in the wrist and the skin pinched.
- Twist joints: in the elbow's subtree but not the hand's, within 3 cm of the elbow -> wrist line, 15-105 % along it (Jin: 508 / 509 /
  510 at 25 / 50 / 75 %, 511 / 512 at the wrist).
- Each solve measures the hand's roll about the forearm before (animated) and after the hand takes the controller frame, against the
  elbow's own frame. Each twist joint turns about the forearm axis by (after - before) x its fraction x fp_forearm_twist (1.0).

## Hand stabilisation (0.18.24, stab.h)

- One Euro filter on each controller's grip pose (tracking space) before it becomes the wrist target: the cut-off rises with hand speed
  (position 1.2 Hz + 10 Hz per m/s, rotation 1.2 Hz + 3 Hz per rad/s), so a still hand is steady and a swing is not delayed.
- Resets (playbook ch02): first pose, tracking lost / regained, recenter, a > 25 cm jump in one frame, a bad time step, non-finite.
- ini: fp_hand_stab, fp_hand_stab_min_hz, fp_hand_stab_beta, fp_hand_stab_rot_min_hz, fp_hand_stab_rot_beta.
