#!/usr/bin/env python3
"""Cross-checks the mod's own Arabic shaping (jni/arabic.h) against a real text shaper.

The host test (hosttest/test.cpp, section R8) dumps every Arabic phrase of the tables together with the glyph codes the mod would
draw (visual order) into /tmp/pcm_ar_codes.txt.  Here each pure-Arabic phrase is drawn twice with the SAME font the atlases were
made from:
  * reference: Pillow's RAQM layout (HarfBuzz shaping + FriBiDi ordering) of the logical text, right-to-left
  * mod:       the atlas glyphs (bank_ar.png / bank_ar.txt) placed exactly as the mod places them
and the two pictures are compared (normalised cross-correlation of the ink, after cropping to the ink box).  A wrong letter
form, a missing ligature or a wrong order shows up as a clearly lower score.  A montage of all phrases is written next to it.

usage: check_arabic.py <PCMenu dir> [codes file] [montage.png]
"""
import sys, os
from PIL import Image, ImageDraw, ImageFont, features
import numpy as np

root = sys.argv[1]
codes_file = sys.argv[2] if len(sys.argv) > 2 else '/tmp/pcm_ar_codes.txt'
montage = sys.argv[3] if len(sys.argv) > 3 else '/tmp/pcm_ar_montage.png'
ttf = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
if not features.check('raqm'):
    print('Pillow has no RAQM support: cannot run the reference shaper'); sys.exit(2)

fd = os.path.join(root, 'pc', 'fonts')
atlas = Image.open(os.path.join(fd, 'bank_ar.png')).convert('RGBA').getchannel('A')
glyphs = {}
size = None
for ln in open(os.path.join(fd, 'bank_ar.txt')):
    p = ln.split()
    if p[0] == 'FONT': size = int(p[1])
    elif p[0] == 'G': glyphs[int(p[1])] = tuple(map(float, p[2:9]))
ref_font = ImageFont.truetype(ttf, size, layout_engine=ImageFont.Layout.RAQM)
space_adv = glyphs[0xFE8D][6]                      # the mod's space between Arabic words (alef width)

W, H, BASE = 1400, 140, 90
def mine(codes):
    im = Image.new('L', (W, H), 0); pen = 20.0
    for c in codes:
        if c == 32: pen += space_adv; continue
        x, y, w, h, xo, yo, adv = glyphs[c]
        if w and h:
            g = atlas.crop((int(x), int(y), int(x + w), int(y + h)))
            im.paste(g, (int(round(pen + xo)), int(round(BASE + yo))), g)
        pen += adv
    return im
def reference(text):
    im = Image.new('L', (W, H), 0); d = ImageDraw.Draw(im)
    d.text((20, BASE), text, font=ref_font, fill=255, anchor='ls', direction='rtl', language='ar')
    return im
def box(im):
    bb = im.getbbox(); return bb
def norm(im, bb, w=480, h=96):
    c = np.asarray(im.crop(bb).resize((w, h), Image.BILINEAR), dtype=np.float32) / 255.0
    return c
def ncc(a, b):
    a = a - a.mean(); b = b - b.mean(); d = np.sqrt((a * a).sum() * (b * b).sum())
    return float((a * b).sum() / d) if d else 0.0

rows = []; worst = 1.0; bad = []
for ln in open(codes_file, encoding='utf-8'):
    ln = ln.rstrip('\n')
    if '\t' not in ln: continue
    text, codes = ln.split('\t'); codes = [int(x) for x in codes.split()]
    if any(c < 128 and c != 32 for c in codes): continue          # mixed Latin / digits: covered by the unit checks (the PC font is not DejaVu)
    a, b = mine(codes), reference(text)
    ba, bb_ = box(a), box(b)
    if not ba or not bb_: bad.append((text, 'empty')); continue
    wa, wb = ba[2] - ba[0], bb_[2] - bb_[0]
    s = ncc(norm(a, ba), norm(b, bb_))
    rw = wa / float(wb)
    rows.append((text, s, rw, a, b)); worst = min(worst, s)
    if s < 0.90 or abs(rw - 1) > 0.06: bad.append((text, 'ncc %.3f width ratio %.3f' % (s, rw)))
print('%d phrases compared, worst correlation %.3f, mean %.3f' % (len(rows), worst, sum(r[1] for r in rows) / max(1, len(rows))))
for t, why in bad: print('  MISMATCH', t, why)

# montage: first 28 phrases, mine above the reference
n = min(len(rows), 28)
M = Image.new('L', (W, n * 2 * 70), 0)
for i in range(n):
    t, s, rw, a, b = rows[i]
    M.paste(a.crop((0, 40, W, 110)), (0, i * 140)); M.paste(b.crop((0, 40, W, 110)), (0, i * 140 + 70))
M.save(montage); print('montage', montage)
sys.exit(1 if bad else 0)
