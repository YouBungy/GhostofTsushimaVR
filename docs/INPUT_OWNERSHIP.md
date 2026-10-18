# Input ownership (static, session 2)

Owner hardware: Quest 3 Touch Plus controllers through VDXR (Virtual Desktop) or SteamVR. OpenXR
interaction profile `/interaction_profiles/oculus/touch_controller` (bind also
`/interaction_profiles/meta/touch_plus_controller` when the runtime offers it, plus `valve/index_controller`
and `khr/simple_controller` fallbacks for SteamVR users).

## Stack `[STATIC]`

```
physical device
  ├─ XInput (pad loader 0x497F30: xinput1_4 → xinput9_1_0, plain LoadLibraryW, called unconditionally from NxInput init 0x44A090)  ─→ GamepadXInput
  ├─ DualSense via PsPcSdk/libScePad                                                  ─→ GamepadSce
  ├─ Steam Input (SteamInput006, action sets Set_InGame / Set_Menus)                  ─→ GamePadSteam
  ├─ Windows.Gaming.Input                                                              ─→ GamepadWindowsInput
  └─ keyboard / mouse / raw input                                                      ─→ NxKeyboardDevice / NxMouseDevice
                       all pads implement nx::Input::NxGamepad (15 slots, base vtable 0x10FD478)
                                          ↓  pad state at gamepad+0x80
                                 NxInputImpl (85 slots, service +0xB0)
                                          ↓
                           Sucker Punch gameplay (action ids, e.g. 0xA3..0xAE read by camera code)
```

## GamepadXInput, decoded (session 2)

| Slot | RVA | Role | Evidence |
|---|---|---|---|
| 1 | 0x497EC0 | probe/connect: `XInputGetState(user)`; sets connected `+0x79`, user `+0x108/+0x11C`, packet `+0x118` | decompile |
| 3 | 0x4983E0 | **rumble**: `XInputSetState(user, {lo*65535, hi*65535})` | decompile |
| 5 | 0x498030 | **per-frame poll** | decompile |

XInput function pointers: loaded flag `0x19B2678`, `XInputGetCapabilities` `0x19B2680`, `XInputGetState`
`0x19B2688`, `XInputSetState` `0x19B2690`, (`0x19B2698` 4th import) `[STATIC]`.

Poll (0x498030): if `dwPacketNumber` changed → `NxGamepad_SetButton(this, bit, &state, id)` 0x4973D0 for:

| XINPUT bit | 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100 | 0x200 | 0x1000 | 0x2000 | 0x4000 | 0x8000 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Nx id | 0x190 | 0x191 | 0x192 | 0x193 | 0x194 | 0x195 | 0x196 | 0x197 | 0x198 | 0x199 | 0x19A | 0x19B | 0x19C | 0x19D |
| Pad | D-Up | D-Down | D-Left | D-Right | Start | Back | L3 | R3 | LB | RB | A | B | X | Y |

(bits 0x400/0x800 are unused), then `NxGamepad_SetAxis` 0x497A90 with axis 0 = LT (byte), 1 = RT, 2 = left stick (sThumbLX/LY),
3 = right stick (RX/RY). Rumble request floats at `+0xEC..+0xF8`, flag `+0xFC` → slot 3. `[STATIC]`

**Consequence:** the game loads `xinput1_4.dll` by bare name at startup, so a proxy with **that** name in the game folder is picked up (probe v0.3). Synthesising `XINPUT_STATE` from
OpenXR actions in the proxy drives the **entire** native input funnel (menus, gameplay, photo mode) with
correct press/release semantics, and `XInputSetState` comes back to us for haptics. This is the T1 input
seam. Caveat: the game picks the "active" device; keyboard/mouse or a DualSense being used at the same
time will switch prompts/ownership (`[OPEN]`, test).

## Action map (proposal, T1: gamepad-equivalent)

Rule: T1 maps Touch controls 1:1 onto the Xbox pad layout (triggers→LT/RT, grips→LB/RB, face buttons
→A/B/X/Y, sticks→sticks/L3/R3, menu→Start), so the game's own control-remap screen and button prompts stay
valid. The semantic meaning of each pad button is the game's, and must be copied from the in-game controls
screen into this table at the first launch; it is not inferred here.

| Physical (Touch) | Pad emulation | Semantic (from in-game controls screen) | Press/release owner | Neutralisation | Haptic return |
|---|---|---|---|---|---|
| L thumbstick | left stick | move | proxy, per poll | zero on focus loss / stale pose | — |
| R thumbstick X | right stick X in menus only; in gameplay → VR-owned snap/smooth body yaw | camera yaw (flat) | proxy | zero | — |
| R thumbstick Y | right stick Y | camera pitch (flat); suppressed in 6DoF gameplay | proxy | zero | — |
| L / R trigger | LT / RT | `[OPEN]` | proxy | release on loss | rumble passthrough |
| L / R grip | LB / RB | `[OPEN]` | proxy | release | |
| A / B / X / Y | A / B / X / Y | `[OPEN]` | proxy | release | |
| Menu | Start | pause | proxy | release | |
| L / R stick click | L3 / R3 | `[OPEN]` | proxy | release | |
| (no Touch button) | Back/View | touchpad equivalent on PC `[OPEN]` | chord, e.g. L-menu long press | release | |

## Beyond pad emulation (T3/T4 motion controls) — owners still `[OPEN]`

| Mechanic | Native endpoint needed | Lead |
|---|---|---|
| Sword swing direction / stance | attack direction or stance selection (game uses buttons + R1 stance) | `StanceTrainer` component, `CApJointWarp` |
| Bow draw/aim | aim ray from hand pose instead of camera centre | `CAnimRelativeAim`, `RangedAim` press/hold/release events, `BowAimed`/`LongbowAimed`, `ReticleBow`, `aim_assist` |
| Throwables (kunai, smoke) | throw arc origin/direction | `ThrowAimArc`, `ThrowAimArcNew` |
| Guiding wind gesture, flute | DualSense touchpad swipe and gyro ("Flute Pitch") | GamepadSce slots (gyro/touch) — can be emulated by a synthetic DualSense path or by the Sce pad vtable |
| Hands visible | hero skeleton joint matrices (`obj+0x278`, 64 B/joint) | camera driver 0xA32DF0 shows the joint array format |

Haptics: XInput rumble (2 motors) is the native semantic return today; map lo → left hand, hi → right hand,
or both for impacts. DualSense adaptive-trigger effects run through libScePad and are not reachable via
XInput.
