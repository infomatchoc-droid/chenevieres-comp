"""Construit les ressources du plugin pour un modèle, à partir de layouts/<model>.json :
  Resources/panels/<model>_panel.jpg      façade de base (boutons éteints, sans pointeurs ni aiguilles)
  Resources/panels/<model>_k_<id>.png     capuchons de boutons rotatifs
  Resources/panels/<model>_b_<id>.png     boutons lumineux allumés (avec halo) ; éteints = façade de base
  Resources/panels/<model>_l_<id>.png     voyants allumés
  Source/layouts/<model>.json             position des commandes (lu par gen_layouts.py)
"""
import cv2, json, math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

import clean

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.join(HERE, '..', '..', 'Resources', 'fonts', 'RobotoCondensed-%s.ttf')


def font(sz, w='700'):
    return ImageFont.truetype(FONT % w, sz)


def erase_text(crop, lit):
    """Efface le texte d'un bouton (intérieur seulement)."""
    h, w = crop.shape[:2]
    g = cv2.cvtColor(crop, cv2.COLOR_BGR2GRAY).astype(np.float32)
    inner = np.zeros((h, w), np.uint8)
    m = max(5, int(min(w, h) * 0.14))
    inner[m:h - m, m:w - m] = 255
    med = cv2.medianBlur(crop, 2 * (min(w, h) // 3) + 1)
    gm = cv2.cvtColor(med, cv2.COLOR_BGR2GRAY).astype(np.float32)
    if lit:
        mask = (g < gm - 28)
    else:
        mask = (g > gm + 28)
    mask = cv2.dilate(mask.astype(np.uint8) * 255, np.ones((5, 5), np.uint8)) & inner
    return cv2.inpaint(crop, mask, 3, cv2.INPAINT_TELEA)


def draw_label(img_rgba, rect_in_img, label, lit, size):
    d = ImageDraw.Draw(img_rgba)
    x, y, w, h = rect_in_img
    lines = label.split('\n')
    f = font(size)
    col = (43, 24, 4, 255) if lit else (232, 232, 234, 255)
    lh = size * 1.0
    total = lh * len(lines)
    for i, ln in enumerate(lines):
        cy = y + h / 2 - total / 2 + lh * (i + 0.5) + 1
        d.text((x + w / 2, cy), ln, font=f, fill=col, anchor='mm')


def build(model, outdir_res, outdir_layout):
    cfg = json.load(open(f'layouts/{model}.json'))
    clean.run(model)
    src = cv2.imread(os.path.join(clean.ORIG, f'{model}.png'))
    base = cv2.imread(f'out/{model}/panel.png')
    H, W = base.shape[:2]

    # 1) textes refaits (graduations, titres renommés) : cuits dans la façade
    pil = Image.fromarray(cv2.cvtColor(base, cv2.COLOR_BGR2RGB)).convert('RGBA')
    d = ImageDraw.Draw(pil)
    for t in cfg.get('texts', []):
        f = ImageFont.truetype(os.path.join(HERE, 'fonts', t['font']), t['size']) if 'font' in t else font(t['size'], t.get('w', '700'))
        col = tuple(t.get('fill', [232, 232, 234]))
        if 'width' in t:
            # texte espacé pour tenir exactement dans la largeur voulue (x = bord gauche, y = milieu)
            chars = list(t['s'])
            ws = [d.textlength(ch, font=f) for ch in chars]
            track = (t['width'] - sum(ws)) / max(1, len(chars) - 1)
            x = t['x']
            for ch, w in zip(chars, ws):
                d.text((x, t['y']), ch, font=f, fill=col, anchor='lm')
                x += w + track
        else:
            d.text((t['x'], t['y']), t['s'], font=f, fill=col, anchor=t.get('anchor', 'mm'))
    # arcs d'échelle à refaire (là où passait l'ancienne aiguille), dessinés en sur-échantillonné
    for arc in cfg.get('arcs', []):
        S = 4
        lay = Image.new('RGBA', (pil.width * S, pil.height * S), (0, 0, 0, 0))
        r = arc['r'] * S
        box = [(arc['cx'] * S - r), (arc['cy'] * S - r), (arc['cx'] * S + r), (arc['cy'] * S + r)]
        # PIL : angles depuis l'axe x, sens horaire ; nos angles : depuis le haut
        ImageDraw.Draw(lay).arc(box, arc['a0'] - 90, arc['a1'] - 90, fill=tuple(arc['fill']) + (255,), width=int(arc['w'] * S))
        pil.alpha_composite(lay.resize(pil.size, Image.LANCZOS))
    base = cv2.cvtColor(np.array(pil.convert('RGB')), cv2.COLOR_RGB2BGR)

    controls = []
    # 2) boutons lumineux
    templates = {}
    for grp in cfg.get('buttons', []):
        rects = grp['rects']
        margin = grp.get('glow', 14)
        if 'template_group' in grp:
            lit_t, lit_halo, unlit_t = templates[grp['template_group']]
        else:
            lit_idx = grp['lit_in_source']
            lx, ly, lw, lh = rects[lit_idx]
            lit_t = erase_text(src[ly:ly + lh, lx:lx + lw].copy(), True)
            lit_halo = src[ly - margin:ly + lh + margin, lx - margin:lx + lw + margin].copy()
            # gabarit éteint : le bouton le plus loin du bouton allumé (sans reflet orange)
            far = max((j for j in range(len(rects)) if j != lit_idx),
                      key=lambda j: abs(rects[j][0] - lx) + abs(rects[j][1] - ly), default=None)
            if far is not None:
                fx, fy, fw, fh = rects[far]
                unlit_t = erase_text(src[fy:fy + fh, fx:fx + fw].copy(), False)
            else:
                unlit_t = grp_unlit_from_lit(lit_t)
            templates[grp['id']] = (lit_t, lit_halo, unlit_t)
        # Le groupe entier est reconstruit : fond sans halo, puis tous les boutons éteints reposés
        rm = margin + 10
        region = np.zeros((H, W), np.uint8)
        for (ox, oy, ow, oh) in rects:
            cv2.rectangle(region, (ox - rm, oy - rm), (ox + ow + rm, oy + oh + rm), 255, -1)
        for (px0, py0, px1, py1) in grp.get('protect', []):   # titres à ne pas toucher
            region[py0:py1, px0:px1] = 0
        hsvb = cv2.cvtColor(base, cv2.COLOR_BGR2HSV)
        white = ((hsvb[..., 2] > 170) & (hsvb[..., 1] < 45)).astype(np.uint8)
        blobs = cv2.morphologyEx(white, cv2.MORPH_OPEN, np.ones((7, 7), np.uint8))
        lines = cv2.morphologyEx(white, cv2.MORPH_OPEN, np.ones((41, 1), np.uint8))
        lines = lines & (1 - cv2.dilate(blobs, np.ones((5, 5), np.uint8)))
        lines = cv2.dilate(lines, np.ones((1, 3), np.uint8))
        keep = lines > 0
        # le fond autour : couleur médiane hors filets, pour que le blanc ne bave pas
        border = cv2.dilate(region, np.ones((9, 9), np.uint8)) & ~region
        bgcol = np.median(base[(border > 0) & (lines == 0)], axis=0)
        tmp = base.copy()
        tmp[lines > 0] = bgcol
        filled = cv2.inpaint(tmp, region, 9, cv2.INPAINT_TELEA)
        if 'texture_sample' in cfg:
            blur = cv2.GaussianBlur(src, (0, 0), 3).astype(np.float32)
            hf = src.astype(np.float32) - blur
            sx0, sy0, sx1, sy1 = cfg['texture_sample']
            tile = hf[sy0:sy1, sx0:sx1]; th_, tw_ = tile.shape[:2]
            reps = np.tile(tile, (H // th_ + 1, W // tw_ + 1, 1))[:H, :W]
            filled = np.clip(filled.astype(np.float32) + reps * (region[..., None] > 0), 0, 255).astype(np.uint8)
        filled[keep] = base[keep]
        base = filled
        # ombre portée douce sous chaque bouton
        sh = np.zeros((H, W), np.float32)
        for (ox, oy, ow, oh) in rects:
            cv2.rectangle(sh, (ox + 1, oy + 4), (ox + ow - 2, oy + oh + 3), 1.0, -1)
        sh = cv2.GaussianBlur(sh, (0, 0), 4) * 0.55
        base = (base.astype(np.float32) * (1 - sh[..., None])).astype(np.uint8)
        for (ox, oy, ow, oh) in rects:
            base[oy:oy + oh, ox:ox + ow] = cv2.resize(unlit_t, (ow, oh), interpolation=cv2.INTER_AREA)
        grp['rewrite_all'] = True
        # chaque bouton : version éteinte dans la façade, version allumée à part
        for i, (x, y, w, h) in enumerate(rects):
            label = grp['labels'][i]
            fs = grp.get('font', 30)
            if True:
                off = cv2.resize(unlit_t, (w, h), interpolation=cv2.INTER_AREA)
                base[y:y + h, x:x + w] = off
                tmp = Image.fromarray(cv2.cvtColor(base, cv2.COLOR_BGR2RGB)).convert('RGBA')
                draw_label(tmp, (x, y, w, h), label, False, fs)
                base = cv2.cvtColor(np.array(tmp.convert('RGB')), cv2.COLOR_RGB2BGR)
            # allumé : gabarit allumé + halo
            on = cv2.resize(lit_halo, (w + 2 * margin, h + 2 * margin), interpolation=cv2.INTER_AREA).astype(np.float32)
            face = cv2.resize(lit_t, (w, h), interpolation=cv2.INTER_AREA)
            on[margin:margin + h, margin:margin + w] = face
            bg = base[y - margin:y + h + margin, x - margin:x + w + margin].astype(np.float32)
            diff = np.abs(on - bg).max(axis=2)
            a = np.clip(diff / 60.0, 0, 1)
            a[margin:margin + h, margin:margin + w] = 1.0
            rgba = Image.fromarray(np.dstack([cv2.cvtColor(on.clip(0, 255).astype(np.uint8), cv2.COLOR_BGR2RGB),
                                              (a * 255).astype(np.uint8)]), 'RGBA')
            draw_label(rgba, (margin, margin, w, h), label, True, fs)
            name = f"{model}_b_{grp['id']}_{i}.png"
            rgba.save(os.path.join(outdir_res, name))
        controls.append({'type': 'buttons', 'id': grp['id'], 'param': grp['param'], 'values': grp['values'],
                         'rects': rects, 'glow': margin, 'toggle': grp.get('toggle', False),
                         'also': grp.get('also', [])})

    # 3) voyants : la façade de base les montre éteints
    for led in cfg.get('leds', []):
        x0, y0, x1, y1 = led['rect']
        reg = base[y0:y1, x0:x1].copy()
        on = Image.fromarray(cv2.cvtColor(reg, cv2.COLOR_BGR2RGB)).convert('RGBA')
        hsv = cv2.cvtColor(reg, cv2.COLOR_BGR2HSV).astype(np.float32)
        warm = (hsv[..., 1] > 90) & (hsv[..., 2] > 90)
        hsv[..., 1] = np.where(warm, hsv[..., 1] * 0.15, hsv[..., 1])
        hsv[..., 2] = np.where(warm, hsv[..., 2] * 0.45, hsv[..., 2])
        base[y0:y1, x0:x1] = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2BGR)
        on.save(os.path.join(outdir_res, f"{model}_l_{led['id']}.png"))
        controls.append({'type': 'led', 'id': led['id'], 'param': led['param'], 'rect': led['rect'],
                         'on_when': led.get('on_when', 1)})

    # 4) boutons rotatifs
    for k in cfg['knobs']:
        spr = Image.open(f"out/{model}/knob_{k['id']}.png")
        spr.save(os.path.join(outdir_res, f"{model}_k_{k['id']}.png"))
        controls.append({'type': 'knob', 'id': k['id'], 'param': k['param'], 'cx': k['cx'], 'cy': k['cy'],
                         'rs': k['rs'], 'sprite_w': spr.width, 'cal': k['cal'], 'log': k.get('log', False),
                         'map': k.get('map', [1, 0]), 'auto_angle': k.get('auto_angle'),
                         'ptr_len': k.get('ptr_len', k['rs'] * 0.98), 'ptr_w': k.get('ptr_w', k['rs'] * 0.085),
                         'ptr_colour': k.get('ptr_colour', '#f4f4f6'), 'fmt': k.get('fmt', ''),
                         'ptr_in': k.get('ptr_in', k['rs'] * 0.04)})

    for m in cfg.get('meters', []):
        controls.append({'type': 'needle', 'id': m['id'], 'pivot': m['pivot'], 'len': m['len'],
                         'face': m['face'], 'scale': m['scale'], 'channel': 0 if m['id'].endswith('L') else 1,
                         'source': m.get('source', 'gr')})

    cv2.imwrite(os.path.join(outdir_res, f'{model}_panel.jpg'), base, [cv2.IMWRITE_JPEG_QUALITY, 90])
    json.dump({'model': model, 'size': [W, H], 'controls': controls}, open(os.path.join(outdir_layout, f'{model}.json'), 'w'), indent=1)
    return base


def grp_unlit_from_lit(lit):
    g = cv2.cvtColor(lit, cv2.COLOR_BGR2GRAY)
    g = (g.astype(np.float32) * 0.22).astype(np.uint8)
    return cv2.cvtColor(g, cv2.COLOR_GRAY2BGR)


if __name__ == '__main__':
    res, lay = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    os.makedirs(res, exist_ok=True); os.makedirs(lay, exist_ok=True)
    for m in sys.argv[3:]:
        build(m, res, lay)
        print('ok', m)
