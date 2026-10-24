#!/usr/bin/env python3
"""Pre-render glyph atlases for PCMenu from the TTF files shipped in PCMenu.zip.

The mod draws text by blitting glyph rectangles out of these atlases, so the
game never has to rasterise a TTF at runtime.

usage: gen_fonts.py <PCMenu dir> [<Arabic TTF>]
output (inside <PCMenu dir>/pc/fonts/):
    bank.png   bank.txt      (BankGothic Bold  - menu items, ASCII 32..126)
    beck.png   beck.txt      (Beckett          - titles,     ASCII 32..126)
    bank_ar.png bank_ar.txt  (Arabic glyphs for the menu items)   - only when an Arabic TTF is given or found
    beck_ar.png beck_ar.txt  (Arabic glyphs for the titles)

The Arabic atlases hold the *presentation forms* (U+FE80..U+FEFC: isolated / final / initial / medial letters and the lam-alef
ligatures) plus the Arabic comma, semicolon and question mark. The mod picks the right form itself (see jni/arabic.h), so the
font only has to be a plain TrueType file with those code points: DejaVu Sans Bold (free licence, shipped with most Linux
distributions) is used by default. Glyphs are rendered so that the height of the letter alef matches the cap height of the
Latin atlas they sit next to (bank: 1.45 x, beck: 1.10 x since 1.1.8; they were 1.12 / 0.95 before).
"""
import sys, os, unicodedata
from PIL import Image, ImageDraw, ImageFont

SIZE = 64
PAD = 3
ATLAS_W = 512

def build(ttf, out_png, out_txt):
    font = ImageFont.truetype(ttf, SIZE)
    glyphs = []
    for code in range(32, 127):
        ch = chr(code)
        x0, y0, x1, y1 = font.getbbox(ch, anchor='ls')
        adv = font.getlength(ch)
        glyphs.append((code, ch, x0, y0, x1, y1, adv))
    # shelf packing
    x = y = row_h = 0
    placed = []
    for g in glyphs:
        code, ch, x0, y0, x1, y1, adv = g
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        cw, ch_h = w + PAD * 2, h + PAD * 2
        if x + cw > ATLAS_W:
            x, y, row_h = 0, y + row_h, 0
        placed.append((g, x, y, w, h))
        x += cw
        row_h = max(row_h, ch_h)
    atlas_h = 1
    while atlas_h < y + row_h:
        atlas_h *= 2
    img = Image.new('L', (ATLAS_W, atlas_h), 0)
    d = ImageDraw.Draw(img)
    lines = []
    for (code, ch, x0, y0, x1, y1, adv), px, py, w, h in placed:
        if w and h:
            d.text((px + PAD - x0, py + PAD - y0), ch, font=font, fill=255, anchor='ls')
        lines.append("G %d %d %d %d %d %d %d %.3f" % (code, px + PAD, py + PAD, w, h, x0, y0, adv))
    rgba = Image.new('RGBA', img.size, (255, 255, 255, 0))
    rgba.putalpha(img)
    rgba.save(out_png)
    cap = font.getbbox('H', anchor='ls')
    asc, desc = font.getmetrics()
    with open(out_txt, 'w') as f:
        f.write("FONT %d %d %d %d %d %d\n" % (SIZE, asc, desc, cap[3] - cap[1], ATLAS_W, atlas_h))
        f.write("\n".join(lines) + "\n")
    print(out_png, img.size, len(lines), 'glyphs, cap height', cap[3] - cap[1])



ARABIC_DEFAULT = ['/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', '/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf']
AR_CODES = [c for c in range(0xFE80, 0xFEFD)] + [0x060C, 0x061B, 0x061F]

def build_arabic(ttf, out_png, out_txt, alef_target):
    """Atlas of Arabic presentation forms, sized so that the alef is alef_target pixels tall."""
    probe = ImageFont.truetype(ttf, 200, layout_engine=ImageFont.Layout.BASIC)
    b = probe.getbbox('\uFE8D', anchor='ls')
    size = int(round(200.0 * alef_target / float(b[3] - b[1])))
    font = ImageFont.truetype(ttf, size, layout_engine=ImageFont.Layout.BASIC)
    cmap = set()
    from fontTools.ttLib import TTFont
    cmap = set(TTFont(ttf).getBestCmap().keys())
    glyphs = []
    for code in AR_CODES:
        if code not in cmap: continue
        ch = chr(code)
        x0, y0, x1, y1 = font.getbbox(ch, anchor='ls')
        glyphs.append((code, ch, x0, y0, x1, y1, font.getlength(ch)))
    aw = 1024
    x = y = row_h = 0
    placed = []
    for g in glyphs:
        code, ch, x0, y0, x1, y1, adv = g
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        cw, chh = w + PAD * 2, h + PAD * 2
        if x + cw > aw: x, y, row_h = 0, y + row_h, 0
        placed.append((g, x, y, w, h)); x += cw; row_h = max(row_h, chh)
    ah = 1
    while ah < y + row_h: ah *= 2
    img = Image.new('L', (aw, ah), 0)
    d = ImageDraw.Draw(img)
    lines = []
    for (code, ch, x0, y0, x1, y1, adv), px, py, w, h in placed:
        if w and h: d.text((px + PAD - x0, py + PAD - y0), ch, font=font, fill=255, anchor='ls')
        lines.append("G %d %d %d %d %d %d %d %.3f" % (code, px + PAD, py + PAD, w, h, x0, y0, adv))
    rgba = Image.new('RGBA', img.size, (255, 255, 255, 0)); rgba.putalpha(img); rgba.save(out_png)
    asc, desc = font.getmetrics()
    with open(out_txt, 'w') as f:
        f.write("FONT %d %d %d %d %d %d\n" % (size, asc, desc, alef_target, aw, ah))
        f.write("\n".join(lines) + "\n")
    print(out_png, img.size, len(lines), 'glyphs, em', size, 'alef', alef_target)

def read_cap(txt):
    return int(open(txt).readline().split()[4])

# alef height relative to the cap height of the Latin atlas it sits next to. 1.1.7 used 1.12 / 0.95: on the phone the Arabic text (and
# the BACK button) looked small and thin next to the wide, heavy PC letters, so 1.1.8 makes it bigger. (env override: PCM_AR_BANK / PCM_AR_BECK)
AR_BANK_FACTOR = float(os.environ.get('PCM_AR_BANK', '1.45'))
AR_BECK_FACTOR = float(os.environ.get('PCM_AR_BECK', '1.10'))

def main_arabic(root, ttf):
    fd = os.path.join(root, 'pc', 'fonts')
    build_arabic(ttf, os.path.join(fd, 'bank_ar.png'), os.path.join(fd, 'bank_ar.txt'), int(round(read_cap(os.path.join(fd, 'bank.txt')) * AR_BANK_FACTOR)))
    build_arabic(ttf, os.path.join(fd, 'beck_ar.png'), os.path.join(fd, 'beck_ar.txt'), int(round(read_cap(os.path.join(fd, 'beck.txt')) * AR_BECK_FACTOR)))

if __name__ == '__main__':
    root = sys.argv[1]
    fd = os.path.join(root, 'pc', 'fonts')
    if not os.path.exists(os.path.join(fd, 'bank.txt')) or os.environ.get('PCM_LATIN') == '1':
        build(os.path.join(fd, 'BankGothic', 'BankGothic Bold.ttf'), os.path.join(fd, 'bank.png'), os.path.join(fd, 'bank.txt'))
        build(os.path.join(fd, 'Beckett', 'Beckett.TTF'), os.path.join(fd, 'beck.png'), os.path.join(fd, 'beck.txt'))
    ar = sys.argv[2] if len(sys.argv) > 2 else next((p for p in ARABIC_DEFAULT if os.path.exists(p)), None)
    if ar: main_arabic(root, ar)
    else: print('no Arabic TTF found (pass one as 2nd argument): Arabic atlases not generated')
