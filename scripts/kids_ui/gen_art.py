"""Generate the "starry night" artwork of the AI Passport kids UI.

Everything is drawn by code (original artwork, no external assets) with Pillow
at 4x supersampling, scaled down, dithered to RGB565 and written as C arrays to
main/boards/folotoy/ai-passport/kids_art.c and kids_art.h.

The stage is one opaque image: the night sky and the mascot in her neutral
pose. Each other expression or effect frame is stored only as the rectangle in
which its full render differs from the stage, so a patch drawn over the stage
is pixel-identical to the stage around its edges.

    python scripts/kids_ui/gen_art.py                # rewrite kids_art.c/.h
    python scripts/kids_ui/gen_art.py --preview DIR  # also write PNG previews
"""
import argparse
import math
import random
from pathlib import Path

import numpy as np
import PIL
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "main" / "boards" / "folotoy" / "ai-passport"

W = 240
STAGE_H = 204  # the stage image covers y < STAGE_H; below it the screen is BG_BOT
S = 4  # supersampling factor


def P(v):
    return int(round(v * S))


# ---------------------------------------------------------------- palette
BG_TOP = (30, 22, 78)
BG_MID = (52, 32, 112)
BG_BOT = (24, 16, 56)  # exact in RGB565, shared with the firmware
GLOW = (150, 110, 255)
OUTLINE = (58, 30, 84)
HAIR = (255, 150, 196)
HAIR_BACK = (226, 112, 168)
HAIR_LIGHT = (255, 208, 230)
EAR_INNER = (255, 222, 236)
SKIN = (255, 236, 224)
SKIN_SHADE = (250, 212, 204)
BLUSH = (255, 132, 160)
LASH = (48, 26, 82)
IRIS_TOP = (74, 46, 176)
IRIS_BOT = (182, 132, 255)
PUPIL = (34, 18, 80)
MOUTH = (176, 66, 104)
MOUTH_IN = (146, 44, 78)
TONGUE = (255, 142, 166)
BODY = (70, 58, 168)
BODY_LIGHT = (92, 80, 196)
COLLAR = (248, 248, 255)
COLLAR_STRIPE = (120, 150, 255)
RIBBON = (255, 108, 160)
STAR = (255, 222, 104)
STAR_EDGE = (214, 160, 48)
MOON = (255, 240, 186)
MINT = (120, 255, 214)
ZZZ = (196, 208, 255)
NOTE = (255, 150, 196)
THINK = (214, 204, 255)
HEART = (255, 104, 150)
HEART_EDGE = (214, 64, 112)

EYE_L = (97, 128)
EYE_R = (143, 128)
MOUTH_C = (120, 151)

EYES = ["open", "wide", "look_up", "happy", "closed", "sad", "heart", "wink"]
MOUTHS = ["cat", "smile", "open_s", "open_m", "open_l", "sad", "o"]
FX = ["none", "listen", "zzz", "notes", "think"]
FX_FRAMES = 3


# ---------------------------------------------------------------- drawing helpers
def ell(d, cx, cy, rx, ry, **kw):
    d.ellipse([P(cx - rx), P(cy - ry), P(cx + rx), P(cy + ry)], **kw)


def pts_arc(cx, cy, rx, ry, a0, a1, n=40):
    out = []
    for i in range(n + 1):
        t = math.radians(a0 + (a1 - a0) * i / n)
        out.append((cx + rx * math.cos(t), cy + ry * math.sin(t)))
    return out


def stroke(d, pts, fill, w, closed=False):
    """Polyline with round joins and caps."""
    sp = [(P(x), P(y)) for x, y in pts]
    if closed:
        sp = sp + [sp[0], sp[1]]
    d.line(sp, fill=fill, width=P(w), joint="curve")
    if not closed:
        r = w / 2
        for x, y in (pts[0], pts[-1]):
            ell(d, x, y, r, r, fill=fill)


def arc(d, cx, cy, rx, ry, a0, a1, fill, w):
    stroke(d, pts_arc(cx, cy, rx, ry, a0, a1), fill, w)


def poly(d, pts, fill, outline=None, ow=1.4):
    d.polygon([(P(x), P(y)) for x, y in pts], fill=fill)
    if outline:
        stroke(d, pts, outline, ow, closed=True)


def quad(p0, p1, p2, n=10):
    out = []
    for i in range(1, n + 1):
        t = i / n
        a, b, c = (1 - t) ** 2, 2 * (1 - t) * t, t * t
        out.append((a * p0[0] + b * p1[0] + c * p2[0], a * p0[1] + b * p1[1] + c * p2[1]))
    return out


def layer(img):
    lay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    return lay, ImageDraw.Draw(lay, "RGBA")


def star_pts(cx, cy, r_out, r_in, n=5, rot=-90):
    out = []
    for i in range(n * 2):
        r = r_out if i % 2 == 0 else r_in
        t = math.radians(rot + i * 180 / n)
        out.append((cx + r * math.cos(t), cy + r * math.sin(t)))
    return out


def heart_pts(cx, cy, s):
    out = []
    for i in range(60):
        t = 2 * math.pi * i / 60
        x = 16 * math.sin(t) ** 3
        y = -(13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))
        out.append((cx + x * s / 16, cy + y * s / 16))
    return out


def masked_paste(img, src, mask_draw):
    """Paste src over img through a mask drawn by mask_draw(ImageDraw)."""
    mask = Image.new("L", img.size, 0)
    mask_draw(ImageDraw.Draw(mask))
    img.paste(src, (0, 0), mask)


# ---------------------------------------------------------------- background
def make_background():
    img = Image.new("RGBA", (W * S, STAGE_H * S))
    d = ImageDraw.Draw(img)
    rows = STAGE_H * S
    flat_from = (STAGE_H - 10) * S  # the last rows are exactly BG_BOT
    for y in range(rows):
        t = min(1.0, y / (flat_from - 1))
        if t < 0.5:
            a, b, k = BG_TOP, BG_MID, t / 0.5
        else:
            a, b, k = BG_MID, BG_BOT, (t - 0.5) / 0.5
        col = tuple(int(round(a[i] + (b[i] - a[i]) * k)) for i in range(3))
        d.line([(0, y), (W * S, y)], fill=col + (255,))

    lay, ld = layer(img)
    ell(ld, 120, 108, 86, 64, fill=GLOW + (120,))
    lay = lay.filter(ImageFilter.GaussianBlur(P(20)))
    fade = Image.linear_gradient("L").resize((W * S, rows))
    fade = fade.point(lambda v: 255 if v < 190 else max(0, 255 - (v - 190) * 5))
    lay.putalpha(Image.composite(lay.getchannel("A"), Image.new("L", lay.size, 0), fade))
    img.alpha_composite(lay)

    # crescent moon with a soft halo
    lay, ld = layer(img)
    ell(ld, 30, 118, 15, 15, fill=MOON + (60,))
    lay = lay.filter(ImageFilter.GaussianBlur(P(4)))
    img.alpha_composite(lay)
    lay, ld = layer(img)
    ell(ld, 30, 118, 9.5, 9.5, fill=MOON + (255,))
    ell(ld, 35, 114, 8.5, 8.5, fill=(0, 0, 0, 0))
    img.alpha_composite(lay)

    rnd = random.Random(7)
    lay, ld = layer(img)
    placed = 0
    while placed < 44:
        x = rnd.uniform(8, 232)
        y = rnd.uniform(40, STAGE_H - 16)
        if math.hypot(x - 120, (y - 120) * 1.05) < 84:
            continue
        if math.hypot(x - 30, y - 118) < 18:
            continue
        r = rnd.choice([0.6, 0.7, 0.8, 1.0, 1.2])
        a = rnd.randint(110, 240)
        col = rnd.choice([(255, 255, 255), (255, 240, 200), (220, 210, 255)])
        ell(ld, x, y, r, r, fill=col + (a,))
        placed += 1
    for x, y, r, col in [(36, 60, 5.5, (255, 236, 170)), (206, 50, 4.5, (255, 200, 228)),
                         (18, 160, 4.0, (200, 220, 255)), (216, 140, 5.0, (255, 236, 170)),
                         (200, 182, 3.5, (255, 255, 255)), (44, 186, 3.0, (255, 214, 236))]:
        poly(ld, star_pts(x, y, r, r * 0.28, n=4), col + (235,))
    img.alpha_composite(lay)
    return img


# ---------------------------------------------------------------- mascot
def grad_ellipse(img, cx, cy, rx, ry, top, bot):
    lay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(lay)
    y0, y1 = P(cy - ry), P(cy + ry)
    for y in range(y0, y1 + 1):
        k = (y - y0) / max(1, (y1 - y0))
        col = tuple(int(top[i] + (bot[i] - top[i]) * k) for i in range(3))
        ld.line([(P(cx - rx), y), (P(cx + rx), y)], fill=col + (255,))
    masked_paste(img, lay, lambda m: ell(m, cx, cy, rx, ry, fill=255))


def draw_open_eye(img, cx, cy, side, scale=1.0, look=(0, 0), sparkle=False, teary=False):
    d = ImageDraw.Draw(img, "RGBA")
    rx, ry = 11.5 * scale, 14.0 * scale
    ell(d, cx, cy, rx, ry, fill=(255, 255, 255))
    ix, iy = cx + look[0], cy + 1 + look[1]
    grad_ellipse(img, ix, iy, 9.6 * scale, 12.4 * scale, IRIS_TOP, IRIS_BOT)
    d = ImageDraw.Draw(img, "RGBA")
    ell(d, ix, iy - 0.5, 4.6 * scale, 6.6 * scale, fill=PUPIL)
    lay, ld = layer(img)
    ell(ld, ix, iy - 9 * scale, 11 * scale, 8 * scale, fill=(26, 12, 66, 105))
    masked_paste(img, Image.alpha_composite(img, lay),
                 lambda m: ell(m, ix, iy, 9.6 * scale, 12.4 * scale, fill=255))
    d = ImageDraw.Draw(img, "RGBA")
    arc(d, ix, iy + 1, 7.2 * scale, 9.6 * scale, 35, 145, (236, 200, 255, 150), 1.6)
    if sparkle:
        poly(d, star_pts(ix - 3.6, iy - 6.4, 5.2 * scale, 1.7 * scale, n=4), (255, 255, 255))
        ell(d, ix + 4.2, iy + 5.6, 2.2, 2.2, fill=(255, 255, 255))
        ell(d, ix - 5.0, iy + 4.0, 1.1, 1.1, fill=(255, 255, 255, 220))
    else:
        ell(d, ix - 3.6, iy - 6.2, 3.7 * scale, 3.7 * scale, fill=(255, 255, 255))
        r2 = (2.9 if teary else 1.9) * scale
        ell(d, ix + 4.2, iy + 5.6, r2, r2, fill=(255, 255, 255, 240))
    # upper lash with a small flick at the outer corner
    arc(d, cx, cy + 0.6, rx + 0.8, ry + 0.8, 196, 344, LASH, 3.2)
    stroke(d, [(cx + side * (rx + 0.4), cy - 3.6), (cx + side * (rx + 4.2), cy - 6.6)], LASH, 2.2)
    arc(d, cx, cy, rx, ry, 48, 132, (150, 104, 150, 200), 1.1)


def draw_closed_eye(d, cx, cy, side):
    arc(d, cx, cy - 1.5, 11.0, 7.5, 18, 162, LASH, 3.2)
    stroke(d, [(cx + side * 10.6, cy + 2.6), (cx + side * 13.8, cy + 0.4)], LASH, 2.0)


def draw_happy_eye(d, cx, cy):
    arc(d, cx, cy + 6.5, 11.0, 9.5, 200, 340, LASH, 3.4)


def draw_eye(img, cx, cy, kind, side):
    d = ImageDraw.Draw(img, "RGBA")
    if kind == "open":
        draw_open_eye(img, cx, cy, side)
    elif kind == "wide":
        draw_open_eye(img, cx, cy - 0.5, side, scale=1.1, sparkle=True)
    elif kind == "look_up":
        draw_open_eye(img, cx, cy, side, look=(2.2, -2.6))
    elif kind == "sad":
        draw_open_eye(img, cx, cy + 1, side, scale=0.96, look=(0, 1.2), teary=True)
        d = ImageDraw.Draw(img, "RGBA")
        stroke(d, [(cx + side * 10, cy - 18.5), (cx - side * 7, cy - 23.5)], LASH, 2.4)
    elif kind == "closed":
        draw_closed_eye(d, cx, cy, side)
    elif kind == "happy":
        draw_happy_eye(d, cx, cy)
    elif kind == "heart":
        poly(d, heart_pts(cx, cy + 0.5, 12.5), HEART, outline=HEART_EDGE, ow=1.4)
        ell(d, cx - 4.6, cy - 5.0, 2.6, 2.0, fill=(255, 222, 232))
    else:
        raise ValueError(kind)


def draw_eyes(img, kind):
    if kind == "wink":
        draw_eye(img, EYE_L[0], EYE_L[1], "open", -1)
        draw_happy_eye(ImageDraw.Draw(img, "RGBA"), EYE_R[0], EYE_R[1])
        return
    draw_eye(img, EYE_L[0], EYE_L[1], kind, -1)
    draw_eye(img, EYE_R[0], EYE_R[1], kind, 1)


def draw_mouth(img, kind):
    d = ImageDraw.Draw(img, "RGBA")
    cx, cy = MOUTH_C
    if kind == "cat":
        arc(d, cx - 4.6, cy - 1.5, 4.6, 4.2, 0, 180, MOUTH, 2.1)
        arc(d, cx + 4.6, cy - 1.5, 4.6, 4.2, 0, 180, MOUTH, 2.1)
    elif kind == "smile":
        arc(d, cx, cy - 3.0, 8.5, 6.6, 18, 162, MOUTH, 2.3)
    elif kind == "open_s":
        ell(d, cx, cy + 0.5, 4.6, 3.6, fill=MOUTH_IN, outline=MOUTH, width=P(1.3))
    elif kind in ("open_m", "open_l"):
        rx, ry = (7.6, 7.6) if kind == "open_m" else (9.6, 10.4)
        top = cy - 4.0
        box = [P(cx - rx), P(top - ry), P(cx + rx), P(top + ry)]
        d.pieslice(box, 0, 180, fill=MOUTH_IN)
        lay, ld = layer(img)
        ell(ld, cx, top + ry * 0.92, rx * 0.62, ry * 0.5, fill=TONGUE + (255,))
        masked_paste(img, Image.alpha_composite(img, lay), lambda m: m.pieslice(box, 0, 180, fill=255))
        d = ImageDraw.Draw(img, "RGBA")
        stroke(d, [(cx - rx, top), (cx + rx, top)], MOUTH, 1.6)
        stroke(d, pts_arc(cx, top, rx, ry, 0, 180), MOUTH, 1.6)
    elif kind == "o":
        ell(d, cx, cy + 0.5, 3.8, 4.8, fill=MOUTH_IN, outline=MOUTH, width=P(1.6))
    elif kind == "sad":
        arc(d, cx, cy + 4.5, 7.0, 5.2, 205, 335, MOUTH, 2.3)
    else:
        raise ValueError(kind)


def bangs_outline():
    top = pts_arc(120, 116, 61, 57, 176, 364, n=60)
    right = quad((181, 124), (182, 146), (171, 160))  # outer edge of the right lock
    right += quad((171, 160), (165, 146), (163, 118))  # inner edge
    lower = []
    knots = [(163, 118), (158, 99), (146, 110), (134, 96), (121, 111), (108, 96), (95, 110),
             (82, 99), (77, 118)]
    for i in range(len(knots) - 1):
        p0, p2 = knots[i], knots[i + 1]
        going_down = p2[1] > p0[1]
        ctrl = ((p0[0] + p2[0]) / 2 + (3.5 if going_down else -1.5),
                (p0[1] + p2[1]) / 2 + (3.0 if going_down else -4.0))
        lower += quad(p0, ctrl, p2, n=8)
    left = quad((77, 118), (75, 146), (69, 160))
    left += quad((69, 160), (58, 146), (59, 124))
    return top + right + lower + left


def draw_body(img):
    """Everything of the mascot that lies under the eyes and the mouth."""
    d = ImageDraw.Draw(img, "RGBA")
    ell(d, 120, 120, 67, 61, fill=HAIR_BACK, outline=OUTLINE, width=P(1.5))
    for sgn in (-1, 1):
        ear = [(120 + sgn * 53, 88), (120 + sgn * 58, 41), (120 + sgn * 17, 63)]
        poly(d, ear, HAIR, outline=OUTLINE, ow=1.5)
        inner = [(120 + sgn * 47, 80), (120 + sgn * 53.5, 51), (120 + sgn * 27, 66)]
        poly(d, inner, EAR_INNER)
    poly(d, [(80, 210), (85, 185), (100, 173), (140, 173), (155, 185), (160, 210)], BODY,
         outline=OUTLINE, ow=1.5)
    stroke(d, [(92, 184), (100, 177)], BODY_LIGHT, 2.0)
    stroke(d, [(148, 184), (140, 177)], BODY_LIGHT, 2.0)
    d.rectangle([P(111), P(162), P(129), P(178)], fill=SKIN_SHADE)
    for sgn in (-1, 1):
        collar = [(120 - sgn * 23, 173), (120 - sgn * 8, 171), (120, 192), (120 - sgn * 16, 190)]
        poly(d, collar, COLLAR, outline=OUTLINE, ow=1.2)
        stroke(d, [(120 - sgn * 20.5, 175.5), (120 - sgn * 14, 187.5)], COLLAR_STRIPE, 1.3)
    poly(d, [(120, 193), (108, 187), (109, 200)], RIBBON, outline=OUTLINE, ow=1.1)
    poly(d, [(120, 193), (132, 187), (131, 200)], RIBBON, outline=OUTLINE, ow=1.1)
    ell(d, 120, 193.5, 3.2, 3.2, fill=(255, 150, 190), outline=OUTLINE, width=P(1.0))
    ell(d, 120, 126, 52, 45, fill=SKIN, outline=OUTLINE, width=P(1.4))
    lay, ld = layer(img)
    ell(ld, 120, 96, 54, 22, fill=SKIN_SHADE + (150,))
    masked_paste(img, Image.alpha_composite(img, lay), lambda m: ell(m, 120, 126, 50.6, 43.6, fill=255))
    lay, ld = layer(img)
    for bx in (80, 160):
        ell(ld, bx, 146, 9.5, 5.6, fill=BLUSH + (120,))
    lay = lay.filter(ImageFilter.GaussianBlur(P(1.6)))
    img.alpha_composite(lay)
    d = ImageDraw.Draw(img, "RGBA")
    for bx in (80, 160):
        for k in (-3.5, 0, 3.5):
            stroke(d, [(bx + k - 1.2, 148), (bx + k + 1.2, 144)], (255, 110, 140, 200), 0.9)


def make_hair_layer(size):
    """Bangs and the star clip: drawn over the eyes."""
    lay = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(lay, "RGBA")
    poly(d, bangs_outline(), HAIR, outline=OUTLINE, ow=1.5)
    for a0, a1 in ((212, 246), (252, 300), (306, 326)):
        arc(d, 120, 114, 45, 35, a0, a1, HAIR_LIGHT, 4.2)
    poly(d, star_pts(160, 92, 9.0, 4.0), STAR, outline=STAR_EDGE, ow=1.2)
    ell(d, 158, 89.5, 1.6, 1.6, fill=(255, 250, 220))
    return lay


# ---------------------------------------------------------------- effects
def zee(d, x, y, s, col):
    stroke(d, [(x - s, y - s), (x + s, y - s), (x - s, y + s), (x + s, y + s)], col, 2.2)


def note(d, x, y, double=False):
    ell(d, x, y + 8, 4.2, 3.4, fill=NOTE)
    if double:
        ell(d, x + 10, y + 5, 4.2, 3.4, fill=NOTE)
        stroke(d, [(x + 3.6, y + 8), (x + 3.6, y - 6), (x + 13.6, y - 9), (x + 13.6, y + 5)], NOTE, 2.2)
    else:
        stroke(d, [(x + 3.6, y + 8), (x + 3.6, y - 6), (x + 10, y - 3)], NOTE, 2.2)


def draw_fx(img, fx, frame):
    d = ImageDraw.Draw(img, "RGBA")
    if fx == "listen":
        for i in range(frame + 1):
            r = 11 + i * 8
            arc(d, 58, 60, r, r, 150, 250, MINT, 3.0)
            arc(d, 182, 60, r, r, 290, 390, MINT, 3.0)
    elif fx == "zzz":
        for i, (x, y, s) in enumerate([(186, 80, 4.0), (198, 64, 5.5), (211, 46, 7.0)]):
            if i <= frame:
                zee(d, x, y, s, ZZZ)
    elif fx == "notes":
        if frame == 0:
            note(d, 190, 82)
        elif frame == 1:
            note(d, 192, 76)
            note(d, 202, 50, double=True)
        else:
            note(d, 188, 70, double=True)
            note(d, 210, 52)
    elif fx == "think":
        for i, (x, y, r) in enumerate([(184, 84, 2.6), (194, 70, 3.6), (206, 52, 5.2)]):
            if i <= frame:
                ell(d, x, y, r, r, fill=THINK)
    elif fx != "none":
        raise ValueError(fx)


# ---------------------------------------------------------------- render + RGB565
BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])


class Renderer:
    def __init__(self):
        self.base = make_background()
        draw_body(self.base)
        self.hair = make_hair_layer(self.base.size)

    def render(self, eyes="open", mouth="cat", fx="none", frame=0):
        img = self.base.copy()
        draw_eyes(img, eyes)
        draw_mouth(img, mouth)
        img.alpha_composite(self.hair)
        draw_fx(img, fx, frame)
        return img.resize((W, STAGE_H), Image.LANCZOS).convert("RGB")


def to_rgb565(img):
    """Ordered dithering with absolute coordinates, so equal pixels stay equal."""
    a = np.asarray(img, dtype=np.float64)
    h, w, _ = a.shape
    t = (BAYER4[np.arange(h)[:, None] % 4, np.arange(w)[None, :] % 4] + 0.5) / 16.0
    r = np.minimum(31, np.floor(a[:, :, 0] / 8.0 + t)).astype(np.uint16)
    g = np.minimum(63, np.floor(a[:, :, 1] / 4.0 + t)).astype(np.uint16)
    b = np.minimum(31, np.floor(a[:, :, 2] / 8.0 + t)).astype(np.uint16)
    return (r << 11) | (g << 5) | b


def rgb565_to_image(px):
    r = ((px >> 11) & 0x1F).astype(np.uint8)
    g = ((px >> 5) & 0x3F).astype(np.uint8)
    b = (px & 0x1F).astype(np.uint8)
    rgb = np.dstack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)])
    return Image.fromarray(rgb, "RGB")


def bbox(mask):
    ys, xs = np.nonzero(mask)
    if len(xs) == 0:
        return None
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


def union(boxes):
    boxes = [b for b in boxes if b]
    return (min(b[0] for b in boxes), min(b[1] for b in boxes),
            max(b[2] for b in boxes), max(b[3] for b in boxes))


def inside(mask, box):
    x0, y0, x1, y1 = box
    m = mask.copy()
    m[y0:y1, x0:x1] = False
    return not m.any()


def build():
    """Render everything; return (stage pixels, layers).

    Each layer is (name, rect, {variant: pixels or None}) in drawing order;
    rect is (x0, y0, x1, y1) and None means "same as the stage".
    """
    rnd = Renderer()
    neutral = to_rgb565(rnd.render())
    diff = {}
    for e in EYES[1:]:
        diff[("eyes", e)] = to_rgb565(rnd.render(eyes=e))
    for m in MOUTHS[1:]:
        diff[("mouth", m)] = to_rgb565(rnd.render(mouth=m))
    for fx in FX[1:]:
        for f in range(FX_FRAMES):
            diff[("fx", fx, f)] = to_rgb565(rnd.render(fx=fx, frame=f))
    masks = {k: v != neutral for k, v in diff.items()}
    half = np.zeros_like(neutral, dtype=bool)
    half[:, : W // 2] = True

    def masks_of(kind, side=None):
        out = {}
        for k, m in masks.items():
            if k[0] != kind:
                continue
            if side == "left":
                m = m & half
            elif side == "right":
                m = m & ~half
            out[k] = m
        return out

    layer_masks = [
        ("eyes", masks_of("eyes")),
        ("mouth", masks_of("mouth")),
        ("fx_left", masks_of("fx", "left")),
        ("fx_right", masks_of("fx", "right")),
    ]
    rects = {name: union([bbox(m) for m in ms.values()]) for name, ms in layer_masks}

    # A patch hides whatever lies under it, so no variant of a lower layer may
    # change pixels inside the rectangle of a layer drawn above it.
    for i, (lower, ms) in enumerate(layer_masks):
        for upper, _ in layer_masks[i + 1:]:
            x0, y0, x1, y1 = rects[upper]
            for key, m in ms.items():
                if m[y0:y1, x0:x1].any():
                    raise SystemExit(f"{key} changes pixels under the {upper} patch")
    for key, m in masks.items():
        if key[0] == "fx" and key[1] != "listen" and (m & half).any():
            raise SystemExit(f"{key} must stay on the right side")

    layers = []
    for name, ms in layer_masks:
        x0, y0, x1, y1 = rects[name]
        variants = {}
        for key in ms:
            variants[key] = diff[key][y0:y1, x0:x1] if ms[key].any() else None
        layers.append((name, rects[name], variants))
    return neutral, layers


# ---------------------------------------------------------------- C output
def c_ident(key):
    return "kids_" + "_".join(str(k) for k in key)


def c_words(px):
    data = px.astype("<u2").tobytes()
    if len(data) % 4:
        data += b"\0\0"
    words = np.frombuffer(data, dtype="<u4")
    lines = []
    for i in range(0, len(words), 8):
        lines.append("    " + ", ".join(f"0x{int(w):08x}" for w in words[i:i + 8]) + ",")
    return lines


def c_image(name, px, storage="static "):
    h, w = px.shape
    out = [f"static const uint32_t {name}_map[] = {{"]
    out += c_words(px)
    out += ["};", f"{storage}const lv_image_dsc_t {name} = {{"]
    out += ["    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565, "
            f".w = {w}, .h = {h}, .stride = {w * 2}}},"]
    out += [f"    .data_size = {w * h * 2},", f"    .data = (const uint8_t*){name}_map,", "};", ""]
    return out


GENERATED = "// Generated by scripts/kids_ui/gen_art.py from code-drawn artwork. Do not edit."


def pillow_note():
    # Resampling may differ between Pillow releases; the test compares only
    # output made with the same version.
    return f"// Rendered with Pillow {PIL.__version__}."


def emit(neutral, layers):
    rects = {name: rect for name, rect, _ in layers}
    variants = {name: v for name, _, v in layers}
    h = [GENERATED, pillow_note(), "#pragma once", "", "#include <lvgl.h>", "",
         "#ifdef __cplusplus", 'extern "C" {', "#endif", ""]
    h += [f"#define KIDS_ART_STAGE_W {W}", f"#define KIDS_ART_STAGE_H {STAGE_H}",
          "#define KIDS_ART_BG_COLOR 0x{:02X}{:02X}{:02X}".format(*BG_BOT), ""]
    for name in ("eyes", "mouth", "fx_left", "fx_right"):
        x0, y0, x1, y1 = rects[name]
        up = name.upper()
        h += [f"#define KIDS_ART_{up}_X {x0}", f"#define KIDS_ART_{up}_Y {y0}",
              f"#define KIDS_ART_{up}_W {x1 - x0}", f"#define KIDS_ART_{up}_H {y1 - y0}"]
    h += [""]
    h += ["typedef enum {"] + [f"    KIDS_EYES_{e.upper()}," for e in EYES] + ["    KIDS_EYES_COUNT,",
                                                                             "} kids_eyes_t;", ""]
    h += ["typedef enum {"] + [f"    KIDS_MOUTH_{m.upper()}," for m in MOUTHS] + ["    KIDS_MOUTH_COUNT,",
                                                                               "} kids_mouth_t;", ""]
    h += ["typedef enum {"] + [f"    KIDS_FX_{f.upper()}," for f in FX] + ["    KIDS_FX_COUNT,",
                                                                        "} kids_fx_t;", ""]
    h += [f"#define KIDS_FX_FRAMES {FX_FRAMES}", "",
          "// NULL entries mean \"identical to the stage\": hide the patch.",
          "extern const lv_image_dsc_t kids_art_stage;",
          "extern const lv_image_dsc_t* const kids_art_eyes[KIDS_EYES_COUNT];",
          "extern const lv_image_dsc_t* const kids_art_mouth[KIDS_MOUTH_COUNT];",
          "extern const lv_image_dsc_t* const kids_art_fx_left[KIDS_FX_COUNT][KIDS_FX_FRAMES];",
          "extern const lv_image_dsc_t* const kids_art_fx_right[KIDS_FX_COUNT][KIDS_FX_FRAMES];",
          "", "#ifdef __cplusplus", "}", "#endif", ""]

    c = [GENERATED, pillow_note(), '#include "sdkconfig.h"', "", "#if CONFIG_AI_PASSPORT_KIDS_UI",
         "", '#include "kids_art.h"', ""]
    total = neutral.size * 2
    names = {}
    for name in ("eyes", "mouth", "fx_left", "fx_right"):
        for key, px in variants[name].items():
            if px is None:
                continue
            ident = c_ident((name,) + key[1:])
            names[(name,) + key[1:]] = ident
            c += c_image(ident, px)
            total += px.size * 2
    c += c_image("kids_art_stage", neutral, storage="")

    def ref(key):
        ident = names.get(key)
        return f"&{ident}" if ident else "NULL"

    c += ["const lv_image_dsc_t* const kids_art_eyes[KIDS_EYES_COUNT] = {"]
    c += [f"    {ref(('eyes', e))}," for e in EYES] + ["};", ""]
    c += ["const lv_image_dsc_t* const kids_art_mouth[KIDS_MOUTH_COUNT] = {"]
    c += [f"    {ref(('mouth', m))}," for m in MOUTHS] + ["};", ""]
    for side in ("fx_left", "fx_right"):
        c += [f"const lv_image_dsc_t* const kids_art_{side}[KIDS_FX_COUNT][KIDS_FX_FRAMES] = {{"]
        for fx in FX:
            refs = ", ".join(ref((side, fx, f)) for f in range(FX_FRAMES))
            c += [f"    {{{refs}}},"]
        c += ["};", ""]
    c += ["#endif  // CONFIG_AI_PASSPORT_KIDS_UI", ""]
    return "\n".join(h), "\n".join(c), total


def write_preview(out_dir, neutral, layers):
    out_dir.mkdir(parents=True, exist_ok=True)
    rgb565_to_image(neutral).save(out_dir / "stage.png")
    tiles = []
    for name, (x0, y0, x1, y1), variants in layers:
        for key, px in variants.items():
            full = neutral.copy()
            if px is not None:
                full[y0:y1, x0:x1] = px
            tile = rgb565_to_image(full)
            ImageDraw.Draw(tile).rectangle([x0 - 1, y0 - 1, x1, y1], outline=(255, 0, 0))
            tiles.append(tile)
    cols = 8
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (W + 6), rows * (STAGE_H + 6)), (10, 10, 14))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % cols) * (W + 6), (i // cols) * (STAGE_H + 6)))
    sheet.save(out_dir / "variants.png")
    zoom = rgb565_to_image(neutral).crop((30, 30, 210, STAGE_H)).resize((540, 522), Image.NEAREST)
    zoom.save(out_dir / "stage_zoom.png")


def generate():
    """Return (header text, source text, art bytes, stage pixels, layers)."""
    neutral, layers = build()
    h, c, total = emit(neutral, layers)
    return h, c, total, neutral, layers


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--preview", type=Path, help="also write PNG previews to this folder")
    args = parser.parse_args()
    h, c, total, neutral, layers = generate()
    (OUT_DIR / "kids_art.h").write_text(h, encoding="utf-8")
    (OUT_DIR / "kids_art.c").write_text(c, encoding="utf-8")
    for name, (x0, y0, x1, y1), variants in layers:
        print(f"{name:9s} rect x={x0} y={y0} w={x1 - x0} h={y1 - y0} variants={len(variants)}")
    print(f"art data: {total} bytes")
    if args.preview:
        write_preview(args.preview, neutral, layers)


if __name__ == "__main__":
    main()
