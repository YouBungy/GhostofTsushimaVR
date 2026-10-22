#!/usr/bin/env python3
"""Find red / blue glints and hit flashes in a flat-test recording and line them up with gotvr.log.
usage: glint_video.py video.mp4 gotvr.log   (needs ffmpeg + numpy)
Alignment: the video's red hit flashes are matched to the log's HIT times (GTR HITREC ... HIT / HIT SCAN unguarded)."""
import sys, subprocess, numpy as np, re
vid, log = sys.argv[1], sys.argv[2]; W, H, FPS = 320, 180, 20
raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', vid, '-vf', f'fps={FPS},scale={W}:{H}', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
f = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3); n = len(f)
m = np.zeros((H, W), bool); m[:20], m[-20:], m[:, :30], m[:, -30:] = True, True, True, True
red = np.zeros(n); blue = np.zeros(n); rg = np.zeros(n)
for i in range(n):
    x = f[i].astype(np.int16); r, g, b = x[..., 0], x[..., 1], x[..., 2]
    red[i] = ((r - np.maximum(g, b)) * m).sum() / m.sum()
    blue[i] = ((b > 200) & (b - r > 60) & (g > 120)).sum()
    rg[i] = ((r > 220) & (r - g > 120) & (r - b > 120)).sum()
def events(a, thr, gap=6):
    out = []
    for i in np.nonzero(a > thr)[0]:
        if out and i - out[-1][-1] <= gap: out[-1].append(i)
        else: out.append([i])
    return [g[0] / FPS for g in out]
hits_v = events(red, np.median(red) + 4 * np.std(red), 10)
L = open(log, 'rb').read().decode('latin1')
hits_l = [int(t) for t in re.findall(r'\[(\d+)\] GTR HITREC #\d+ from \w+[^:]*: HIT', L)] or [int(t) for t in re.findall(r'\[(\d+)\] HIT SCAN: blow \d+ .*unguarded hit', L)]
best = None
for hv in hits_v:
    for hl in hits_l:
        off = hl - hv * 1000; sc = sum(any(abs((x - off) / 1000 - v) < 0.25 for v in hits_v) for x in hits_l)
        if not best or sc > best[0]: best = (sc, off)
print(f'video {n / FPS:.1f} s; hit flashes {len(hits_v)}, log hits {len(hits_l)}; aligned {best[0]} hits, log ms = video s * 1000 + {best[1]:.0f}')
for name, ev in (('BLUE', events(blue, 6)), ('RED', events(rg, 10))):
    for v in ev: print(f'{name} glint at video {v:7.2f} s = log {v * 1000 + best[1]:.0f}')
