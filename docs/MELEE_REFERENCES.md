# Physical melee in existing VR mods: notes for the GoT katana mod

Clones: `/home/claude/bldbrne-vr/bldbrne-vr` (BB), `.../scratchpad/refs/gowvr` (GoW), `.../scratchpad/refs/bettervr` (BotW).
Note: BB's swing classifier itself (`core/vr/swing_detector.h`, `melee_window.h`) is a 6-line stub that includes a **private DLL** (`physical_interactions/...`), so its numbers below come from `melee_settings.h`, `vr_session.h`, `bloodborne_melee.cpp` and the tests (`tests/test_melee_window.cpp`, `test_swing_arc.cpp`).

## 1. Bloodborne VR (shadPS4 fork): the closest match to your design
**Animations are suppressed.** `melee=physical` means "the swing is the attack (no attack animation; the game's own attack on the real blade while it moves fast)". The mod calls `BehaviorModule::InvokeAttack(module, bid, type, slot, flags)` directly (`bloodborne_melee.cpp:70-82`). It uses private attack slots 6 and 7 (the game's TAE uses 0-2) and refreshes the slot every frame while the window is open. The engine retires a slot that stops getting refreshed.
- **Hit shape:** the game's own attack capsule hangs from weapon dummies (`DmyPoly` 300 = R_Weapon), and those dummies follow the VR-posed weapon. The capsule radius is multiplied by `radius_scale=0.7` and fitted to the blade model plus a 10 cm margin. Without that, the game's 0.3-0.5 m capsule "hit 40 cm from the visible blade". The hit entry stores the current and previous end positions (`+0x210/+0x230`), which gives a swept capsule. Damage and multi-hit bookkeeping stay with the game's attack handle, so each window can hit a target once.
- **Window (tests):** the onset opens the window when the stroke starts and it must pass the gate within **0.15 s** or it aborts. A typical fast stroke is blade 8 rad/s, hand 3 m/s, **tip 9 m/s**. The window closes about 50 ms after the speed drops below the keep rate, or after a maximum of **0.6 s**. A hit still counts up to `HitGraceNs=120 ms` after the close. With the budget off, counted swings are at least **0.35 s** apart.
- **Heavy:** `heavy_arc_deg=140` blade sweep from a wound-up start (`heavy_windup_deg=100` from forward). Holding the trigger for `charge_s=1.0` gives a charged swing (0.35 s gives a heavy). Wind-up splitting uses `swing_windup_turn_deg=25`. A hand passing the face only counts once it is under 1.5 m/s.
- **Velocity basis:** the blade **tip** velocity is computed from two pose ticks (`BladeTickMaxNs=100 ms`). Ticks taken across a snap turn are discarded. The tests reject pose glitches above 35 rad/s and score head-relative speed when the headset re-localizes.
- **Damage:** a hook on the final-damage function multiplies damage by `DamageFactor(tip)`, **0.75 at 3 m/s up to 1.10 at 12 m/s**. A cadence factor (never below 0.5) keeps flailing DPS at or below vanilla × 1.10. **Combo budget:** 2 charges per weapon (light costs 1, heavy 2, charged all of them). Charges refill after the move's TAE recovery minus 80 ms. With none left the swing does not register (dull buzz). The map-hit (wall bounce) function is detoured to suppress the bounce while keeping sparks and sound.
- **AI fairness:** heavy or wide swings call `NotifyAttack` (vt+0x440), so the AI can step or guard. `ParryTiming` (vt+0x448) lets parrying enemies time their parry. Quick flicks are never telegraphed (`telegraph_arc_deg=60`, `telegraph_long_s=0.40`).
- **Parry:** gun shot (vanilla). There is no weapon-vs-weapon deflect.
- **Haptics and feel:** a hit gives full amplitude for `haptic_melee_hit_ms=65` (+15 ms × weight), then a decay of about 150-170 ms in total. The hit is guarded for 200 ms so other buzzes can't cut it. Heavy hits add a "crunch" 40 ms later. Quest motors need a floor of 0.3 amplitude and 25 ms. A **"bite"** makes the hand lag the controller briefly on a hit, and the game's hitstop (row value, default 0.09 s) is kept. Swinging also plays the whoosh sound and the blade trail FFX (`TrailFfx=250`). Stamina: light swings cost 80% of the game's cost.

## 2. God of War VR (MIT): gesture → button press, animation kept but frozen
`source/src/input/vrmap.cpp:85-140, 400-480`:
- **Velocity:** the **hand (grip) position in room space**, differenced over about 60 ms of fresh samples. It also tracks path length over 200 ms and `along` (velocity · controller −Z).
```cpp
swingSpeed = config "swing_speed" 2.8f; swingTravel = "swing_travel" 0.32f;
swingGate = s.armed && now >= s.cooldownUntil && s.speed > minSpeed && s.path > minTravel &&
            (s.along > 0.3f || downward);                // downward = vy < -0.6*speed
if (s.speed < 1.2f) ...; if (slowSince && now - slowSince >= 80) armed = true; // rearm
// or rearm by reversing (>0.6 m/s, dot(fireDir) < -0.2, >70 ms after fire)
returnMs = 700  // motion against the last swing within 700 ms = arm returning, never an attack
lightCool = "swing_cooldown_ms" 300; heavy 300
```
- **Heavy:** a chop (speed > 1.35× the threshold, downward, path > 1.25× the travel) or a swing with the trigger held. The optional `heavy_by_strength` mode waits up to 80 ms after the gate to see the peak, and a peak of ≥ 6.0 m/s (fists 5.5) gives R2.
- **Delivery:** `QueueAttack` → a **50 ms R1/R2 press** into the XInput funnel. The press waits until the previous attack ends plus `combo_gap_ms=60`, so every swing starts a combo's first attack. The attack animation plays at **2× time scale** with the body pose frozen (HOLD). A queued swing fast-forwards the rest of the current attack at 20×, but only after 0.5 s of game time, so the hit window isn't skipped.
- **Hits** (`fp/combat.cpp:159-470`): the mod hooks the game's sphere-sweep volume test (prev → cur frame). While Kratos is the attacker, each sphere radius is raised to `hit_radius=1.0 m` (fists `0.1`). When the game misses, it **re-tests with the spheres laid along the drawn axe grip→head (this frame and last)**, because the game's spheres follow animation joints, not the VR weapon. Damage, reaction and hit pause are all the game's.
- **Lunge:** the drawn body and view stay put. The game's invisible position may follow the lunge up to `attack_reach=2.5 m` (fists 0.3) and snaps back afterwards.
- **Block:** left grip = L1 (shield) with a 150 ms delay. Parry is the game's own L1 timing.
- **Haptics:** on fire, light `XrHaptic(1, 0.6, 0.05 s)`, heavy `(0.9, 0.09 s)`, charge `(0.4, 0.12 s)`. Hit rumble comes from the game.

## 3. BotW BetterVR (Cemu): game's attack sensor armed by motion
`src/hooking/weapon.h:110-530`, `weapon.cpp:401-528`:
- **Velocity:** the runtime's `XrSpaceVelocity`, converted into the **controller-local frame**. Slashes use angular velocity in the controller XY plane (twist about Z removed). Stabs use linear velocity along −Z.
```cpp
slashVelocityThreshold 5.0 (Normal 7.0) rad/s  | slashAccThreshold 14 (20) rad/s^2
slashSpeedThreshold 1.0 (1.5) rad/s to stay alive | slashTravelAngle 30 (36) deg before active
stabVelocityThreshold 1.0 m/s, stabTravelDistance 0.12 m * armLen/0.55, 45-60 deg cone
minGoodSwingDuration 25 (40) ms, maxBadDuration 33 (22) ms, grace 80 (40) ms
refractory 250 ms after an active attack ends
```
- **Hit:** the weapon matrix is overridden with the VR pose (`hook_ChangeWeaponMtx`). While `IsAttacking()`, the weapon's **AttackSensor** is set `mode=2`. Arming bumps the attack id, which wipes the game's per-attack hit list. `OneHitPerSwing=true` arms only on the rising edge. Otherwise the sensor re-arms every frame with a 0.5× damage multiplier.
- **Damage:** `0.20 + 1.15*smoothstep((power-0.15)/0.85)`, where power = peak angular rate / 10 rad/s (stab: peak / 2 m/s × straightness factor 0.7-1). Two-handed grip ×1.2, one-handed large sword ×0.85.
- **Parry:** guard is held while the left (shield) hand is at chest height. A spike above **35 m/s²** in left-hand linear acceleration presses A (the game's parry).
- **Haptics:** continuous while attacking: amplitude 0.7·v, frequency 0.5·v, where v = max(handSpeed − 2.0, power × 1.4).
- **Bow:** the trigger presses ZR (the game's draw). The draw engages only if the right hand is within **0.35 m** of the nock point, and pull = (separation − engage separation) / **0.30 m**. The game's draw amount sets launch speed (`firstSpeed·draw²`). Releasing the trigger fires, and the mod overrides the shot direction and target.

## Recommendations for a katana mod
1. Detect on **blade-tip velocity** (grip pose + 0.75-0.9 m blade offset), relative to the play space. Drop samples across snap turns and anything above 35 rad/s. Open at a tip speed of about **4-5 m/s** (≈ hand 2.5-3 m/s) and require **≥ 30° of blade sweep** or **≥ 0.25-0.32 m** of hand travel to commit.
2. Open the hit window at onset and abort it if not committed within **150 ms**. Close it about **50 ms** after the tip drops below ~2 m/s, with a hard cap of **0.6 s**. Accept hits up to **120 ms** after the close.
3. Use a **swept capsule** (base→tip, previous frame to current frame) about 4-6 cm in radius along the visible blade. Don't trust the game's animation-attached hitboxes, which is the lesson from GoW.
4. Rearm after **80 ms below 1.2 m/s** or a direction reversal. Ignore anything moving against the last swing within **~500-700 ms** (the return to guard). Allow **one hit per enemy per window**.
5. Pace swings with a budget of **2 charges** refilling at about the game's own light-attack recovery (~0.35-0.57 s), or an equivalent cadence factor. That keeps DPS at or below vanilla × 1.1.
6. Scale damage by tip speed: **×0.75 at 3 m/s to ×1.1 at 12 m/s**, or BotW's 0.2-1.35 smoothstep if light taps should be weak. Use **×1.2 for a two-handed grip**.
7. Classify heavy by **arc ≥ 140°** from a wound-up start (≥ 100° from forward) or a **peak tip speed ≥ 6 m/s within 80 ms** of the gate.
8. Deliver damage through the game's own hit/attack function so stagger, posture, hitstop and sounds come for free (the BB and BotW approach). Fall back to button injection only for special moves.
9. For parry, test **blade vs enemy-weapon capsule contact** during the enemy's active frames, with a ~150-200 ms perfect window. Then call the game's parry/deflect routine. None of the three mods implements weapon-vs-weapon contact, so you will need to build it. BotW's 35 m/s² hand-acceleration spike is a cheap fallback.
10. Haptics: on a hit, amplitude **1.0 for 65-80 ms**, then a decay to ~160 ms, protected for 200 ms. Add a second pulse **40 ms** later on heavies. On commit, a 0.6 tick for 50 ms. Keep amplitude ≥ 0.3 and duration ≥ 25 ms for Quest motors.
11. Add a short **"bite"** (blade lags the controller 50-100 ms on contact) plus the game's hitstop (~0.09 s). Never lunge the camera. If the game needs a lunge for reach, let only the invisible root move (≤ 0.3-0.5 m) and snap it back.
12. Notify the enemy AI for heavy or wide swings only (BB's telegraph), so enemies still block and dodge but quick flicks stay unreadable.

## Weapon weight (0.18.58)

- **Blade & Sorcery:** the held weapon is joined to the hand by a spring (position and rotation) that pulls it back to the
  hand and a damper that gives it inertia, so a heavy weapon builds speed slowly and is hard to stop; the desync between
  the real and the VR hand is the intended feel, and players tune spring / damper (one reported 1.3 / 0.35). Weapons are
  deliberately not 1:1 with real masses. Source: Steam community discussion "weapon weight"
  (https://steamcommunity.com/app/629730/discussions/2/3362406825551877390).
- **Playbook ch02:** simulated weapon weight is a follow filter; the shippable part is the enumerated reset list (FEAR VR's
  WeaponWeightResetReason); order: controller pose → two-handed solve → weight → (recoil) → collision; a hard clamp in a
  tracked path reads as a wall, compress asymptotically instead.
- GoTVR: attack.h Weight = rotational spring-damper on the blade's forward, gravity sag one-handed, a second hand = a
  second joint (a third of the lag, a fifth of the sag), soft lag limit, reset list in gotvr.cpp.

