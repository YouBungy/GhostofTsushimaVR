# Ghost of Tsushima VR (GoTVR)

An unofficial, independent VR mod for Ghost of Tsushima DIRECTOR'S CUT on PC. It renders the game in true stereo with 6DoF head
tracking, puts you in Jin's body in first person, and gives you motion-controlled katana combat with no attack animations: you
draw, swing, block, parry and sheathe the sword yourself. No game files are included.

**Work in progress.** Current version: GoTVR 0.18.90 (game DLL), GoTVRHost 0.15.10.

## Features

- **True stereo and 6DoF:** each eye is rendered by the game from its own camera, both from the same game tick, and shown as a
  complete pair. Room-scale movement and leaning move Jin with you.
- **First-person body:** Jin's own body with full-body IK arms that follow your controllers, finger poses from your grip and
  trigger, and his head hidden from your view.
- **Physical katana:** grab the hilt on your hip and pull to draw. The blade rides out of the sheath along its rail, and you guide
  the tip back into the mouth to sheathe.
- **Free-flowing combat, no attack animations:** your blade hits what it touches, as hard as you swing. Heavy attacks: hold the
  trigger to charge, then swing. A swing toward a nearby enemy takes a step toward them.
- **The game's own block and parry:** hold the katana up in front of you to block. Move it into an attack to parry, and as late as
  you can for a perfect parry. The game plays its own sparks, sounds and reactions. Unblockable (blue) and unparryable (red) attacks
  follow the game's rules.
- **Katana sounds from the game:** draw, sheathe and swing sounds are the game's own, played from the sword. A swing's volume
  follows its speed.
- **Wrist HUD and comfort:**
  - Health and resolve on your left forearm, with a heartbeat at low health.
  - A sharp HUD panel that stays put in the room.
  - Menus, map and videos on a theatre screen.
  - A red damage vignette and block / parry flashes.
- **3D audio:** the game's sound listener is your head.
- **VR controllers:** any OpenXR controller (Quest Touch / Touch Plus / Pro, Index, Vive, WMR, HP Reverb, Pico). Haptics included.
- **Item wheel and gestures:** a wheel for heal, photo mode, horse, flute and recenter; hand-to-head gestures for the guiding wind
  and the objective.

## Supported game version

Ghost of Tsushima DIRECTOR'S CUT on Steam, current build. The mod checks the game's exe (hash, build stamp and code signatures) and
does nothing on an unknown build.

**Single-player only.** Never enter Legends (online) with the mod installed.

## How to use

1. Copy `mod/install/xinput1_4.dll` into the game folder (next to `GhostOfTsushima.exe`).
2. Copy the `mod/install/GoTVR` folder (`GoTVRHost.exe`, `openxr_loader.dll`) into the game folder.
3. Start the game once to create `GoTVR\gotvr.ini`, quit, and set `xr=4` in it.
4. In the game's graphics settings, set the display mode to **Windowed** and anti-aliasing to **TAA**.
5. Start your VR runtime (Virtual Desktop / VDXR, or SteamVR), then start the game from Steam.

Tested on Quest 3 with Virtual Desktop. SteamVR works through OpenXR.

## Controls

Quest Touch names (X/Y left, A/B right). Every button can be rebound in `[controls]` of `gotvr.ini`.

### LEFT CONTROLLER

- **Move:** left stick. **Run:** click the stick.
- **X:** quick press heals; hold to open the item wheel. Choose with the right stick and release to confirm.
- **Y:** quick attack / assassinate.
- **Trigger:** Focus (while not holding the katana).
- **Menu (three lines):** the game's menu.
- **Grip:** hold the sheath (with the right hand on the hilt, to draw or sheathe).

### RIGHT CONTROLLER

- **Snap turn:** right stick left / right (30 degrees). **Crouch:** click the stick.
- **A:** jump. **B:** dodge (with a throwable held: cycle items).
- **Trigger:** the game's own interact / pick up, and standoff when it is offered. With the katana out: hold to charge a heavy
  attack.
- **Grip:** take the katana's hilt.

### The katana

- **Draw:** put your right hand on the hilt at your left hip, squeeze the grip and pull the blade out. Holding the sheath with the
  left grip steadies it.
- **Attack:** swing. Faster swings hit harder. **Heavy:** hold the right trigger until the rumble builds, then swing.
- **Two hands:** bring the left hand to the hilt and squeeze its grip.
- **Block:** hold the blade up in front of you as the enemy strikes.
- **Parry:** move the blade into the attack just before it lands. Late = perfect parry.
- **Sheathe:** bring the tip to the sheath mouth at your left hip and push the blade home.

### Gestures

- **Guiding wind:** right hand at your head + trigger.
- **HUD and objective:** left hand at your head + trigger.
- **Recenter:** hold the headset's own recenter button, or pick Recenter on the item wheel.

## Options

All options live in `GoTVR\gotvr.ini` (created on the first start). Restart the game after changing one. The most useful:

<details>
<summary>General</summary>

| Option | Default | What it does |
| --- | --- | --- |
| `xr` | 0 | 4 = VR (synchronized stereo). 0 = the mod does nothing to the picture. |
| `play_mode` | vr | `flat` = play on the monitor while the mod logs, for flat test rounds. |
| `render_size` | 2304 | Each eye's square render size. **F2** cycles sizes while playing. |
| `history_mode` | 7 | Per-eye culling preset. **F6** cycles 1-9 (the number shows as squares). |

</details>

<details>
<summary>Combat</summary>

| Option | Default | What it does |
| --- | --- | --- |
| `parry_engine` | outcome | The game's own block / parry from your blade. |
| `parry_perfect_ms` | 150 | How late a parry must be to count as perfect. |
| `guard_reach_cm` | 110 | How far in front of you the blade counts for a block / parry. |
| `sword_lunge_swing_cm` | 80 | The step toward the enemy you swing at (0 = off). |
| `sword_weight` | 1.0 | The katana's weight and follow-through (0 = none). |
| `sword_sheathe_pivot_max_deg` | 25 | Below this angle the blade pivots into the sheath; wider, it goes straight in. |

</details>

<details>
<summary>Sound</summary>

| Option | Default | What it does |
| --- | --- | --- |
| `sound_katana` | 1 | The game's draw, sheathe and swing sounds from your sword. |
| `sound_swing_gain` | 1.4 | Light swing loudness. |
| `sound_swing_full_speed` | 5.0 | Swing speed (m/s) for full volume. Slower swings are quieter. |
| `audio_head` | 1 | 3D audio: the sound listener is your head. |

</details>

<details>
<summary>Display</summary>

| Option | Default | What it does |
| --- | --- | --- |
| `vr_aa` | taa | `dlss` = experimental per-eye DLSS (choose `dlss_quality`: game, ultra_performance, performance, balanced, quality, dlaa). |
| `xr_depth_layer` | 0 | 1 = send each eye's depth to the headset, for better reprojection of near and far objects. |
| `xr_display_hz` | 0 | Ask the headset for a refresh rate (0 = the runtime's own). |
| `fp_ssr_off` | 1 | Screen-space reflections off in first person (they mirror your hands onto glossy surfaces). |
| `hud`, `hud_distance`, `hud_width_deg` | 1, 1.6, 55 | The HUD panel. **F9** cycles sizes, **F4** fixes it to your view. |
| `theater` | 1 | Menus, map and videos on a screen in front of you. **F10** cycles modes, **F11** recentres. |

</details>

---

## Source

- `mod/src`: the game-side DLL (`gotvr.cpp`), the OpenXR host (`host.cpp`) and the shared protocol.
- `mod/build.sh`: MinGW-w64 cross build (needs MinHook and the OpenXR SDK headers).
- `mod/README.md`: every version's changes and test steps.
- `docs/`: findings, address registry, bug log, test plans.
- `tools/`, `test/`, `probe/`: helper scripts, self-test harnesses and the early census probe.

The repository contains no game files, disassembly or decompiled game code.

## Affiliation and rights

GoTVR is an independent fan project. It is not affiliated with, endorsed, sponsored, or approved by Sony Interactive Entertainment,
Sucker Punch Productions, Nixxes Software, or any headset or runtime maker.

Ghost of Tsushima and PlayStation are trademarks of Sony Interactive Entertainment. Rights in Ghost of Tsushima and its game content
remain with Sony Interactive Entertainment, Sucker Punch Productions and the respective rights holders. Other names and trademarks
belong to their respective owners. This project claims no ownership of those rights.

No game files are included. You need your own lawfully obtained copy of Ghost of Tsushima DIRECTOR'S CUT. Please do not request or
share game files here.

Third-party licences (MinHook, the OpenXR SDK / loader) are in [mod/licenses](mod/licenses).
