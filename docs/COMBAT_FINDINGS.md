# Combat findings: the flat-play session of 0.18.42 (2026-10-21)

**Source:** gotvr.log with play_mode=flat. Tags: archery 21 s, sword combat 54 s + 25 s (marks 1-16), grappling 4 s, climbing 4 s.
The observer watches the hero's first 16 KB. Offsets are hero-relative, and the addresses are this game build's (rva).
The binary findings come from a byte-pattern scan of the local copy of the exe. No decompiled code is kept in the repo.

**Not available:**
- Pad buttons stayed 0x0000: the session was played on keyboard / mouse, so the observer's pad capture saw nothing.
- The marks do not say which event each was. They were matched to the data by timing.

## Confirmed states

| Field (hero+) | Type | Meaning | Evidence |
|---|---|---|---|
| 0x1974 | float | **health** | -100 / -125 per hit taken, +122.5 per resolve heal (1 -> 123.5), +7 regen ticks, floor at 1 (the modded save). Marks 6 / 7 / 15 are 129 - 167 ms after a drop. |
| 0x1970 | float | max health | SetHealth compares against it. SetMaxHealth (0x69F540) writes it. |
| 0x19D8 / 0x19DC | int | 32-entry hit-history ring: write index (31 .. 0) / count | Steps on every health change. HandleHit writes it (0x6941C8). |
| 0x19E0 + 32 * i | struct | history entry: time, hit id, +0x94, +0x98, optional 3 floats, flag byte | HandleHit 0x694195 - 0x69426D |
| 0x1990 | ring | 16 recent hit ids (a hit is never applied twice) | HandleHit's first call (0x84DF30) |
| 0x39C0 | pointer | set at the instant a hit lands on Jin, cleared 1 - 3 s later (attacker / damage-source reference) | Every hit taken (the high half of the pointer = "1416" in the log) |
| 0x3918 + 16 * i, count at 0x39A0 | list | **paired actions** (0.18.45 correction): set 12 ms after the click of a finisher on a crawling enemy, at a horse mount (E), around assassinations. NOT incoming attacks (only 7 events in the 0.18.44 session). | Writers: 0x9CBA00 (remove by id), 0xB97C60, 0xC3F4B0, 0xC460A0, 0x1099490 |
| 0x36A8 | int | 1 while Jin is at the health floor (the downed / "last stand" state of the modded save) | 0 -> 1 when health -> 1, back after the heal. Writer 0x69ED10 (a life-state setter). |
| 0x8 | int | 3 <-> 4 in fights (twice as often as outside them) | Candidate: combat / alert state. Unconfirmed. |
| 0x23A0 + 0xA0 * k (k = 0..4) | 5 slots | pointer + sequence number + two ms timestamps per slot: an action / animation request queue | Archery changes them about once a second (bow draw / release). Sword combat, grappling and climbing occasionally. |

## The game functions (hook points)

- **SetHealth(character, float)**, rva 0x69F3D0: the ONLY runtime writer of +0x1974.
  - It keeps the old value, writes the new one, and compares it with max (+0x1970).
  - It posts a "health changed" event (ids 0x75 / 0x66) with old and new.
  - Every character goes through it: damage, heals, regen.
- **HandleHit(character, hitInfo)**, rva 0x694080: a virtual function, in 4 vtables at .rdata 0x112DD98 / 0x1172AA8 / 0x12030A8 /
  0x125E8D8 (character classes). What it does with hitInfo:

  | hitInfo field | Use |
  |---|---|
  | +0x2C | hit id; repeats are dropped (the hit-id ring at +0x1990) |
  | +0x90 | flags |
  | +0x94, +0x98 | two floats, recorded in the history (damage and posture / stagger: to confirm) |
  | +0x9C / +0xA0 / +0xA4 | optional values, used when flag bits 2 / 8 / 0x80 are set |

  - Flag bit 21 calls 0x6A1250.
  - Flag bit 20 is the kill path: SetHealth(0), then 0x69DF00(0), then 0x69ED10(state 2).
  - This is where an attack becomes damage. The physical katana will call it on an enemy with a hit record of its own.

## What 0.18.43 logs (next flat session)

- **`COMBAT hit on JIN / other <ptr> (class N)`:** the hit id, flags and the five floats, health before -> after, the active tag, and
  the hit record's first 0xB0 bytes as hex.
- **`COMBAT health:`** every SetHealth, of any character.
- **`COMBAT/s:`** hits per second (on Jin), health changes, the incoming-attack count on Jin, Jin's health.
- **What these will settle:**
  - which hitInfo field is the damage and which is the attacker;
  - whether the hits Jin lands on enemies go through the same HandleHit (expected: yes, the vtables cover the enemy classes);
  - what an enemy's class id and health are;
  - what perfect parry vs parry vs dodge look like (the hit record of a parried attack, or no hit at all).

## How the physical katana plugs in (plan)

1. **Damage:** sweep the blade (guard -> tip, every frame) against the enemies' bodies. On contact, build a hit record from a template.
   The template is a copy of the last hit Jin's own sword landed on that enemy class, captured by the HandleHit hook. The mod then gives
   it a new hit id and scales the damage by the tip speed, and calls HandleHit(enemy, record) on the game thread.
   - Fail closed: no template captured, no damage.
2. **Parry:** while hero+0x39A0 > 0, the attacker is known (list entry). If the katana blocks the attacker's weapon in that window,
   that is a parry; the closer to the hit, the better (perfect).
   - Next session: find how the game resolves a parry - the HandleHit record of a blocked attack and the list entry's removal.
3. **Getting hit:** the HandleHit hook on Jin = haptics (both controllers, scaled by health lost) and the hit reaction kept steady in
   first person (ROADMAP 4).
4. **Standoff:** not logged yet.

## For the next session

- Tag each activity and press F8 at each event.
- A sword combat tag with only one kind of event in it (one fight: only parries; another: only perfect parries; another: only taking
  hits; another: only light attacks; then heavy) makes each signature unambiguous.
- A standoff, if possible.

# Session 2 (0.18.44, 2026-10-21): the hit messages decoded

**The session:** 31 tags, 180 marks, 310 hit messages, 301 health changes.
- **Classes:** Jin = class 15. Human enemies = class 14. Class 16 = most likely the dogs (only health inits as they spawned, no
  hits).
- **Caps Lock:** no effect; the logger reads the physical keys.

## The HandleHit message (0xB0 bytes seen)

| Field | Meaning |
|---|---|
| +0x00 | vtable (one message class) |
| +0x2C | message id, unique per hit; repeats dropped |
| +0x30 | a sender reference (not the attacker) |
| +0x38 | damage-kind hash: 0x07222524 = sword / melee (almost all); 0x0672DF0E = assassination / another kind; 0xF808B28B = a third kind |
| +0x40 | the move / attack id: enemy moves on Jin 3, 8, 0xA, 0x1F9, 0x229 ...; Jin's 2, 6, 8, 0xE, 0x215 ... |
| +0x48 | **the attacker's character** (Jin when he lands a hit) |
| +0x50, +0x58, +0x78 | more references (weapon / animation: to confirm) |
| +0x60 | hit level: Jin's light 3, heavy 4 - 5, specials 5; enemy hits 2 - 5 |
| +0x80 | hit category: 2 = Jin's hit on an enemy, 3 = an enemy's hit on Jin, 4 = heavy / break |
| +0x90 | flags (the message kind, below) |
| +0x94 | health change (negative = damage; -FLT_MAX = kill) |
| +0x98 | a second damage value, only on heavy hits / heavenly strike (-65 / -75): posture / stagger |
| +0x9C | with flag 0x60000002: the max health to set (350 Jin, 255 / 375 / 525 enemies) |

| Flags | Kind | Seen |
|---|---|---|
| 0x03C00041 | Jin's sword hit on an enemy | light -75 / -85 (by enemy type), heavy -170 with -65 posture (+0x100 bit: 0x03C00141), heavenly strike -150 / -75 |
| 0x19C00841 | Jin's light hit, bit 0x800 (a guarded / armoured target?) | -85 |
| 0x03C00001 / 0x03800001 / 0x03400001 | kill (-FLT_MAX) - sent after the damage brings health to the end, or by a finisher | every death |
| 0x03800041 | an enemy's hit on Jin | -100 / -109 / -125 / -87.5 / -74 / -30 |
| 0x11802041 | a heavy enemy hit on Jin, while Jin was attacking (interrupted) | -122.5 |
| 0x11C01041 | chip damage on an enemy (Jin's hit blocked) | -12.5 |
| **0x00000001** | **a successful parry (and a perfect parry): Jin gets +12.5 health** | every parry |
| **0x00000200** | **the parried enemy staggered** (0 damage); also after a heavy | parry / perfect parry / heavy |
| 0x00401001 | a perfect-parry counter kill (-FLT_MAX) | perfect parry |
| 0x00002000 | the enemy down / crawling (finisher state) | finisher tag |
| 0x00001000 | heavenly strike break | heavenly strike |
| 0x40000001 | resolve heal (+122.5) | healing |
| 0x04000001 | revived to 1 (the modded save) | |
| 0x01000041 | fall damage (-37.5) | ledges |
| 0x60000002 / 0x60000080 | set max health (spawn) | |

## What this means for the design doc

- **Light / heavy attacks:** the physical katana sends HandleHit(enemy, a copy of Jin's last real hit of that kind on that enemy class):
  - flags 0x03C00041 (heavy 0x03C00141 with the posture value at +0x98), the attacker = Jin at +0x48;
  - a fresh id at +0x2C, damage from the game's own value (-75 / -85 / -170) scaled by the swing.
  - When health would reach 0, the kill message follows, as the game does.
  - No template captured yet = no damage (fail closed).
- **Blocking / parry / perfect parry:** the game decides these from the BLOCK input's timing (Q on your keyboard).
  - The VR plan: when the katana meets the attacker's weapon, the mod presses block for the game at that moment. With speed (a
    deflect) it is a tap: the game's own timing gives parry or perfect parry. Held still in contact, block is held.
  - The result is read back from the messages: Jin +12.5 (0x1) = a parry; the enemy 0x200 = staggered; 0x401001 = a perfect-parry
    kill. That is the design doc's "Show contact and result".
  - The game's parry window decides, so the "parry window multipliers" become how early the mod presses.
  - Parry variant 2 (no player animation) = the mask from 0.18.39 over Jin's parry animation.
- **The finisher only on a crawler:** 0x00002000 marks the crawling state on that enemy. A sword hit on an enemy in that state lets
  the game's finisher play; any other death is the plain kill message.
- **Getting hit:** 0x03800041 / 0x11802041 on Jin -> haptics scaled by the damage. Fall damage (0x01000041) is separate.
- **Health / resolve on the forearm:** health = +0x1974 / +0x1970. Resolve is not found yet; the heal message (0x40000001, +122.5)
  marks its use.

## What the hero's own fields could NOT show (another way needed)

- **Standoff, wall climbing, ledges / ropes / logs:** no clean flag in the hero's first 16 KB. The standoff fields were Jin's ground
  position history (4 Hz).
- **0.18.45 watches 32 KB of the hero and the ENEMY last in a hit with Jin** (ENEMY/s), so its attack wind-up and stagger show.

# Session 3 (0.18.45, 2026-10-21): parry, standoff, traversal

- **Parry (numpad 4) and perfect parry (numpad 5):**
  - Both produce the SAME result message: Jin +12.5 health (flags 0x1). Missed presses produce no message.
  - The difference is timing. The block press (Q) came 64 ms before the message for the parry (mark 18) and 22 ms before it for the
    perfect parry (mark 21).
  - After the perfect parry, the next attack (LMB, 1.1 s later) killed the enemy in one blow: kill message, move 6, level 5.
  - So the game itself sorts parry from perfect parry by how close to the impact block was pressed.
  - **VR parry:** at blade-on-weapon contact, the mod presses block for the game; with speed it is a tap. The game decides parry or
    perfect parry; the mod reads the +12.5 message (and the enemy's 0x200 stagger) as the result. No enemy wind-up flag is needed.
  - The enemy's own fields showed only event queues ticking (0x10C0..0x1700), no clean "attacking" flag.
- **Standoff:** about 1.5 s after X:
  - hero+0x1070 0 -> 1, hero+0x1084 48 -> 305, hero+0x106C a reference to the opponent / encounter, hero+0x107C 0 -> 8;
  - a win = the opponent's kill message after the release; a loss = a -349 hit on Jin.
  - In session 1, +0x1070 = 2 / +0x1084 = 305 also appeared when a fight started (the combat mode; to confirm).
  - **VR:** standoff active = +0x1070 == 1. The design doc's strike ("unsheathe to the drawing distance") is answered with the game's
    standoff release, while its animation is masked.
- **Ledges / ropes / logs / beams:**
  - hero+0xB60..0xB98 (two plane equations: a unit normal + a distance) and the matching blocks at 0xCDC..0xD1C change almost ONLY
    in this tag (13 of 14). Each change is 0.5 - 0.9 s after a step-on mark (1, 5, 7): the traversal element Jin attaches to.
  - **Missing:** the step-off is not visible there.
  - **VR plan:** a change of this plane = hand the body / arms to the game's animation (the view stays first person). Hand them back
    when Jin walks normally again: the plane unchanged, speed and pose back to plain walking. Thresholds to tune in the headset.
- **The dog:** class 16, confirmed (watched as the enemy at 573797578 / 574104000), not fought.
- **The enemy watcher:** 0.18.45 also jumped to spawning enemies (their set-max-health message). 0.18.46 follows real hits only.

# Session 4 (0.18.47, 2026-10-21): how the game builds a hit, and how the katana sends one

Read from the binary with Ghidra; addresses are rva for this build, and each one GoTVR calls is signature-checked.

| What | Where | Notes |
|---|---|---|
| Hit-message pool (allocate) | 0x7686D0 | A free list of 0x210-byte messages |
| Message base / vtable | 0x84B420 / 0x11F3B70 | The base sets defaults (lists' cap fields 0x14, handles 0, id -1) |
| Constructor (msg, target) | 0x84B390 | base + vtable + fresh id (0x9321F0) at +0x2C + the target handle at +0x30 |
| Full constructor (msg, desc) | 0x84B1A0 | Used by the composer: attacker +0x48, +0x50, the tags, the flag payloads |
| Destructor / deleting destructor | 0x84B800 / 0x84BD70 | Frees the lists, unregisters the handles, releases the id unless +0x28 == 3, returns the message to the pool |
| Weak-handle set (slot, object) | 0x84BB50 | Unregisters the slot from the old object's +0x10 list (0x842420) and registers it with the new one (0x842330). A dying object nulls every registered slot. |
| Tag-list copy (dst, src) | 0x519DB0 | Lists are {ptr, count, ..., cap at +0x14}, with u64 hashes |
| The composer (Jin's attacks and others) | 0x850880 | Pool → 0x84B420 → id → target → refcount → 0x858FD0 / 0x84C820 (fill) → 0x84D2A0 (chain / queue) |

The message layout:
- `+0x08` refcount (the high dword is the 0xBEDFAD96 tag); `+0x2C` id; `+0x30` target; `+0x38` damage kind; `+0x40` move.
- `+0x48` attacker; `+0x50` a handle that stays the same for each hero (its weapon or attack source, presumably).
- `+0x58` tags (count `+0x60`); `+0x78` tags (count `+0x80`). 0.18.43 read these counts as "level" and "category".
- `+0x90` flags; `+0x94` health change; `+0x98` posture change; `+0x9C`..`+0xA4` optional, per flag bits 2 / 8 / 0x80.
- `+0x168` a handle (flag 0x800000); `+0x170` 48 bytes (flag 0x01000000); `+0x1A0` (flag 0x02000000).
- `+0x1C8` list (stride 0x18, count `+0x1D0`): extra hit entries. `+0x1E0` list (stride 8, count `+0x1E8`).

What HandleHit does with the message, in order:
1. Drops repeats (the id ring at +0x1990).
2. Logs the hit (the history at +0x19E0).
3. Bit 21: a reset. Bit 20: the kill path.
4. Health and posture through SetHealth / 0x69DF00, with the lethal checks.
5. The state machine 0x69ED10, passing the message.
6. The listeners: character+0x9C0 chain, each called through vtable +0x88.
7. Remembers the last attacker (+0x1988).
8. The AI threat record (character+0x3670), with the tags copied.

How the katana's blow is sent (cbt::strike, game thread, after the tick):
1. Allocate from the pool and construct on the target.
2. Take our own reference.
3. Fill in the template: damage kind, move, attacker = Jin, the +0x50 / +0x168 handles, the tags (copied with the game's own
   copy), and the numbers and payload (positions re-based on the target).
4. Scale the damage by swing speed. A blow that would kill is sent the game's way: 0x40 cleared and −FLT_MAX.
5. Call HandleHit directly.
6. Drop our reference. If nobody kept the message, the game's own deleting destructor puts it back in the pool.

**To check in the first live run (flat):**
- `STRIKE ... health a -> b` matches the template's damage.
- The enemy reacts (flinch, sound, blood) as on a game blow.
- A killing blow kills with the death reaction.
- Nothing is left in the pool (no "the game kept the message" in the log).
- The `+0x170` payload log shows what those 12 floats are: position, direction, normal.
- `+28` of a live message is 3; ours stays 0, so the id is released.

Still to find:
- How an enemy's guard turns Jin's blow into the 0x19C00841 guarded variant. Our blows ignore the enemy's guard for now.
- The heavy template comes free from one game heavy attack.

## Session 5 (0.18.47 live, flat, 2026-10-21): the first injected blows

- **The template** comes from Jin's own light hits: flags 0x03C00041, -75 health, damage kinds 0x07222524 (moves 2 / 6) and
  0x0672DF0E (move 14, -75 posture), 3+2 or 4+4 tags, lists +1D0 / +1E8 empty.
  - **+0x168 is the attacker character** (Jin, class 15); flag 0x800000 = "attacker handle present". +0x50 is a class-24
    object, probably the weapon.
  - **The +0x170 payload is three vectors:**
    - the impact point (about 1.1 m above the target's feet), w = z;
    - a vector of a few hundred to a few thousand units, w = 3 (the blade's velocity or impulse?);
    - a unit vector (the hit direction).
- **The blows themselves work:**
  - Every strike took exactly the template's health (255 → 180 → 105 → 30) and the killing blow killed (flags 0x03C00001).
  - All 87 HandleHit calls ran on the game thread, none off it.
  - The game kept no message ("kept the message" never logged), so the pool is clean. No crash.
- **No reaction** (owner: "they just took damage or died on the spot"): no flinch, sound or blood.
  - On a real hit the enemy's +0x1694 (= 868), +0x1698 (= 1) and +0x16A4 (= 8) are set within about 100 ms; an injected
    blow never sets them.
  - HandleHit is only the victim's bookkeeping (health, posture, history, listeners). The reaction, sound and blood come
    from the attacker's side (Jin's attack, the composer 0x850880 and what it calls on contact).
  - Next: find that call and make it too.
  - Writers found by displacement scan (for that search): +0x1694 at 0x1053DD3; +0x1698 at 0x4110C0, 0x6FB65B, 0x8DF6B3,
    0x10B017D / AF5 / C31; +0x16A4 at 0x97CFEA.
- **Hostility gate too strict:** most hotkey presses were refused with "no hostile enemy within 6 m". An enemy only became
  hostile after it and Jin exchanged a hit. That timing is why Ctrl+Shift+J looked necessary; it isn't, it only lists.
  The refusal log is limited to one line every 2 s, so many presses looked like nothing happened.
  - Fix: count every enemy that is alerted / in combat with Jin, not only those already in a hit.
- **Resolve heal** (W0): flags 0x40000001 +122.5, sent from 0x85A9A9 ← 0x85A524 ← 0x68CF11 ← 0x712061 ← 0x71367F ←
  0x71A754 ← 0x71A2A9 (the ability chain). The resolve spend should be in 0x71xxxx; decompile next.

## Session 6 (0.18.48 flat, 2026-10-21): the reaction is in the delivery, not in HandleHit

- **HHDIFF:** the enemy object changes identically under a real blow and an injected one. Health, +0x1990.. (the
  dedupe ring), +0x19D0.. (history) and +0x1D20..+0x1D94 (the hit record) change, plus +0x3AA4 (the time of the last hit)
  and +0x16D0 (a counter).
- **The real blow's caller chain:** 0x85A9A9 (Deliver) ← 0x85A524 (Deliver2) ← 0x850677 (HitProc 0x8504D0) ← 0x85B024
  (the hit loop 0x85AE40) ← 0x59E670 ← 0x4C4E6D (the job).
- **Deliver 0x85A770** is where the reaction is born: event 0x16 on the target ("hit received") and 0x17 on the attacker
  ("hit dealt"); the shake for flag 0x08000000; impact effects for flag 0x400; then HandleHit through vtable+0x1D8.
  0.18.49 delivers the katana's blows through Deliver2 0x85A4F0, the way the game's own resolve heal (0x68CE70) is sent.
- **Jin's own light hit message:** +0x100 / +0x120 / +0x140 lists empty; +0x158 = -1; +0x168 = Jin; +0x1A8 / +0x1B0 = -1
  (no impact effect ids); flag 0x400 not set. So the message the mod builds lacks nothing; the delivery was the missing
  piece.

## Session 9 (0.18.52 flat, 2026-10-22): the flinch lives in HitProc, before the delivery

- **HITPROC** (the game's hit processing 0x8504D0, logged for real blows): the hit record's +0x60 is the same object as the
  message's +0x50 (Jin's weapon / attack component, class 24). Its callbacks:
  - +0x60 vtable+0xA8 = 0x846290 (fires event 0x11) and +0xB0 = 0x872F00 (0x872F50, net only, and event 0x12);
  - +0x78 vtable+0xC0 = 0x8466B0 (event 0x14) and +0xC8 = 0x870FF0 (event 0x15; when the hero struck, it sets a flag
    in the target's +0x9C8 component ring, +0x438).
- **EVENT:** the victim itself only gets 0x16 / 0x17 from the delivery, the same as ours; nearby enemies get 0x95 (from
  0x70F514, an alert). So the difference between a real blow and ours is entirely on the attack side: the composer and
  the four attack-component callbacks, which the mod's message path never ran.
- Our strikes still did not flinch: the sound plays (Deliver), the damage lands, no reaction.
- **0.18.53:** `cbt_mode 2` runs HitProc itself on a copy of Jin's latest real hit record (captured in the always-installed
  HitProc hook), with +0x48 = the enemy, +0x58 = Jin, the contact point moved onto the enemy and the record's reference
  count held high so the game never frees our static copy. The `injecting` flag keeps it out of the template capture.
  Untested.
- **Open:** in VR a record must be captured each session (one real blow). A later step: build the record from the attack
  component and an attack definition, without needing a real blow first.
- **Resolve (W0):** 6 gain events, 0 spends in the scan. The candidate that matches every gain is hero+0x43A4 (float,
  pips; fractional gains, a copy at [hero+0xB8]+0x7BC). The maximum is still unknown; 0.18.53 logs hero+0x4380..0x43BF
  every 5 s (`VITALS`).

## Session 10 (0.18.53 flat, 2026-10-22): the whole hit path is not enough - the attack must be in progress

- One real light blow, then 11 strikes through `cbt_mode 2` (the game's HitProc on a copy of Jin's record). Every strike
  composed the same message as the real one (hitInfo identical apart from ids and pointers), changed the same enemy fields
  (DLVDIFF) and fired the same events (0x16 on the enemy, 0x17 on Jin). Damage and kills landed. No flinch.
- So nothing in the hit itself is missing. What differs is Jin's state: a real blow arrives during his attack. The class-24
  attack component's owner (+0x28) keeps the attacks in progress (+0x350 array of 0xE0 entries, the attack hash first;
  +0x358 count). The callback 0x870FF0 asks 0x871A50 whether the record's attack hash (+0x80) is in that list and only
  then marks the target's +0x9C8 ring entry (+0x438 = "hit by the hero's attack"). With Jin idle the list is empty.
- **Correction (same night):** the owner +0x350 / +0x358 list is Jin's whole MOVESET (attack definitions by hash; per-attack
  state in the component's +0x5B0 array, 0x58 each), not "attacks in progress". 0x871A50 → 0x872990 is a static lookup
  ("is attack H in group G", groups at owner +0x360 / +0x368), so the 0x870FF0 ring mark does not depend on the attack
  playing either - it ran for our strikes too. The "in progress" hypothesis has no evidence in the code; it is tested
  empirically instead (flat swing test, Ctrl+Shift+G).
- **0.18.54:** no attack button is ever pressed (owner: no attack animations in VR). Diagnostics: the whole delivered
  message real vs ours (MSGDIFF; HitSend 0x850880 reads the attacker's animation state while composing the back half,
  which was never compared), and the swing test. `cbt_assist=1` keeps the animated method only for comparison.
- **Resolve maximum** (static, from the decompiles of every reader of hero+0x43A4): max = hero+0x4644 + 3.0. 0x8EA6D0
  refills to it, 0x8F7200 divides by it, 0x8E1C80 / 0x8E2470 / 0x905FE0 clamp to it. 0x8EADE0 returns the maximum instead
  of the value while flag 0x8F is set (an unlimited-resolve state). The only writer is 0x8FB210 (also mirrors it to
  [hero+0xB8]+0x7BC). 0x8E56B0 empties it.
- Two resolve heals were logged (the caller chain is the script native chain 0x712061 / 0x71367F / 0x71A754 as before).

## Session 11 (0.18.54 flat, 2026-10-22): the reaction comes from the attack's data - found

- The first real blow was attack 0x95611255ef4e58cc. The game composed flags 0x11C01041 (bit 0x10000000 and 0x1000 more
  than session 10's 0x03C00041 from attack 0x74bf02936aeeeb47). Ctrl+Shift+H strikes on that record (Jin standing still,
  no animation) made enemies flinch (owner: "definitely"). So the flinch is decided by the attack definition the record
  points at (+0x50 holder → definition), not by Jin's animation; bit 0x10000000 of the composed flags marks it.
- Swing test (Ctrl+Shift+G): the blow during the owner's own swing worked the same, so a playing attack adds nothing.
- Records are 0x120 bytes (consecutive records 0x120 apart). +0xE0 = the base damage (75.0 for Jin's light, 100.0 for
  the enemies' blows); the composer adds upgrades (75 → 85 later in the session).
- MSGDIFF (whole message, ours vs the latest real one): only ids, pointers, the contact geometry (+0xB0..+0xD8, +0x170)
  and the flags' 0x1000 / 0x800 bits (direction) differ.
- **Insta-kills:** at 601549218 a real blow of Jin's (flags 0x03C00001, -FLT_MAX, +0x1000 follow-up) became the template;
  every strike after it killed outright. Fixed in 0.18.55 by judging each record by the blow the game composed from it.
- Jin died once mid-test; the hero object changed (new pointer). Records from before a respawn must not be used with their
  old pointers (0.18.55 re-derives them from the new Jin, or waits for a new blow).

## Session 12 (0.18.55 flat, two runs, 2026-10-22): records not saved; the game's own combo flags

- Run 1: real blows on one enemy in combo order: 0x03C00041 (opener, no reaction), 0x11C00841 (second, reaction), then
  0x03C00001 with -FLT_MAX at 150 health (the game's own finisher). A heavy-ish 0x01401141 (-12.5) and 0x00008000 also
  seen. The library kept 0x74bf08798839b183 (0x11C00841); H flinched; K killed (it is the kill key).
- REC: component at hero+? NOT found (not directly in the hero's first 32 KB); +0x68 object = component+0x318; holder not
  inside the moveset entry for the hash (or the entry not found) - nothing saved.
- Run 2: no saved record → message path, then the first real blow (opener, no reaction bit) became the record → no flinch.
- One H strike killed a 300-health enemy: the game composed 0x03C01041 with -FLT_MAX itself (not the record) - most likely
  the unaware-enemy one-hit kill.
- 0.18.56: two-level component search, own definition cell (all readers use only **(rec+0x50)), REC near diagnostics,
  message templates prefer the reaction bit, Ctrl+Shift+M message-path comparison.

## Session 13 (0.18.56 flat, 2026-10-22): Ctrl+Shift+M crash; component found, definition not

- Library: light 0x74bf08798839b184 / ...b183 (0x11C01041, reaction), heavy 0xb9ea921bfd74396d (0x03C00141, posture
  -150). H flinched.
- REC: component = (hero+0x408)+0x230; +0x68 = component+0x318; moveset 209 attacks, entry found by hash; holder NOT in the
  entry; definition (0x1d8b..., an asset heap, def+0x88 = 0x7222524 / 0x672df0e, move 538 / 6 / 14) not in the entry,
  hero, component or owner; no heap pointer in component / owner / +0x68 within 16 KB below the holder.
- Ctrl+Shift+M with the light template 0x11C01041 (+0x38 0x672df0e, move 14): the game froze inside the blow (census
  tick=0 after it; no STRIKE line). DBW watchpoints were also armed. Cause taken as the reaction bit on a message the game
  did not compose (the reaction needs what HitSend fills in). 0.18.57: message path never carries 0x10000000.

## Session 14 (0.18.57 flat, 2026-10-22): the method is settled; the definition is deeper

- H (record path, light 0x95611255ef4e58cc, 0x11C01041) flinched; M (message path, plain 0x03C00041 template) did 75 damage
  four times with no flinch and no crash. The ini's reacting message template was dropped at load as intended.
- REC: component (hero+0x408)+0x230, +0x68 = component+0x318, moveset 209, entry found. Definition 0x1343ebe0720 (asset
  heap; +0x88 0x672df0e, move 14) not within one step of the moveset entry (0xE0: hash, a 2nd hash, -1s, 1, ptr+0x1C,
  ptr+5, 9, ptr, ...) or of the component's per-attack entry (0x58 at component+0x5B0[index]). 0.18.58 searches 3 steps.
- Real records carry +0x90 the contact point, +0xA0 the blade's velocity (cm/s, e.g. (-955, 297, 327)), +0xAC 3.0,
  +0xB0 its direction (normalised). 0.18.58 writes the VR blade's contact point and tip velocity there.

## Session 15 (0.18.58 flat, 2026-10-22): definitions live apart; the reaction depends on the combo

- 49 hits. Light records seen: 0x74bf02936aefda5e (move 2), 0x74bf02936aeeeb47 (move 8), 0x0e406240d0e87cd2 (move 538),
  0x74bf08798839b183 (move 14 - 0x03C00041 here, 0x11C00841 / 0x11C01041 in sessions 12-13), 0x74bf02936aeeeb48 (move 6),
  0x522baba1b8c7943c (move 14, 0x11C00841 - kept). Heavy 0xb9ea921bfd74396d (0x03C00141, 225). The reaction bit is not a
  fixed property of the attack: it depends on where the blow falls in the combo / the enemy's state.
- Definitions at 0x17135df0720 / 0930 / 0e00 / 0eb0 / 0f60 (0xB0 apart: one asset array), moveset entries at
  0x17134f0xxxx, holders on the heap (0x3fea2399718 / 830 / 880 ...). No path of ≤ 3 steps from the moveset entry or the
  per-attack entry (component+0x5B0, whose qwords +0x10..+0x38 are six heap objects 0x3C0 apart) reaches the definition.
- 0.18.59: the saved record carries the definition's 0xB0 bytes (only +0x88 / +0x9C / +0xA4 are read by the game's hit
  path); records load at start.

## Session 16 (0.18.59 flat, 2026-10-22): records saved; dogs

- Every REC line ended "SAVED for VR": component (hero+0x408)+0x230, +0x68 = component+0x318, definition carried (0xB0).
  ini: light 0x522baba1b8c7943c (0x11C00841, 75), heavy 0xb9ea921bfd74396d (0x03C00141, 225).
- Reacting light blows seen: 0x74bf08798839b183 (0x11C01041, 170 - a bonus) and 0x522baba1b8c7943c (0x11C00841, 75).
  "Light" (no 0x100) blows of 150 (0x74bf02936aeeeb4a) and 225 (0xb9ea921bfd74396d, the heavy's own hash) also seen.
- A real hit on a dog (class 16): 0x11C01041, -85, 125 → 40 - dogs react to the same bit. Records were people-only (class
  14); 0.18.60 keeps dog slots too.


## Session 18 (0.18.61 VR, 2026-10-22): first parry test; flicker; heavy never fired

- PARRY: the blade history covered 76 ms (pushed several times per frame) - too short for timing. Both blows that met the
  blade were "replayed" after the game had auto-sheathed its sword 0.1 s earlier, so no guard: the game needs its sword out.
  0.18.62: one sample per 8 ms (HN 64), draw chord on a guard (+450 ms), keep-drawn LB tap while an enemy is within 8 m.
- Two hands: the explicit spring had a 15 ms period with two hands (stiffer) - unstable at 11-14 ms frames; tip 1600+ m/s,
  HEAVY spam. 0.18.62: implicit (backward-Euler) step.
- Heavy: the charge was reset by > 10 cm of hand drift during the 450 ms hold; then the trigger + swing never coincided.
  0.18.62: 25 cm, charge kept 300 ms into the swing (heavyUntil), one hand allowed.
- Owner question: "Can you not put a hitbox on the player held sword for parrying?" - 0.18.62 parry_mode=blade: the
  enemy's weapon = its right hand + 90 cm along its forearm (people share Jin's rig; joint 637 used when in that hand),
  judged against the katana blade capsule; falls back to the blow's line (dogs).

## Session 22 (0.18.66 flat GUARD SCAN, 2026-10-22): Jin's guard flag
- 12 block holds (Q, 1 s on / 1 s off, sword drawn, no enemies): pre == after != held for hero+0x14C 12/12 (0x3000104 -> 0x3000105,
  bit 0 = guarding); float vectors hero+0x1E0/0x1E4, 0x200/0x204, 0x240..0x248, 0x3B0/0x3B4 non-zero only while guarding (guard
  direction / pose data); hero+0xFB4 -34 vs -27 (float); hero+0x2D0 11/12; hero+0xF60 0x100 -> 0x101 in the first 3 holds only.
- 0.18.67 hybrid parry: bit 0 set around oHitProc for the defended blow; composed flags logged (0x00400000 = guarded).

## Session 24 (0.18.69 flat HIT SCAN, 2026-10-22): where the guard and the parry are decided
- 11 enemy blows on Jin: 6 unguarded (composed 0x03800041, -87.5 / -100), 4 on a held guard (0x03800000, 0 damage, block held 422-1797 ms),
  1 through the guard after 1797 ms (0x03800041 - a guard break / unblockable?). The guard is decided before HitProc (the attack side).
- Game parries (4) never reach HitProc on Jin: only the +12.5 reward (flags 0x00000001, HitProc record Jin -> Jin, from 0x84d252).
- Hero dwords that never shared a value guarded vs unguarded: +0x148 (0x4000100 vs 0x6000100), +0x10D0.. stride 0x18 (6 vs 4),
  +0xF80 / +0xF84 / +0xFB0 / +0xFB4 floats, +0xEDC / +0xEEC / +0xF1C (-4.77e6 sentinel vs 15.4), +0x250 (pointer vs 0).
- 0.18.70: the game's own guard button driven by the katana (guard engine).

## Session 27 (0.18.74 flat GUARD TRACE, 2026-10-22): the parry is an interaction; enemy blows skip the hit submit
- 25 enemy blows (4 guarded 0x03800000, 2 through a held guard, 19 unguarded; the spearman / brute 0x11802041 -125), 5 parries, 8 BLUE /
  ~6 RED glint tags. 0 enemy blows went through HitSubmit 0x84CEA0 - they reach HitProc 0x8504D0 another way (round 2 logs the chain there).
- Parry reward (+12.5, Jin -> Jin record +0x30 0x1087, hash -1) submitted from 0x71367f (script effect apply, also used by resolve heals)
  <- 0x71a754 <- 0x71a8f6 <- 0x5758ab <- 0x574f84 (event listener, filters id) <- 0x4e5820 = EventBus::Broadcast 0x4E5780(bus 0x141CDDD30,
  id, args) <- 0xB2043C in 0xB1F130 = Interaction::SetState(obj, state, params): obj+0x500 state, +0x28 owner, +0x510 partner handle; at the
  end it broadcasts 0x98 (owner) and 0x99 (partner). Called with state 5 from 0xB26401 in 0xB25D40 (the interaction update: 2/3 -> 4 -> 5
  on timers obj+0xEF8 / +0xEFC) <- 0xB24110 <- 0xB216B0 (update) <- 0xB5ACB7. Event args: 16-byte variants, count at +0x80 (0x4E4300 adds,
  type 0xE = entity).
- Parries were fresh block presses 0-79 ms old at the reward (the 5000 ms one was a re-press during a long hold).
- Glint object diffs (enemy +0..0x3000 every 100 ms) are dominated by transforms - no stable field; script events (hook 0x7E05F0) only 0x17
  from the hit delivery. Round 2: event-bus ids.
- Hero read watchpoints (+0x148 / +0xED8 / +0x10D0 / +0xF80): ~600k hits, all frame-regular; nothing blow-specific. Off in round 2.

## Session 28 (0.18.75 flat round 2 + video, 2026-10-22): the guard is in the record; attacks are interactions
- Enemy blows reach HitProc from the queue flush 0x85AE40 (HitSubmit queued them on another thread - round 1 only watched the game thread).
- Guarded records at HitProc vs unguarded: +0x34 = -1 (unguarded: a small index 0..0x1d), +0x40 = 9 (0), +0x80 hash 0x5aaff71d1cecac23/24 (the
  attack's own), +0xE0 damage 0 (100 / 125), tag count 2 (3). Ctrl+Shift+B wrote +0x34 = -1 and +0xE0 = 0 at HitProc: 16 / 16 blows composed
  0x03800000 (guarded), no damage, block not pressed - but no spark (video): the spark / clang presumably need +0x40 / the guard hash / tags.
- Every enemy attack is an interaction (0xB1F130): -1 -> 0 -> 1 (wind-up) -> 2 (strike; blows land here) or 3 / 4 (no contact) -> 4 -> 5 -> -1.
  Video: the blue glint shows 0.21-0.29 s after 0 -> 1, the strike 0.3-0.6 s after the glint. Parry reward at the parried attack's 2 -> 4 / 5.
- Video glints found by colour (blue: b > 200, b - r > 60; red: r > 220, r - g/b > 120) matched the owner's F6 tags within 0.2-0.35 s; video
  aligned to the log by the hit flashes (tools/glint_video.py). Event-bus ids: nothing glint-specific yet.

## Session 30 (0.18.77 flat round 3, 2026-10-22): guarded marks; the blue attack is its own definition
- Real guarded records (5): +0x34 = -1, +0x3C 0x101 (unguarded 0x100), +0x40 = 9 (0), damage 0, the attack's OWN hash (b3451229 13721652/53) -
  round 2's 5aaff71d hash was just another attack. Ctrl+Shift+N with the hash and the TAG LIST POINTER replaced crashed at once (dump
  GhostOfTsushima.exe.72876.dmp would not upload; the video froze on the blow's frame): the record owns +0xF0 and frees it.
- gtr_attacks.bin (44 dumps): one swordsman's four BLUE-glint wind-ups (video-labelled) share interaction +0x580 / +0xEE8 (the attack
  definition, 0x...39539c00) and params +0x190 hash 0xf488a0ce524d5919; his follow-ups use other definitions. The glint is a property of the
  attack definition -> v2 dumps carry 0x800 bytes of both definitions.
- Video freeze time and the single hit flash agree on the log offset within 0.12 s.
