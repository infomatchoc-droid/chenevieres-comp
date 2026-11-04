"""Aperçu : façade nettoyée + boutons, aiguilles, graduations dessinés + barre du haut."""
import json, math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

F = '/home/claude/work/fonts/RobotoCondensed-%s.ttf'
EQ = '/home/claude/chenevieres-eq/Resources/'


def font(sz, w='700'):
    return ImageFont.truetype(F % w, sz)


def interp_angle(cal, v, log=False):
    xs = [c[0] for c in cal]; ys = [c[1] for c in cal]
    f = (lambda t: math.log(t)) if log else (lambda t: t)
    v = min(max(v, min(xs)), max(xs))
    pts = sorted(zip(xs, ys))
    for (x0, a0), (x1, a1) in zip(pts, pts[1:]):
        if x0 <= v <= x1:
            t = 0 if x1 == x0 else (f(v) - f(x0)) / (f(x1) - f(x0))
            return a0 + t * (a1 - a0)
    return pts[-1][1]


def draw_pointer(base, cx, cy, ang, r_in, r_out, w_out, colour=(244, 244, 246), S=4):
    """Pointeur effilé (sur-échantillonné), avec ombre portée."""
    pad = int(r_out + 12)
    box = (int(cx - pad), int(cy - pad), int(cx + pad), int(cy + pad))
    W = 2 * pad
    lay = Image.new('RGBA', (W * S, W * S), (0, 0, 0, 0))
    sh = Image.new('RGBA', (W * S, W * S), (0, 0, 0, 0))
    a = math.radians(ang)
    ux, uy = math.sin(a), -math.cos(a)
    px, py = -uy, ux
    ccx, ccy = (cx - box[0]) * S, (cy - box[1]) * S

    def poly(off=(0, 0)):
        w_in = w_out * 0.45
        p = []
        for r, w in [(r_in, w_in), (r_out, w_out)]:
            p.append((ccx + (ux * r + px * w / 2 + off[0]) * S, ccy + (uy * r + py * w / 2 + off[1]) * S))
        for r, w in [(r_out, w_out), (r_in, w_in)]:
            p.append((ccx + (ux * r - px * w / 2 + off[0]) * S, ccy + (uy * r - py * w / 2 + off[1]) * S))
        return p
    ImageDraw.Draw(sh).polygon(poly((1.5, 2.5)), fill=(0, 0, 0, 150))
    sh = sh.filter(ImageFilter.GaussianBlur(2 * S))
    d = ImageDraw.Draw(lay)
    d.polygon(poly(), fill=colour + (255,), outline=(120, 120, 125, 255))
    lay = Image.alpha_composite(sh, lay).resize((W, W), Image.LANCZOS)
    base.alpha_composite(lay, (box[0], box[1]))


def draw_needle(base, piv, ang, length, face, colour=(20, 18, 16), S=4):
    x0, y0, x1, y1 = face
    W, H = x1 - x0, y1 - y0
    lay = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))
    sh = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))
    a = math.radians(ang)
    ux, uy = math.sin(a), -math.cos(a)
    def seg(off):
        return [((piv[0] - x0 + off[0]) * S, (piv[1] - y0 + off[1]) * S),
                ((piv[0] + ux * length - x0 + off[0]) * S, (piv[1] + uy * length - y0 + off[1]) * S)]
    ImageDraw.Draw(sh).line(seg((4, 3)), fill=(60, 30, 0, 90), width=3 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(2.5 * S))
    ImageDraw.Draw(lay).line(seg((0, 0)), fill=colour + (255,), width=int(3.2 * S))
    lay = Image.alpha_composite(sh, lay).resize((W, H), Image.LANCZOS)
    base.alpha_composite(lay, (x0, y0))


def text_c(d, xy, s, f, fill, anchor='mm'):
    d.text(xy, s, font=f, fill=fill, anchor=anchor)


def top_bar(width, h, model_name, model_sub, preset):
    bar = Image.new('RGBA', (width, h), (0, 0, 0, 255))
    arr = np.zeros((h, width, 4), np.uint8)
    t = np.linspace(0, 1, h)[:, None]
    top = np.array([30, 27, 24]); bot = np.array([13, 12, 11])
    arr[..., :3] = (top * (1 - t) + bot * t)[:, None, :].repeat(width, 1).reshape(h, width, 3)
    arr[..., 3] = 255
    bar = Image.fromarray(arr, 'RGBA')
    d = ImageDraw.Draw(bar)
    gold = (232, 163, 58)
    d.line([(0, h - 2), (width, h - 2)], fill=(200, 137, 47, 160), width=2)
    cy = h // 2

    def box(x0, x1, y0=cy - 34, y1=cy + 34, fill=(22, 20, 18), outline=(78, 70, 60), r=10, wdt=2):
        d.rounded_rectangle([x0, y0, x1, y1], r, fill=fill, outline=outline, width=wdt)

    def arrow(x, left):
        box(x, x + 54)
        s = 11
        if left:
            d.polygon([(x + 33, cy - s), (x + 33, cy + s), (x + 19, cy)], fill=gold)
        else:
            d.polygon([(x + 21, cy - s), (x + 21, cy + s), (x + 35, cy)], fill=gold)

    x = 28
    text_c(d, (x, cy - 44), 'MODÈLE', font(19, '600'), (150, 140, 128), 'lm')
    arrow(x, True)
    box(x + 66, x + 530, outline=(200, 137, 47), wdt=2)
    d.text((x + 92, cy - 4), model_name, font=font(40, '700'), fill=(245, 236, 222), anchor='lm')
    nw = d.textlength(model_name, font=font(40, '700'))
    d.text((x + 92 + nw + 18, cy - 1), model_sub, font=font(22, '500'), fill=(170, 160, 146), anchor='lm')
    d.polygon([(x + 492, cy - 6), (x + 510, cy - 6), (x + 501, cy + 6)], fill=gold)
    arrow(x + 542, False)

    x = 690
    text_c(d, (x, cy - 44), 'PRESET', font(19, '600'), (150, 140, 128), 'lm')
    arrow(x, True)
    box(x + 66, x + 420)
    d.text((x + 88, cy), preset, font=font(30, '500'), fill=(235, 228, 216), anchor='lm')
    d.polygon([(x + 382, cy - 6), (x + 400, cy - 6), (x + 391, cy + 6)], fill=gold)
    arrow(x + 432, False)

    x = 1220
    text_c(d, (x, cy - 44), 'COMPARAISON', font(19, '600'), (150, 140, 128), 'lm')
    box(x, x + 62, fill=(70, 50, 24), outline=gold)
    text_c(d, (x + 31, cy), 'A', font(32), (255, 214, 140))
    box(x + 74, x + 136)
    text_c(d, (x + 105, cy), 'B', font(32), (200, 190, 175))
    box(x + 148, x + 330)
    f26 = font(26, '500'); col = (200, 190, 175)
    w1 = d.textlength('COPIER A', font=f26); w2 = d.textlength('B', font=f26)
    tot = w1 + 34 + w2; sx = x + 239 - tot / 2
    d.text((sx, cy), 'COPIER A', font=f26, fill=col, anchor='lm')
    ax = sx + w1 + 7
    d.line([(ax, cy), (ax + 18, cy)], fill=col, width=3)
    d.polygon([(ax + 14, cy - 6), (ax + 21, cy), (ax + 14, cy + 6)], fill=col)
    d.text((ax + 27, cy), 'B', font=f26, fill=col, anchor='lm')

    x = 1585
    box(x, x + 170)
    d.ellipse([x + 20, cy - 8, x + 36, cy + 8], fill=(60, 52, 44), outline=(30, 28, 24))
    text_c(d, (x + 100, cy), 'BYPASS', font(28, '600'), (190, 180, 165))
    # réglages
    x = 1780
    box(x, x + 70)
    gx, gy = x + 35, cy
    for k in range(8):
        a = math.radians(k * 45)
        d.line([(gx + math.cos(a) * 12, gy + math.sin(a) * 12), (gx + math.cos(a) * 21, gy + math.sin(a) * 21)], fill=(205, 195, 180), width=8)
    d.ellipse([gx - 16, gy - 16, gx + 16, gy + 16], fill=(205, 195, 180))
    d.ellipse([gx - 7, gy - 7, gx + 7, gy + 7], fill=(22, 20, 18))
    # Pistache
    beagle = Image.open(EQ + 'logo_beagle.png').convert('RGB')
    bh = h - 2
    bw = int(beagle.width * bh / beagle.height * 1.12)
    beagle = beagle.resize((bw, bh), Image.LANCZOS)
    m = np.ones((bh, bw), np.float32)
    ramp = np.clip(np.arange(bw) / (bw * 0.22), 0, 1)
    m *= ramp[None, :]
    bar.paste(beagle, (width - bw - 4, 0), Image.fromarray((m * 255).astype(np.uint8)))
    return bar


def compose(model, state, out):
    cfg = json.load(open(f'layouts/{model}.json'))
    panel = Image.open(f'out/{model}/panel.png').convert('RGBA')
    d = ImageDraw.Draw(panel)
    # textes ajoutés (graduations refaites, titres renommés)
    for t in cfg.get('texts', []):
        d.text((t['x'], t['y']), t['s'], font=font(t['size'], t.get('w', '700')),
               fill=tuple(t.get('fill', [232, 232, 234])), anchor=t.get('anchor', 'mm'))
    for m in cfg.get('meters', []):
        gr = state.get(m['id'], 0)
        ang = interp_angle(m['scale'], gr)
        draw_needle(panel, m['pivot'], ang, m['len'], m['face'])
    for k in cfg['knobs']:
        spr = Image.open(f"out/{model}/knob_{k['id']}.png").convert('RGBA')
        # sprite centré sur (cx, cy)
        R = (spr.width - 1) / 2
        panel.alpha_composite(spr, (int(round(k['cx'])) - int(R), int(round(k['cy'])) - int(R)))
        v = state[k['id']]
        ang = interp_angle(k['cal'], v, k.get('log', False))
        draw_pointer(panel, k['cx'], k['cy'], ang, k['rs'] * 0.04, k.get('ptr_len', k['rs'] * 0.98),
                     k.get('ptr_w', k['rs'] * 0.085))
    bar = top_bar(panel.width, cfg.get('bar_h', 132), cfg['name'], cfg['sub'], state.get('preset', "Réglage d'usine"))
    full = Image.new('RGBA', (panel.width, panel.height + bar.height), (0, 0, 0, 255))
    full.alpha_composite(bar, (0, 0))
    full.alpha_composite(panel, (0, bar.height))
    full.convert('RGB').save(out)


if __name__ == '__main__':
    model, out = sys.argv[1], sys.argv[2]
    state = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
    compose(model, state, out)
