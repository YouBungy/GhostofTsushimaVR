# Wrist HUD: health and resolve on Jin's left forearm

The design doc ([UI]) asks for three things:

- "Health bar and resolve is on left forearm, very diegetic."
- "Holding left hand up to head shows full in-game HUD, including objectives."
- [REFERENCES] "Rumble feedback low health": a phased rumble when health is low.

This is the roadmap and framework for building it. The decision core already exists: `src/vitals.h` is self-tested and in
the build, but not yet wired in.

## What the references do

**bldbrne-vr, the Blood Gauge** (`core/vr/bloodborne_vitals.cpp`, `vitals_channel.h`, `tests/test_vitals.cpp`):

- **Data.** One game-thread site reads HP, max HP, the rally pool, stamina and bullets from the game's own data module
  (signature-checked) and publishes them through a single-writer seqlock to the render side.
- **Placement.** The gauge sits on the hunter's left forearm, placed every frame from the game's final pose: two dummies on
  the hand and one on the forearm. It is fitted to whatever he wears using an in-frame depth probe, then drawn into the game's
  frame with the game's camera, light and depth. A sleeve or cuff never covers it, but a hand or weapon in front still does.
- **Behaviour** (its test list, which ours mirrors):
  - fills as fractions with guards;
  - change edges (hit / rally / heal / stamina / shot / max-HP), so a level-up is not a heal;
  - a heartbeat below 30 % HP: 60 bpm at the line rising to about 130, a lub-dub envelope, felt only within 20 s of the last
    HP lost;
  - dark view edges below 60 %;
  - glance-to-show: it appears after 0.12 s of facing it and hides 0.25 s after looking away;
  - digits that turn upright.
- **Fallback.** "Wrist works" needs a fresh status and an anchor found within 1 s. Otherwise the game's own HP cluster shows
  as before, and one log line says why.
- **Settings:**
  - `hud_vitals` (wrist / classic)
  - `vitals_gauge_scale`
  - `vitals_gauge_from_wrist_m` 0.035
  - `vitals_gauge_standoff_m`
  - `vitals_heartbeat`
  - `vitals_low_hp` 0.30
  - `vitals_heartbeat_mute_s` 20
  - `vitals_edge`
  - `haptic_floor` 0.3 / `haptic_min_ms` 25 (the weakest Quest pulse that can be felt)

**Playbook ch04, UI & HUD:**

- The three places a HUD can live (compositor quad, blitted into the eye images, a world entity), and their costs: a quad
  is the sharpest but has no occlusion.
- Body-locked panels.
- Look-activate with a forced-on override at low health (DOOM-3-BFG-VR `vr_hudLowHealth`): "the comfort feature yields
  automatically in the one situation where hiding information could cost the player the game".
- The stereo-widget lifecycle: track texture, layer, visibility and pose space separately, and never leave a ghost copy behind.

**What GoTVR already has:**

- **The game's UI as its own image.** v7 UI layer, HUD-004: the host copies it into `uiSc` and shows it as a room-fixed
  panel (`host.cpp`, `uq`).
- **A GDI-painted overlay atlas** (`xroverlay.h`, `ov`, 1024×512, two halves) for the item wheel and the gesture indicator,
  shown as quads at the hand (`wq` / `gq`).
- **The controller grip poses** in LOCAL space in the host (`xrinput.h`, `grip_pose`).
- **The left-hand-at-head gesture** (`ov::St.atHead`; 0.18.0 toggles the HUD panel with it).
- **Health:** hero+0x1974 / +0x1970, and every change goes through SetHealth (0x69F3D0, hooked since 0.18.43).
- **Haptics:** `vrin::pulse`.

## The plan

### W0: find resolve (RE, flat play, no VR needed)

Health is solved. Resolve is not: the 32 KB hero watch never showed it, so it lives outside the hero object.

1. **Follow the heal.** A resolve heal arrives as HandleHit flags 0x40000001 (+122.5) on Jin.
   - Log the caller chain for that message (`RtlCaptureStackBackTrace` in `cbt::hHandleHit`, flat only).
   - The heal ability two or three frames up spends resolve just before sending it.
   - Disassemble or decompile there: the field it subtracts from is resolve.
2. **Follow the stat names.** The binary names its resolve stats: `MeleeResolve`, `RangedResolve`, `DamageResolve`,
   `ResolveLossOnDamage`, `ResolveRegenerateMax`, `ResolveRefundChance`, `CombatResolveThresholdAmount`,
   `ToolKillResolveAmount`, `HealGourdResolve`, `GourdResolve` (.rdata 0x1172CE8..). Their cross-references lead to the code
   that adds and removes resolve.
3. **Confirm in a flat session.** A new observer line, `VITALS/s`, logs health, max, resolve and resolve max. Tag a heal
   (numpad /), a kill (resolve gain), a parry (resolve gain) and a special art (resolve spend).
   - Resolve should move by whole segments for spends and by fractions for gains.
   - Set `vitals resUnit` to the value of one segment.
4. **Exit:** resolve and its maximum read through a signature-checked path that fails closed, plus the segment size.
   Until then the gauge shows health only, and W1 shows the game's own resolve art.

### W1: the game's own art on the wrist (fast path, no resolve RE needed)

The game already draws the health and resolve cluster into the UI layer, so show that rectangle on the wrist.

- **Find the rectangle.** Ctrl+F12 (new) saves the UI layer to `GoTVR\ui_dump.png` and logs its size. Measure the
  bottom-left health / resolve cluster once and store it as `wrist_hud_rect` = x,y,w,h, as fractions of the UI canvas.
  This scales with resolution; check it against the game's HUD-size and safe-area options.
- **Host:** a third `XrCompositionLayerQuad` on the same `uiSc` swapchain, using `subImage.imageRect` = that rectangle.
  Nothing new is rendered.
  - Its pose is the left grip pose × `wrist_hud_offset`: on the OUTSIDE (the back) of the forearm (owner; the kunai goes
    on the inside), `wrist_hud_from_wrist_m` 0.06 toward
    the elbow, 1 cm above the sleeve, face up and turned toward the thumb.
  - Its size is `wrist_hud_width_m` 0.09, with height from the rectangle's aspect.
- **Cut it out of the room panel**, so it isn't shown twice. After the copy into the `uiSc` image, copy a transparent block
  over the rectangle in the room panel's image only. That needs a second `uiSc` image or a clear with a rect on a
  render-target view, so the wrist quad uses the uncut copy.
  - Simplest: two swapchains, the full UI for the wrist sub-rect and the cut copy for the panel.
- **Visibility:** `vit::Glance` (facing you, within 0.65 m, within 35° of view; 0.12 s to show, 0.25 s to hide), forced on
  below 30 % health and for 2 s after any change. When hidden, the quad is simply not submitted.
- **Full HUD at the head.** `vit::HandAtHead` (left grip within 22 cm in front of the eyes for 0.3 s, released past 30 cm)
  shows the whole room HUD panel, objectives included, uncut, beside the head. The rest of the time the panel shows the
  cut copy, or nothing (`wrist_hud_panel` = always / at-head).
- **Known limits:**
  - The game's own HUD fades health out of combat. Either set the game's HUD option to always show health and resolve,
    or use W2.
  - A compositor quad has no depth, so the katana or the right hand passing in front won't cover it. Hide it while the
    left hand grips the katana (two-handed) or the sheath. The playbook calls this cost of quads acceptable for small panels.
- **Exit:** health and resolve readable on the wrist in the headset, shown only once, falling back to the room panel if
  the left controller is untracked.

### W1b: in world space, hidden behind things (owner, 2026-10-21: "is there no way to hide it behind objects by having it in world space?")

Yes. A compositor quad (W1) can't be covered by the game's world, because the runtime puts it over the finished picture. There
are three ways to get real occlusion with the game's own HUD art, best first:

1. **Draw it into the game's own frame, per eye, against the game's depth: the true world-space HUD.** This is the
   bldbrne-vr route (its gauge is drawn into the game's frame with the game's camera and depth). In GoTVR:
   - **Texture:** the UI layer we already capture (v7, the game's UI render target), cropped to the health / resolve
     rectangle. The UI drawn last frame is used this frame; a frame's lag is invisible on a HUD.
   - **When:** inside each eye's render, after the game's TAA / upscale and tonemap (so it is not jittered, smeared or
     tone-mapped twice), before the game composites its UI. The render-pass atlas (docs/14) and the per-eye hooks already
     know these boundaries.
   - **How:** one textured quad with our own small PSO, at the wrist pose converted to that eye's view (hSlot43 composes
     each eye's camera). In its pixel shader it reads that eye's scene depth (reversed-Z, with the render → output scale for
     DLSS) and discards where the world is nearer.
   - **What it gets right:** your hand, the katana, a wall or grass in front all cover it, as on a real arm. It is drawn at
     eye-buffer resolution, which is fine at forearm distance (about 9 cm wide at 35 cm).
   - **Cost:** a moderate amount of D3D12 work: PSO, root signature, precompiled shaders, a hook at the right pass, DLSS
     scale. The risk is the pass boundary, so it fails closed and falls back to the W1 quad.
2. **Let the runtime depth-test the quad** (`XR_FB_composition_layer_depth_test` plus `XR_KHR_composition_layer_depth` on
   the eye layer). We submit each eye's depth with the picture and the runtime hides the quad where the world is nearer.
   - Cheaper, and the depth submission also feeds Spacewarp (already on the roadmap).
   - **Only if Virtual Desktop's runtime offers the extension.** The host now logs every extension the runtime offers
     (`runtime extensions (n): ...` in host.log) so the next start answers this.
3. **Hide it against the things we know** (always available, no game depth). Hide or cut the panel when your right hand,
   the katana's blade (both are segments we track) or the left hand gripping the katana comes between your eye and the
   panel. That covers almost every real case on a forearm. A wall in front of your arm is the only miss.

**Decision (owner, 2026-10-21):** option 1. The mod must work on every OpenXR runtime, not only Virtual Desktop, so it can't
depend on a runtime extension. Option 2 is dropped as a route (the extension log stays as information); option 3 stays as
the fallback.

**Plan:**
- Build W1 as the quad first, so the crop, the placement on the outside of the forearm and the glance get tuned quickly.
- Add option 3 at once.
- Then option 1 for true world-space occlusion. This is the committed target.

#### W1b implementation plan (option 1, chosen)

1. **Where:** in the game DLL, at the point each eye's picture is published to the host. That's after the scene, TAA and
   tonemap, before the game composites its UI (the UI is already redirected to its own target, HUD-004).
2. **Inputs per eye:**
   - The eye's scene colour: our shared eye texture, written by us.
   - The scene depth:
     - the Present descriptor's `desc+0x18` (reversed-Z, D3D12_TRANSPORT.md; whether it is filled with frame generation
       off is EXP-06, open);
     - else the depth target the render-pass atlas finds bound for the final opaque passes.
   - The UI texture we already capture: the health / resolve rectangle (`wrist_hud_rect`).
   - The eye's pose and asymmetric FOV, from the pose packet that rendered the pair (S->eye[e], S->fov[e]).
   - The left grip pose for the same display time (S->hand[0]). The panel pose uses the same math as xrwrist.h, moved into
     a shared header.
3. **Draw:** one textured quad with our own root signature and PSO. The vertex shader projects the four corners with that
   eye's view × projection.
4. **Hide behind things:** the pixel shader samples the scene depth at the pixel (scaled if the depth is at render
   resolution), turns both into view depth (reversed-Z: z = near / d with the frame's near plane), and discards where the
   world is nearer by more than 1 cm. Then it blends premultiplied UI colour (the UI's own alpha) into the eye colour.
5. **Resources:** the depth goes to PIXEL_SHADER_RESOURCE and back to its own state (the descriptor gives it); one SRV heap
   with 2 descriptors; the eye texture as RTV. All on our own command list on the game's direct queue, before the existing
   publish fence.
6. **Fail closed:** no depth found, a format we don't know, or the PSO failing → back to the W1 compositor quad (one log
   line).
7. **Exit tests:**
   - hand / katana / grass / a wall in front of the panel hide it;
   - it is fixed on the forearm in both eyes, no swimming (same pose packet as the picture);
   - the HUD rectangle is still cut from the room panel;
   - no cost when the panel is hidden (nothing is drawn).

### W2: our own gauge from the values (the bldbrne-vr route, adapted)

- **Data:** shared block v10 `vitals` (seqlock, game → host): hp, hpMax, res, resMax, ok, lastHitMs, changed bits.
  - It is written in `hGameTick` from `vit::ChangeTracker` (health from hero+0x1974 / +0x1970, resolve from W0).
  - SetHealth's hook stamps `lastHitMs` for the heartbeat mute.
- **Painting:** the host paints the gauge into the overlay atlas with GDI, like the item wheel: a dark lacquer plate,
  a white health bar (red edge for damage just taken, fading over 0.4 s), gold resolve segments (the part-filled one
  lighter), and small digits.
  - It is repainted only when the values change (the `ov` signature scheme).
  - With W1b it is drawn in world space too, just with our own texture instead of the game's crop.
  - Shown with the W1 quad placement and visibility rules.
  - Repainting on change also removes the game-HUD fade problem: always visible on a glance.
- **Heartbeat** (design doc "Rumble feedback low health"): `vit::Heartbeat` / `vit::bpm` / `beatEnvelope` drive the left
  controller (lub 1.0 then dub 0.7, 150 ms apart, at `vitals_heartbeat_gain` × the design doc's value). It is felt only within
  20 s of the last damage (`vit::heartbeatFelt`) and never below Quest's 0.3 / 25 ms floor. The gauge pulses with the beats.
- **Optional dark edges** below 60 % health: a host vignette quad. Off by default; the playbook comfort rules apply.
- **Settings:**
  - `wrist_hud` (0 off / 1 game art W1 / 2 own gauge W2)
  - `wrist_hud_width_m`
  - `wrist_hud_from_wrist_m`
  - `wrist_hud_standoff_m`
  - `wrist_hud_glance`
  - `wrist_hud_low_hp` 0.30
  - `wrist_hud_heartbeat` 1
  - `wrist_hud_heartbeat_gain`
  - `wrist_hud_heartbeat_mute_s` 20
  - `wrist_hud_panel` (always / at-head)
- **Exit:** the self-tested core decides everything (fills, edges, heartbeat timing, glance); the host only paints and
  places it.

### W3: on Jin's own sleeve (polish)

- **Place it from Jin's drawn left forearm and hand**, not the controller. The FBIK already writes the world matrices of the
  IK arm, so take the forearm joint → hand joint and convert game world → LOCAL with the eye-anchor transform that hSlot43
  publishes. It then stays on the sleeve whatever the wrist trims do. (bldbrne-vr places it from the game's final pose for
  the same reason.)
- **Occlusion,** in order of cost:
  - hide it when the right hand or the blade is between the eye and the gauge (we know both segments);
  - depth submission (XR_KHR_composition_layer_depth, already on the roadmap for Spacewarp);
  - drawing it into the game's frame (bldbrne-vr's in-frame route). That is a large job on D3D12 we don't own.
- **The kunai** (owner, 2026-10-21): on the inside of the left forearm (the side facing up with the palm up). The HUD is on
  the outside (the back of the forearm), so the two never overlap.

## Order and why

1. **W0 and W1 together:** W1 needs no resolve RE and gives the game's real art at once. W0 runs alongside in flat sessions,
   which I can drive myself through computer use when the PC is awake.
2. **W2 once W0 has resolve:** it gives always-on readability and the heartbeat.
3. **W3 last.** It's polish, and it depends on depth / occlusion work shared with the arms.

## Status

- **0.18.54: W0 DONE.** Resolve maximum = hero+0x4644 + 3.0, the game's own progression value (every refill / clamp /
  fraction in the game uses it), so the pips follow Jin's upgrades.
- **0.18.53: W0 mostly done.** Resolve is hero+0x43A4, a float in pips (found by the resolve finder in session 9; a copy
  at [hero+0xB8]+0x7BC). Published as vitRes; vitResMax = `wrist_resolve_max` (default 10) until the real maximum is
  found. The `VITALS` log line (every 5 s) shows the fields around it for that search.

- **0.18.52 / host 0.15.1: W2 built** (xrgauge.h). Our own gauge, painted with GDI from vitHp / vitHpMax (vitRes / vitResMax
  once W0 finds resolve), is shown at the wrist place whenever you glance (owner: "always show when the player looks at the
  forearm"; the game's HUD fades health out of combat). `wrist_hud_mode` 1 / 2 / 3; 3 (default) stacks the game's own art above it.

- **0.18.48 / host 0.15.0: W1 is built** as a compositor quad, with the crop, the cut from the room panel, glance, low
  health and change forcing, the hand-at-head full HUD and the heartbeat. It needs headset tuning: `wrist_hud_rect` (the
  default was measured on the flat 16:9 HUD), the size and the place on the forearm.
- **Next:** W1b (drawn into the game's frame against its depth) and W0 (resolve: the heal's caller chain is logged).
- `src/vitals.h` is done and in the self-test (selftest=0). It covers fills with guards, change edges (hit / heal / resolve
  gain and spend / max changes / died / revived), the heartbeat rate and timing, the envelope and mute, the glance with
  low-health and on-change overrides, and hand-at-the-head with hysteresis.
