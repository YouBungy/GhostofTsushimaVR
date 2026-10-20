# GoTVR audio ownership (playbook ch20 "Minimum audio ownership document")

- **Engine:** Wwise (AK::SoundEngine, exported by GhostOfTsushima.exe). The mod steers the game's own listener; it does not replace the
  mixer. Distance attenuation, occlusion, reverb, music and UI stay the game's.
- **Listener source:** first person in VR (xr=4, first person active): the head = OpenXR VIEW pose -> recenter reference -> the eye
  anchor + level yaw + VR-owned view yaw (the same compose() the eyes use) -> game world (cm). Published from hSlot43 at pair phase 0
  (seqlock, 250 ms freshness). Otherwise: the game's CAM-B camera.
- **Where it enters:** Audio_ListenerFromCamera 0x5BAC80 reads CAM-B +0x890 (rows fwd / left / up / pos). The hook lends it the head
  for the call (then restores CAM-B), so the game derives listener 0-2, object 3, the inverse and its focus blend / smoothing itself.
- **World -> audio transform:** the game's (0x1420A1CD0, front / top y mirrored); untouched, so emitters and listener stay consistent.
- **Authored camera / cutscene policy:** cutscenes (camera cut), flat / theatre contexts and third person: the game's listener.
- **Lanes:** world spatial = Wwise emitters (game); listener-relative UI / music = Wwise busses (game, unchanged).
- **Known gap:** the player's own emitters (voice, gear) sit within cm of the head listener and may pan on head turns - next step:
  centre them (God of War VR D-162).
- **Haptics:** the game's XInput rumble -> both controllers (host); no event haptics yet.
- **Tests:** self-test "3D audio test" (swap during the call, restore after, off / third person untouched). Headset: a voice on the
  left moves to the right ear after turning 180 deg; a sound behind stays behind when you look around.
