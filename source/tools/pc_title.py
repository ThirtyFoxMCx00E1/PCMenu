#!/usr/bin/env python3
"""Move the "Grand Theft Auto San Andreas" title of the mobile loading pictures to the PC position / size
(bottom left, just above the PC style progress bar), like the PC game.

  pip install opencv-python numpy onnxruntime
  python3 pc_title.py [first last]            # default 0 27

Input : source/tools/lds_original/mobilesc<N>.png  (the untouched mobile pictures, 1024x512)
Output: assets/PCMenu/pc/lds/mobilesc<N>.png
Steps : 1. the title is found by comparing all pictures (it is the same in every one)
        2. the old title and its shadow are removed and the background is rebuilt (LaMa inpainting, ONNX)
        3. the title is drawn again, scaled to the PC size, with a soft shadow; its white letters cover
           x 0.0875..0.242 and end at y 0.878 of the picture (measured on the PC game), so the bar at
           x 0.091..0.238, y 0.897 sits right under "San Andreas".
To add mobilesc24..27: put the untouched pictures into lds_original/ and run the script again."""
import cv2, numpy as np, glob, sys, os, urllib.request, onnxruntime
HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "lds_original"); DST = os.path.join(HERE, "..", "..", "assets", "PCMenu", "pc", "lds")
MODEL = os.path.join(HERE, "lama_fp32.onnx"); URL = "https://huggingface.co/Carve/LaMa-ONNX/resolve/main/lama_fp32.onnx"
first, last = (int(sys.argv[1]), int(sys.argv[2])) if len(sys.argv) > 2 else (0, 27)
if not os.path.exists(MODEL): print("downloading", URL); urllib.request.urlretrieve(URL, MODEL)
sess = onnxruntime.InferenceSession(MODEL)
num = lambda f: int(os.path.basename(f)[8:-4])
allf = sorted(glob.glob(os.path.join(SRC, "mobilesc*.png")), key=num)
ims = {num(f): cv2.imread(f, cv2.IMREAD_UNCHANGED) for f in allf}
W, H = 1024, 512; X0, Y0, X1, Y1 = 50, 160, 410, 490
white = lambda im: (im[:, :, :3].min(2) > 240).astype(np.float32)
base = ims[min(ims)]; tpl = white(base)[175:465, 65:390]
offs = {}
for n, im in ims.items():
    r = cv2.matchTemplate(white(im)[150:490, 40:420], tpl, cv2.TM_CCORR_NORMED); loc = cv2.minMaxLoc(r)[3]
    offs[n] = (loc[0] + 40 - 65, loc[1] + 150 - 175)
crops = np.stack([im[Y0 + offs[n][1]:Y1 + offs[n][1], X0 + offs[n][0]:X1 + offs[n][0], :3].astype(np.float32) for n, im in ims.items()])
med = np.median(crops, 0); mad = np.median(np.abs(crops - med).max(3), 0); lum = med.mean(2)
core = ((mad < 14) & ((lum > 200) | (lum < 45))).astype(np.uint8)
k, lab, st, _ = cv2.connectedComponentsWithStats(core); mask = np.zeros_like(core)
for i in range(1, k):
    if st[i, 4] > 30: mask[lab == i] = 1
mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, np.ones((3, 3), np.uint8))
ff = mask.copy(); cv2.floodFill(ff, np.zeros((ff.shape[0] + 2, ff.shape[1] + 2), np.uint8), (0, 0), 1); mask[ff == 0] = 1
ys, xs = np.nonzero(mask); bx0, bx1, by0, by1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
logo = med[by0:by1, bx0:bx1].clip(0, 255).astype(np.uint8); la = mask[by0:by1, bx0:bx1].astype(np.float32)
s = ((0.242 - 0.0875) * W) / (380 - 70)
tw, th = int(round(logo.shape[1] * s)), int(round(logo.shape[0] * s))
px, py = int(round(0.0875 * W - (70 - (X0 + bx0)) * s)), int(round(0.878 * H - (456 - (Y0 + by0)) * s))
lg = cv2.resize(logo, (tw, th), interpolation=cv2.INTER_AREA).astype(np.float32)
al = cv2.resize(la, (tw, th), interpolation=cv2.INTER_AREA)
sh = cv2.GaussianBlur(cv2.copyMakeBorder(al, 12, 12, 12, 12, cv2.BORDER_CONSTANT, 0), (0, 0), 3.0) * 0.55
for n in sorted(ims):
    if n < first or n > last: continue
    im = ims[n]; bgr = im[:, :, :3].copy(); dx, dy = offs[n]
    m = np.zeros((H, W), np.uint8); m[Y0 + dy:Y0 + dy + mask.shape[0], X0 + dx:X0 + dx + mask.shape[1]] = mask
    rm = cv2.dilate(m, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (65, 65)))
    rgb = cv2.cvtColor(bgr[:, :512], cv2.COLOR_BGR2RGB).astype(np.float32) / 255
    o = sess.run(None, {"image": rgb.transpose(2, 0, 1)[None], "mask": rm[:, :512].astype(np.float32)[None, None]})[0][0].transpose(1, 2, 0)
    if o.max() <= 1.5: o = o * 255
    o = cv2.cvtColor(o.clip(0, 255).astype(np.uint8), cv2.COLOR_RGB2BGR).astype(np.float32)
    fm = cv2.GaussianBlur(rm[:, :512].astype(np.float32), (0, 0), 2)[..., None]
    bgr = bgr.astype(np.float32); bgr[:, :512] = bgr[:, :512] * (1 - fm) + o * fm
    reg = bgr[py - 10:py - 10 + sh.shape[0], px - 10:px - 10 + sh.shape[1]]; reg *= (1 - sh[..., None])
    reg = bgr[py:py + th, px:px + tw]; bgr[py:py + th, px:px + tw] = reg * (1 - al[..., None]) + lg * al[..., None]
    res = bgr.clip(0, 255).astype(np.uint8)
    if im.shape[2] == 4: res = np.dstack([res, im[:, :, 3]])
    cv2.imwrite(os.path.join(DST, "mobilesc%d.png" % n), res); print("mobilesc%d.png" % n)
