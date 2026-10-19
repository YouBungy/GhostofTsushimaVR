# GoTVR 0.16.5 — SYNCHRONIZED STEREO 6DoF (xr=4): both eyes from the same moment of the game

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
