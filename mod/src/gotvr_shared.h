// GoTVR shared-memory protocol between the game DLL (xinput1_4.dll) and GoTVRHost.exe.
// The host owns the OpenXR session in its own process (playbook ch09 out-of-process helper, ch07: keep
// the runtime out of a process full of overlays/interposers). The game shares its eye images as D3D12
// NT shared textures plus one shared fence; poses travel the other way through this block.
#pragma once
#include <windows.h>
#include <stdint.h>

#define GV_MAGIC 0x52565447u /* 'GTVR' */
#define GV_VERSION 10u
#define GV_TEXCOUNT 6        /* 2 eyes x 3 buffers (game writes round-robin, never waits on the host) */
#define GV_UITEX 3           /* v8: UI-layer images, own size (the UI target's), names Local\GoTVR_<pid>_u<uiGen>_ui<i> */

// Names (wchar): Local\GoTVR_<pid>, Local\GoTVR_<pid>_g<gen>_tex<i> (new names per generation), Local\GoTVR_<pid>_fence
static inline void gvName(wchar_t *out, size_t n, uint32_t pid, const wchar_t *suffix) {
    _snwprintf(out, n, L"Local\\GoTVR_%u%ls", pid, suffix ? suffix : L"");
    out[n - 1] = 0;
}

struct GvPose { float q[4]; float p[3]; };   // OpenXR LOCAL space: x right, y up, -z forward, metres
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
