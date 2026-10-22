# Flat-play data capture plan (for the design doc) - GoTVR 0.18.44+

## How to capture

- **Settings:** `play_mode=flat` in gotvr.ini (delete the second, older `play_mode=vr` line further down). NumLock on.
- **One tag per activity:** press its key to start the tag; the same key again ends it. Within a tag, press **F8 at each event**. The
  F8 marks are numbered, so a written note like "mark 12 = perfect parry" makes it airtight.
- **What gets logged:**
  - Your keyboard / mouse presses, with their time (INPUT/s). This version also logs them, so keyboard / mouse play is fine.
  - Jin's changing values (STATE/s).
  - Every hit and health change of every character, with its hit record (COMBAT).
- **Keep it clean:** one kind of event per tag, about 5 - 10 repetitions, a few seconds apart. A fight that mixes everything is much
  harder to read than five short fights.
- **Send:** gotvr.log (and gotvr.prev.log if the game was restarted).

| Key | Tag | Do this | What it gives (design doc item) |
|---|---|---|---|
| numpad 1 | light attacks | light attacks that land, on 2 - 3 enemy types (sword, spear, shield, brute); some whiffs | the hit record of a landed hit = the template the physical katana sends (Combat: light attacks, damage, "best stance" per enemy) |
| numpad 2 | heavy attacks | heavy attacks (charged, released) that land; the same enemy types | heavy vs light in the hit record (Heavy attacks) |
| numpad 3 | blocking | hold block while enemies hit you (no timing) | how a blocked hit looks (Blocking) |
| numpad 4 | parry | normal (late) parries | what the game does on a parry: the incoming-attack list, the attacker's hit record (Parry variants 1 / 2) |
| numpad 5 | perfect parry | perfect parries only | perfect vs normal (Parry windows) |
| numpad 6 | dodge / perfect dodge | dodges, then perfect dodges | the dodge state (Sidestep button) |
| numpad 7 | taking hits | stand and take hits, include an unblockable (red) attack | damage and hit reactions on Jin (haptics, first-person hit reaction) |
| numpad 8 | standoff | trigger standoffs: a win, a loss (release early), a chained win | the standoff state and its timing (Standoff via left trigger + draw) |
| numpad 9 | finisher (crawling enemy) | let an enemy crawl away, then finish him | the crawl state (finisher animation only for crawlers) |
| numpad 0 | assassination | ground assassinations, then from a height (a roof / a ledge) | ground vs air (air keeps its animation) |
| Ctrl+numpad 1 | bow | draw, hold, release; hits and misses | the arrow's fire path (Archery) |
| Ctrl+numpad 2 | longbow + concentrated aim | longbow shots; concentrated aim (slow time) | the slow-time state (left trigger slow-down) |
| Ctrl+numpad 3 | kunai | kunai throws at enemies | the throw / projectile path (Kunai on the wrist) |
| Ctrl+numpad 4 | blowgun | blowgun shots | blowgun fire (chest blowgun) |
| Ctrl+numpad 5 | heavenly strike | a few heavenly strikes | its state (sheathe + trigger + swing) |
| Ctrl+numpad 6 | dance of wrath | a few dances, several enemies | its target list (auto-turn to each locked enemy) |
| Ctrl+numpad 7 | grapple | grapples, including the moment the prompt appears in the air | the "hook point available" state (grapple prompt) |
| Ctrl+numpad 8 | wall climbing | climbing walls / cliffs | the climb state |
| Ctrl+numpad 9 | ledges / ropes / logs / beams | small ledges, walking ropes, logs, beams | a reliable "traversal" state (below) |
| Ctrl+numpad 0 | crouch / stealth | crouch and walk; stand; crouch in tall grass | the crouch state (physical crouch threshold) |
| numpad / | healing / resolve | heal a few times; gain resolve in a fight | the resolve value (forearm health / resolve UI) |
| numpad * | cutscene | a cutscene start to end | (the mod already detects these - a check) |
| numpad - | horse | mount, ride, dismount | the riding state (camera / arms on the horse) |

## Priorities

1. **Combat:** numpad 1, 2, 4, 5, 3, 7. These turn the HandleHit / SetHealth hooks (0.18.43) into the physical katana's damage and
   parry.
2. **Traversal:** Ctrl+numpad 9, Ctrl+numpad 8, Ctrl+numpad 7. Your note: ledges, ropes and logs stay first person, but Jin's own
   animation takes over the body and arms. Today the arms go back to the game only when the animation lifts a hand to head height
   (climbing), so a rope or log walk fights the VR arms. A traversal flag from this data lets the mod hand the body over cleanly, and
   take it back after.
3. **Standoff, assassination, the finisher:** numpad 8, 0, 9.
4. **Then the gadgets and the specials:** Ctrl+numpad 1 - 6.

## What does not need flat data (built from the controllers alone)

- Swing detection, heavy charge (trigger + stillness), the lunge and its parameters, the blade-to-blade contact geometry: playbook
  HAND-004 (a testable motion decision core) and INPUT-010 (measure swings against the body, not the world - walking must not count
  as a swing).
- The holsters:
  - the bow over the left shoulder, arrows over the right, the kunai on the wrist, the blowgun on the chest, the tanto;
  - playbook HAND-013: a holster is a pose, a hand and an accept-list, with grab hysteresis and in-headset placement.
- The kunai's throw release: HIGGS / SteamVR (the release velocity from the hand's recent peak), per the design doc's references.
- The gestures (bowing, the blood wipe between forearm and bicep): a decision core, then the game's own action (the blood flick =
  the sheathe swipe's chiburi; the bow = the touchpad-down swipe).
