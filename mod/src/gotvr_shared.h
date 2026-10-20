// GoTVR shared-memory protocol between the game DLL (xinput1_4.dll) and GoTVRHost.exe.
// The host owns the OpenXR session in its own process (playbook ch09 out-of-process helper, ch07: keep
// the runtime out of a process full of overlays/interposers). The game shares its eye images as D3D12
// NT shared textures plus one shared fence; poses travel the other way through this block.
#pragma once
#include <windows.h>
#include <stdint.h>

#define GV_MAGIC 0x52565447u /* 'GTVR' */
#define GV_VERSION 15u
#define GV_TEXCOUNT 6        /* 2 eyes x 3 buffers (game writes round-robin, never waits on the host) */
#define GV_UITEX 3           /* v8: UI-layer images, own size (the UI target's), names Local\GoTVR_<pid>_u<uiGen>_ui<i> */

// Names (wchar): Local\GoTVR_<pid>, Local\GoTVR_<pid>_g<gen>_tex<i> (new names per generation), Local\GoTVR_<pid>_fence
static inline void gvName(wchar_t *out, size_t n, uint32_t pid, const wchar_t *suffix) {
    _snwprintf(out, n, L"Local\\GoTVR_%u%ls", pid, suffix ? suffix : L"");
    out[n - 1] = 0;
}

struct GvPose { float q[4]; float p[3]; };   // OpenXR LOCAL space: x right, y up, -z forward, metres
// v12 canonical controller (one per hand). Buttons are physical roles, not game meanings:
//   PRIMARY = the lower face button (A on the right Touch, X on the left; Index A; WMR/Vive: trackpad click)
//   SECONDARY = the upper face button (B / Y; Index B; Vive / WMR: menu button)
//   MENU = the system-style menu button where a controller has a separate one (left Touch menu)
//   STICK = thumbstick click (Vive: trackpad click), TRIGGER / GRIP = digital versions of the analog values
enum : uint32_t { GV_BTN_PRIMARY = 1u, GV_BTN_SECONDARY = 2u, GV_BTN_MENU = 4u, GV_BTN_STICK = 8u, GV_BTN_TRIGGER = 16u, GV_BTN_GRIP = 32u, GV_BTN_THUMBREST = 64u,
                   GV_BTN_THUMBTOUCH = 128u, GV_BTN_TRIGTOUCH = 256u };   // v14+: the thumb resting on the stick / face buttons / thumb rest; the index on the trigger (VR fingers)
struct GvHand {
    GvPose grip, aim;                 // LOCAL space (grip = palm, for hands and held objects; aim = pointing ray)
    uint32_t poseValid;               // 1 = grip valid, 2 = aim valid
    float stick[2];                   // thumbstick (or trackpad) x right, y up, -1..1, raw (the game DLL applies deadzones)
    float trigger, squeeze;           // 0..1
    uint32_t buttons;                 // GV_BTN_*
};
// fov as OpenXR angles in radians: left (<0), right (>0), up (>0), down (<0)

enum GvHostState : int32_t { GV_HOST_STARTING = 0, GV_HOST_NO_RUNTIME = 1, GV_HOST_SESSION = 2, GV_HOST_RUNNING = 3, GV_HOST_FAILED = 4, GV_HOST_EXITED = 5 };
enum GvMode : uint32_t { GV_MODE_MONO = 1, GV_MODE_STEREO_AFR = 2, GV_MODE_TEST = 9 };

struct GvSlot {                 // one per eye and buffer, written by the game at Present (seqlock: odd = writing)
    volatile LONG seq;
    uint32_t tex;               // index into the shared textures (eye*3 + buffer)
    uint32_t gen;               // texture generation it belongs to
    uint64_t fence;             // shared fence value signalled after the copy
    uint32_t frame;             // game frame id (Reflex frame token)
    GvPose pose;                // the eye pose this image was rendered with (LOCAL space)
    float fov[4];               // the frustum this image was rendered with (angles)
    uint32_t flags;             // v5: GV_SLOT_* (flat context: show on the theatre screen instead of the projection layer)
};
enum : uint32_t { GV_SLOT_THEATER = 1u, GV_SLOT_CROP169 = 2u, GV_SLOT_FLAT = 4u };   // CROP169: a 16:9 video letterboxed in the frame; FLAT: a normal flat game frame (no eye pose)

struct GvShared {
    uint32_t magic, version;
    uint32_t gamePid, hostPid;
    volatile LONG hostState;
    char hostMsg[160];
    volatile LONG64 gameBeat, hostBeat;  // GetTickCount64 of the last update on each side
    uint32_t mode;

    // host -> game: latest located views (seqlock)
    volatile LONG poseSeq;
    int64_t displayTime;
    GvPose head;                // VIEW in LOCAL
    GvPose eye[2];
    float fov[2][4];
    uint32_t poseValid;

    // game -> host
    volatile LONG texGen;       // bumped after all textures + fence exist under their names
    uint32_t texW, texH, texFmt;
    LUID gameLuid;              // adapter of the game's device (host must match)
    volatile LONG imagesOk;     // host: 1 once the current texture generation is open and its swapchains exist
    volatile LONG gameRefuse;   // game: non-zero = it stopped publishing (reason in the game log), e.g. frame generation on
    GvSlot slot[2][3];          // history: one entry per buffer; host picks the newest one whose fence completed

    // stats (diagnostics only)
    volatile LONG64 gamePublished, gameMismatch, hostCopied, hostReused, hostFrames;
    volatile LONG64 hostPairs;  // v4: host frames that switched to a newer complete stereo pair (xr=4)

    // v5 theatre screen (playbook ch04 HUD-002/003): game -> host settings, written once before the host starts
    float theaterDist, theaterWidth, theaterHeight;  // metres in front of you, screen width, centre height relative to your eyes
    uint32_t theaterBackdrop;                        // 0 = black room around the screen, 1 = the last game image behind it
    volatile LONG theaterRecenter;                   // game bumps it (F11) -> host re-places the screen in front of you
    volatile LONG64 hostTheater;                     // host frames shown as the theatre screen (diagnostics)

    // v7 UI layer (playbook ch04 "Get UI off the backbuffer", HUD-004): the game's UI render target, captured at full
    // resolution, on its own quad layer. slot.flags = GV_UI_*; pose/fov unused.
    GvSlot ui[GV_UITEX];              // slot.tex = UI image index, slot.gen = uiGen
    float hudDist, hudDeg, hudDown;   // HUD quad: metres in front, width in degrees, centre below eye level (degrees)
    uint32_t uiPremul;                // 1 = the UI image is premultiplied alpha
    volatile LONG uiOk;               // host: 1 once its UI-layer swapchain exists for the current generation
    volatile LONG64 hostUi;           // host frames that showed the UI layer (diagnostics)
    volatile LONG uiGen;              // v8: bumped after the UI images exist under their names
    uint32_t uiW, uiH, uiFmt;
    int32_t content[4];               // v8: x, y, w, h of the picture inside the eye images (the game letterboxes a 16:9
                                      // render inside a 16:10 window, for example); w == 0 -> the whole image
    // v9: where the room-fixed HUD panel is (the game DLL runs the lazy follow, HUD-002), LOCAL space (seqlock)
    volatile LONG uiPoseSeq;
    float uiPos[3];                   // panel centre
    float uiYaw;                      // panel faces you from this yaw (0 = straight ahead along -z)
    float uiWidth;                    // metres
    uint32_t uiPoseOk;
    // v10 (BUG-64): the headset render is square but the game lays out its UI, menus and flat frames for 16:9 (the mod tells
    // the game "16:9" while the image is square): every image is ANAMORPHIC - stretched vertically by dispAspect. The host shows
    // the UI image and theatre (flat) frames at this display aspect (width / height); 0 = the images' own pixel aspect.
    float dispAspect;
    // v11 (BUG-69: black edges / judder on fast head turns while the game itself keeps its frame rate). The image of a pair
    // reaches the display a whole pair (or more) after its pose was predicted, so the compositor warps it across that gap
    // and the frustum edge shows. The host predicts the pose for its display time PLUS the measured pipeline lag.
    float poseLeadCfgMs;              // game -> host: < 0 = automatic (measured), 0 = off, > 0 = fixed milliseconds
    volatile LONG poseLeadUs;         // host -> game: the lead in use (microseconds)
    volatile LONG poseLagUs;          // host -> game: how late new pairs reach the display relative to their prediction
    volatile LONG hostGpuPrio;        // host -> game: the host's GPU scheduling class (D3DKMT, -1 = could not set)
    // v12: VR controllers (playbook ch03 / INPUT-001: the host transports PHYSICAL controls of one canonical controller per
    // hand; the game DLL owns every game meaning). Each interaction profile (Touch, Touch Plus, Index, Vive, WMR, HP Reverb,
    // Pico, KHR simple) is bound onto the same canonical controls in GoTVRHost's profile table (xrinput.h).
    volatile LONG inSeq;              // seqlock over the input snapshot below
    uint32_t inActive;                // 1 = session focused and actions synced this frame (else: neutral)
    int64_t inTime;                   // XrTime the hand poses were located for (= the pose packet's display time)
    volatile LONG64 inSamples;        // snapshots written (diagnostics)
    GvHand hand[2];                   // 0 = left, 1 = right
    char inProfile[2][96];            // current interaction profile per hand ("" = none)
    // game -> host haptics
    float hapLevel[2];                // continuous rumble per hand 0..1 (the game's XInput motors: low -> left, high -> right)
    volatile LONG hapPulseSeq[2];     // bumped per one-shot pulse request
    float hapPulseAmp[2]; uint32_t hapPulseMs[2];
    // v13 overlay (game -> host, seqlock): GoTVRHost draws the item wheel at the hand and the head-gesture test indicator
    volatile LONG ovSeq;
    uint32_t ovWheelOpen; int32_t ovWheelSel; uint32_t ovWheelN; int32_t ovWheelHand; char ovWheelName[8][24];   // v14: up to 8 options
    uint32_t ovDebug;                 // 1 = show the head-gesture indicator (gameplay, [controls] gesture_indicator)
    uint32_t ovAtHead;                // bit 0 left hand, bit 1 right hand in the gesture zone
    uint32_t ovGestSeq[2]; char ovGestName[2][24];   // bumped per fired gesture, with its name
    // v15 (owner: "have the Meta controller reset position button properly call the reset view function in game"): host -> game,
    // bumped when the runtime announces a recentre of the LOCAL space (XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: holding
    // the Meta / Oculus button, or Virtual Desktop's own reset); the game runs its own recenter (= F11) right after the change
    volatile LONG hostRecenterSeq;
};
// head-locked HUD quad / world-locked panel / v9 FULL: the UI covers the whole view of each eye exactly like on the monitor
// (cinematics: letterbox bars, fades and subtitles at the screen edges) - one quad per eye on the frustum its image used
enum : uint32_t { GV_UI_HEAD = 1u, GV_UI_WORLD = 2u, GV_UI_FULL = 4u };

// seqlock helpers (single writer per field group)
static inline void gvWriteBegin(volatile LONG *s) { InterlockedIncrement(s); MemoryBarrier(); }
static inline void gvWriteEnd(volatile LONG *s) { MemoryBarrier(); InterlockedIncrement(s); }
template <class F> static inline bool gvRead(volatile LONG *s, F copy) {
    for (int i = 0; i < 8; i++) {
        LONG a = *s; if (a & 1) { YieldProcessor(); continue; }
        MemoryBarrier(); copy(); MemoryBarrier();
        if (*s == a) return true;
    }
    return false;
}
