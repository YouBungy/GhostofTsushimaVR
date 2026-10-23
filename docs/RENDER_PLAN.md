# Render plan: the TAA hand / sword ghost and the Spacewarp depth layer

## Flat round 7 results (0.18.87, raining in game; render 1920 x 1080)

- **Motion vectors:** R16G16_FLOAT, in screen fractions. In the mid-swing capture, Jin's body and the blade have their own motion,
  separate from the camera's. This is **Case A**: the game writes per-object motion from the skin's current and previous pose.
  - Background: about 2 px of camera motion. The blade: up to about 20 px.
  - The rain draws no motion.
- **Depth:** D32_FLOAT_S8X24. Subresource 0 is the 4-byte depth plane (row pitch = width x 4).
  - Reversed-Z with an infinite far plane: the sky is exactly 0 (40 % of the far view); the ground at Jin's feet is about 0.044; the
    nearest values are about 0.09.
  - The near plane is about 10 cm (near / distance).
- **Why the ghost, then (hypothesis):** xr = 4 renders both eyes from one tick. The skin palette is rebuilt once per tick (it is
  stamped with the per-tick frame counter at item+8 +0x50, 0x59EB30 / 0x59EEF0). So the skin's "previous pose" is the previous
  tick's in BOTH eyes.
  - With the engine's cross-eye TAA (preset 7), the right eye's history is the LEFT frame of the SAME tick, where the hand
    already is at this tick's pose.
  - The right eye's hand pixels still say "I came from where I was a tick ago". TAA fetches history from there: background, or a
    hand a tick old. The result is a translucent second hand.
  - The left eye is consistent: its history is the previous tick's right frame.
- **Test (0.18.88):** history_mode 8 / 9 (F6 in the headset) give each eye its own TAA history and reprojection on top of
  preset 7's culling. Then history, camera and skin "previous pose" all agree. If the ghost goes away in 8 or 9, the hypothesis
  holds. The fix is then either 8 / 9 itself (cost: a history copy per eye) or, cheaper, making the right eye's skin "previous
  pose" equal its current one.
- **VR capture (0.18.88):** Insert = both eyes now; Insert twice = both eyes 120 ms after your next katana swing. Hold your head
  still and swing; the right-eye frame's hand motion shows the mismatch directly.


Status 0.18.86. This is the work to do after flat round 7 (docs/FLAT_TEST_PLAN.md, part B), in this order. Each step says what it
needs from the captures, what to build, and how to check it. The TAA path stays the default (`vr_aa = taa`) throughout. Every new
piece gets its own ini switch, off until a headset session proves it. The culling preset 7 work is not touched.

## What we know now

- **The ghost.** A faint second copy of Jin's hands and the katana trails them when they move fast (GoTTest1.mp4). It is the game's
  TAA history being blended in at the wrong place.
  - TAA moves last frame's picture by the motion vectors (AA +0xEA90, written by AA_Velocity 0x614720) before blending.
  - The arms and the katana are posed by the mod after the game's animation (IK, the sword in the fist). If the motion vectors for
    those pixels come from the game's own un-edited pose, or from the other eye's frame, the history is fetched from where the arm
    was not. That gives a second, fading hand.
  - Per-eye TAA history alone (0.18.37) was tried and removed in 0.18.38. It did not remove the ghost and cost a copy per eye.
- **Depth.** AA_Velocity reads depth from the frame ring: `[0x33761E0] +0x18`, entry `+0x620 * 0xA8`, `+0x88`. Its format, its
  size and whether it is reversed (near = 1) are not confirmed yet.
- **Headset runtime.**
  - VDXR offers XR_KHR_composition_layer_depth, XR_FB_display_refresh_rate and XR_KHR_visibility_mask.
  - It does not offer XR_FB_space_warp, so Meta's Application SpaceWarp (the app sends motion vectors) is not available through
    Virtual Desktop.
  - What can be added is a depth layer. Runtimes that use depth for reprojection use it to move near and far things correctly when a
    frame is repeated. Whether Virtual Desktop's own SSW uses it must be tested; SteamVR's runtime does use submitted depth.

## Session 35-36 update: the ghost is worst over glossy surfaces

- History modes 8 / 9 (per-eye TAA history) ran (restored 118/s) and did not remove it.
- The owner sees it most over glossy surfaces, so the suspect is screen-space reflections (SSR): the hands fill the screen in first
  person, and SSR's own temporal history in xr = 4 is the other eye's frame.
- `fp_ssr_off = 1` turns SSR off in first person as a test. The owner wants SSR on, so the fix must be SSR's own per-eye history (as
  the TAA bank does for TAA).
- **Next RE step:** find the SSR pass (the PC settings block ScreenSpaceReflections +0x84 -> the render service's SSR switch -> its
  pass and the previous-frame colour it reads), then bank that history per eye.

## Part 1: TAA ghost

### 1a. Read the captures (no code)

From round 7 B, captures 1 and 2 (camera still; idle, then mid-swing):
- **Case A: the swing has its own motion vectors.** Capture 2's 16 x 9 map shows motion only where the sword and arm are. The
  engine writes per-object motion from skinning (current against previous bone matrices). Our IK edit is the mismatch: the motion
  vectors still describe the game's own arm, not the one drawn.
- **Case B: no per-object motion.** Both captures are nearly zero. Only the camera writes motion. A moving hand then always
  reprojects onto the background, and TAA's neighbourhood clamp is what limits the ghost.

Capture 3 (pan) checks the scale and sign: the whole image moves, so we know the units of the motion-vector buffer.

### 1b. Case A fix: give the edited arm its own previous pose

The skinning pass that writes motion needs this frame's and last frame's bone matrices for the same pose.
1. In Ghidra, find where the skinned mesh's previous-frame bone palette is kept. Start from the joint write the mod already does in
   the pose hook and follow the copy into the GPU constant buffer. Look for the second, "previous" palette.
2. After writing our IK bones for this eye, also write that eye's previous palette from **our** pose of the same eye last frame. We
   already keep each eye's pose; it is one more copy per eye, about 2 KB per frame.
3. ini `fix_arm_motion` (0/1), default 0.

Check: capture 2 again with the fix on. The arm's motion matches the swing direction and size. In the headset, the ghost is gone on a
fast swing and the background behind the hand is clean.

### 1c. Case B fix (or if 1b can't be found): mask the hands out of history

1. Draw the first-person arms and katana into a small mask (stencil bit, or the existing velocity buffer set to a "no history" value
   for those pixels).
2. Make TAA use the current frame only inside the mask. This is cheaper than per-eye history: one draw of the arms with a flat
   shader, no copies.
3. ini `taa_hand_mask` (0/1), default 0.

Check: no ghost; the hands may show a little more aliasing at the edges. That is the trade-off, so it stays a switch.

### 1d. Not doing

- Turning TAA off for the whole frame (the game's foliage then shimmers badly).
- Per-eye TAA history again on its own (0.18.37: no gain).

## Part 2: Spacewarp depth layer

**Status 0.18.89:** built (2b and 2c) behind `xr_depth_layer`, waiting for the headset test (docs/VR_TEST_PLAN.md step 7).

### 2a. Confirm the depth buffer (round 7 B, captures 1, 4 and 5)

- Format, size and range from the `CAPTURE #n depth` line.
- Capture 5: a near wall then the horizon. If the near wall gives values near 1 and the sky near 0, the depth is reversed-Z. Note
  whether the far plane is infinite (sky exactly 0).
- The near-plane distance is already known per eye from the mod's projection (fp near clip, 2 cm in first person).

### 2b. Share each eye's depth with the host

1. After the eye's scene is drawn (the same point the mod copies the eye's colour for the host), copy the depth into a shared
   D3D12 texture per eye, created next to the colour one (same shared-handle path as D3D12_TRANSPORT.md).
2. If the game's depth is a typeless 32-bit depth format with stencil (R32G8X24), copy just the depth plane, or resolve it with a
   small compute pass into R32_FLOAT. The host converts it to the swapchain's D32_FLOAT.
3. Shared block: add depth handle, near, far, reversed flag (shared block v19).

### 2c. Submit it from the host

1. Create a depth swapchain per eye (usage DEPTH_STENCIL_ATTACHMENT, format D32_FLOAT) when XR_KHR_composition_layer_depth is on.
2. Chain an XrCompositionLayerDepthInfoKHR on each projection view: the eye's depth sub-image, minDepth 0, maxDepth 1, nearZ / farZ
   from the shared block. For reversed-Z, pass nearZ > farZ, which the extension allows. For an infinite far plane, use a large
   finite farZ.
3. The depth must be the same frame and the same pose as the colour of that eye. It travels with the pair, as the colour does (pair
   atomic).
4. ini `xr_depth_layer` (0/1), default 0. Cost: one depth copy per eye per frame and one depth swapchain image. Measure it; drop it
   if it costs more than about 0.3 ms.

### 2d. Test

- Virtual Desktop with SSW on, then off. Turn your head fast past a near post and a far hill. With depth, a repeated frame should not
  smear the post across the hill.
- SteamVR (if the owner uses it) with motion smoothing.
- If Virtual Desktop shows no difference, keep the switch off by default and say so in the README. It only costs when on.

## Order of work after round 7

1. Read the captures (1a, 2a) and write the findings to COMBAT_FINDINGS / BUGLOG.
2. Case A or B fix for the ghost, behind its switch. One headset session.
3. Depth sharing and the depth layer, behind its switch. One headset session with SSW on and off.
4. Defaults are changed only after the owner confirms each one in the headset.
