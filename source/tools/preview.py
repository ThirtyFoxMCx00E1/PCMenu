#!/usr/bin/env python3
"""Pixel-faithful preview of the PCMenu layout math in main.cpp (same constants), for checking against screenshots."""
import sys, math
from PIL import Image
ROOT = sys.argv[1]; W = int(sys.argv[2]); H = int(sys.argv[3]); mode = sys.argv[4]; out = sys.argv[5]
S = 1.0
GOLD=(171,131,12,255); DIM=(66,57,10,255)

class Font:
    def __init__(s, base):
        t = open(f"{ROOT}/pc/fonts/{base}.txt").read().split("\n")
        _, size, asc, desc, cap, aw, ah = t[0].split(); s.cap=int(cap); s.aw=int(aw); s.ah=int(ah)
        s.img = Image.open(f"{ROOT}/pc/fonts/{base}.png").convert("RGBA"); s.g={}
        for l in t[1:]:
            if l.startswith("G "):
                _, c, x, y, w, h, xo, yo, adv = l.split(); s.g[int(c)] = (int(x),int(y),int(w),int(h),int(xo),int(yo),float(adv))
    SPX=0.0; XS=1.0; BOLD=0.0
    def width(s, txt, sc, tr): return sum(s.g[ord(c)][6]*sc*s.XS + (s.SPX if c==' ' else 0) for c in txt) + tr*(len(txt)-1)

def text(canvas, f, txt, x, base, sc, tr, col):
    pen = x
    for ch in txt:
        gx_, gy_, w, h, xo, yo, adv = f.g.get(ord(ch), f.g[ord('?')])
        if w and h:
            gl = f.img.crop((gx_, gy_, gx_+w, gy_+h))
            tw, th = max(1,round(w*sc*f.XS)), max(1,round(h*sc))
            gl = gl.resize((tw, th), Image.BILINEAR)
            a = gl.getchannel("A"); tint = Image.new("RGBA", gl.size, col); tint.putalpha(a)
            canvas.alpha_composite(tint, (int(round(pen + xo*sc*f.XS)), int(round(base + yo*sc))))
            if f.BOLD>0: canvas.alpha_composite(tint, (int(round(pen + xo*sc*f.XS + f.BOLD)), int(round(base + yo*sc))))
        pen += adv*sc*f.XS + tr + (f.SPX if ch==' ' else 0)

bank, beck = Font("bank"), Font("beck")
SPXF=float(sys.argv[7]) if len(sys.argv)>7 else 0.0
cv = Image.new("RGBA", (W,H), (0,0,0,255))
aw = min(W, 1.25*H); ax0 = (W-aw)/2; cx = W/2
bank.SPX=0.011*H; bank.XS=1.12; bank.BOLD=0.0012*H; beck.SPX=0.006*H
itemScale = (0.0215*H*S)/bank.cap; titleScale = (0.0947*H*S)/64.0; track = 0.0022*H
rowStep = 0.0669*H*S; firstY = 0.336*H; curH = 0.074*H*S; curW = 0.0547*H*S; curGap = 0.019*H
back = Image.open(f"{ROOT}/pc/images/back8.png").convert("RGBA")
bw = 0.461*aw; bh = 0.47*H; x1 = ax0+aw
cv.alpha_composite(back.resize((round(bw), round(bh)), Image.BILINEAR), (round(x1-bw), 0))
title = "Pause Menu" if mode == "pause" else "Main Menu"
text(cv, beck, title, ax0+0.068*aw, 0.0771*H + beck.cap*titleScale, titleScale, 0, GOLD)
entries = (["RESUME","START NEW GAME","MAP","STATS","BRIEF","OPTIONS","MODS","QUIT GAME"] if mode=="pause"
           else ["START GAME","OPTIONS","MODS","QUIT GAME"])
sel = int(sys.argv[6]) if len(sys.argv) > 6 else 2
arrow = Image.open(f"{ROOT}/pc/images/arrow.png").convert("RGBA").crop((0,2,62,95))
cap = bank.cap*itemScale
for i,e in enumerate(entries):
    cy = firstY + rowStep*i; w = bank.width(e, itemScale, track); left = cx - w/2
    text(cv, bank, e, left, cy + cap/2, itemScale, track, GOLD if i==sel else DIM)
    if i == sel:
        x1c = left-curGap; y0 = cy-curH/2+0.006*H
        cv.alpha_composite(arrow.resize((round(curW), round(curH)), Image.BILINEAR), (round(x1c-curW), round(y0)))
cv.convert("RGB").save(out); print("saved", out)
