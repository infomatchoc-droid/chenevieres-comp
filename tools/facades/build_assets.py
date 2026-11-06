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
        if 'shadow' in t:
            sdx, sdy, sblur, salpha = t['shadow']
            from PIL import ImageFilter
            shl = Image.new('RGBA', pil.size, (0, 0, 0, 0))
            ImageDraw.Draw(shl).text((t['x'] + sdx, t['y'] + sdy), t['s'], font=f, fill=(0, 0, 0, int(salpha * 255)), anchor=t.get('anchor', 'ls'))
            pil.alpha_composite(shl.filter(ImageFilter.GaussianBlur(sblur)))
            d = ImageDraw.Draw(pil)
        if 'parts' in t:
            x = t['x']
            for txt, pc in t['parts']:
                d.text((x, t['y']), txt, font=f, fill=tuple(pc), anchor='ls')
                x += d.textlength(txt, font=f)
            continue
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
        r = (arc['r'] + arc['w'] / 2) * S   # PIL trace l'épaisseur vers l'intérieur
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
            lit_t = src[ly:ly + lh, lx:lx + lw].copy() if grp.get('no_erase') else erase_text(src[ly:ly + lh, lx:lx + lw].copy(), True)
            lit_halo = src[ly - margin:ly + lh + margin, lx - margin:lx + lw + margin].copy()
            # gabarit éteint : le bouton le plus loin du bouton allumé (sans reflet orange)
            far = max((j for j in range(len(rects)) if j != lit_idx),
                      key=lambda j: abs(rects[j][0] - lx) + abs(rects[j][1] - ly), default=None)
            if 'unlit_rect' in grp:
                fx, fy, fw, fh = grp['unlit_rect']
                unlit_t = erase_text(src[fy:fy + fh, fx:fx + fw].copy(), False)
            elif far is not None:
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
            base[oy:oy + oh, ox:ox + ow] = nine(unlit_t, ow, oh)
        grp['rewrite_all'] = True
        # chaque bouton : version éteinte dans la façade, version allumée à part
        for i, (x, y, w, h) in enumerate(rects):
            label = grp['labels'][i]
            fs = grp.get('font', 30)
            if True:
                off = nine(unlit_t, w, h)
                base[y:y + h, x:x + w] = off
                tmp = Image.fromarray(cv2.cvtColor(base, cv2.COLOR_BGR2RGB)).convert('RGBA')
                if isinstance(label, dict):
                    paste_icon(tmp, src, label['icon'], (x, y, w, h), False)
                else:
                    draw_label(tmp, (x, y, w, h), label, False, fs)
                base = cv2.cvtColor(np.array(tmp.convert('RGB')), cv2.COLOR_RGB2BGR)
            # allumé : gabarit allumé + halo
            on = nine(lit_halo, w + 2 * margin, h + 2 * margin, margin + 12).astype(np.float32)
            face = nine(lit_t, w, h)
            on[margin:margin + h, margin:margin + w] = face
            bg = base[y - margin:y + h + margin, x - margin:x + w + margin].astype(np.float32)
            diff = np.abs(on - bg).max(axis=2)
            a = np.clip(diff / 60.0, 0, 1)
            a[margin:margin + h, margin:margin + w] = 1.0
            rgba = Image.fromarray(np.dstack([cv2.cvtColor(on.clip(0, 255).astype(np.uint8), cv2.COLOR_BGR2RGB),
                                              (a * 255).astype(np.uint8)]), 'RGBA')
            if isinstance(label, dict):
                paste_icon(rgba, src, label['icon'], (margin, margin, w, h), True)
            else:
                draw_label(rgba, (margin, margin, w, h), label, True, fs)
            name = f"{model}_b_{grp['id']}_{i}.png"
            rgba.save(os.path.join(outdir_res, name))
        controls.append({'type': 'buttons', 'id': grp['id'], 'param': grp['param'], 'values': grp['values'],
                         'rects': rects, 'glow': margin, 'toggle': grp.get('toggle', False),
                         'also': grp.get('also', []), 'off': grp.get('off'), 'lit_above': grp.get('lit_above')})

    # 3) voyants : la façade de base les montre éteints
    for led in cfg.get('leds', []):
        x0, y0, x1, y1 = led['rect']
        w, h = x1 - x0, y1 - y0
        if 'lit_src' in led:
            # voyant rond : éteint = copie d'un voyant éteint de la façade, allumé = copie d'un voyant allumé
            lx, ly = led['lit_src']; ox, oy = led['off_src']
            lit = src[ly:ly + h, lx:lx + w].astype(np.float32)
            off = src[oy:oy + h, ox:ox + w].copy()
            m = np.zeros((h, w), np.float32)
            cv2.circle(m, (w // 2, h // 2), min(w, h) // 2 - 1, 1.0, -1)
            m = cv2.GaussianBlur(m, (0, 0), 2)[..., None]
            base[y0:y1, x0:x1] = (off.astype(np.float32) * m + base[y0:y1, x0:x1].astype(np.float32) * (1 - m)).astype(np.uint8)
            a = np.clip(np.abs(lit - base[y0:y1, x0:x1].astype(np.float32)).max(axis=2) / 40, 0, 1) * m[..., 0]
            on = Image.fromarray(np.dstack([cv2.cvtColor(lit.astype(np.uint8), cv2.COLOR_BGR2RGB), (a * 255).astype(np.uint8)]), 'RGBA')
        elif led.get('grey'):
            reg = base[y0:y1, x0:x1].copy()
            on = Image.fromarray(cv2.cvtColor(reg, cv2.COLOR_BGR2RGB)).convert('RGBA')
            hsv = cv2.cvtColor(reg, cv2.COLOR_BGR2HSV).astype(np.float32)
            mm = np.zeros((h, w), np.float32); cv2.circle(mm, (w // 2, h // 2), led['grey'], 1.0, -1)
            mm = cv2.GaussianBlur(mm, (0, 0), 1.5)
            hsv[..., 1] *= 1 - 0.85 * mm
            hsv[..., 2] *= 1 - 0.68 * mm
            base[y0:y1, x0:x1] = cv2.cvtColor(np.clip(hsv, 0, 255).astype(np.uint8), cv2.COLOR_HSV2BGR)
            # halo autour de la lampe : grisé aussi
            ring = np.zeros((H, W), np.float32); cv2.circle(ring, ((x0 + x1) // 2, (y0 + y1) // 2), led['grey'] + 16, 1.0, -1)
            ring = cv2.GaussianBlur(ring, (0, 0), 3)
            hz = cv2.cvtColor(base, cv2.COLOR_BGR2HSV).astype(np.float32); hz[..., 1] *= 1 - 0.8 * ring
            base = cv2.cvtColor(np.clip(hz, 0, 255).astype(np.uint8), cv2.COLOR_HSV2BGR)
            a = np.clip(np.abs(cv2.cvtColor(np.array(on.convert('RGB')), cv2.COLOR_RGB2BGR).astype(np.float32) - base[y0:y1, x0:x1].astype(np.float32)).max(axis=2) / 30, 0, 1)
            on.putalpha(Image.fromarray((a * 255).astype(np.uint8)))
        else:
            reg = base[y0:y1, x0:x1].copy()
            on = Image.fromarray(cv2.cvtColor(reg, cv2.COLOR_BGR2RGB)).convert('RGBA')
            hsv = cv2.cvtColor(reg, cv2.COLOR_BGR2HSV).astype(np.float32)
            warm = (hsv[..., 1] > 90) & (hsv[..., 2] > 90)
            hsv[..., 1] = np.where(warm, hsv[..., 1] * 0.15, hsv[..., 1])
            hsv[..., 2] = np.where(warm, hsv[..., 2] * 0.45, hsv[..., 2])
            base[y0:y1, x0:x1] = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2BGR)
        on.save(os.path.join(outdir_res, f"{model}_l_{led['id']}.png"))
        hit = led.get('hit', led['rect'])
        controls.append({'type': 'led', 'id': led['id'], 'param': led['param'], 'rect': led['rect'], 'hit': hit,
                         'on': led.get('on', 1), 'off': led.get('off', 0), 'lit_above': led.get('lit_above')})

    # 3 bis) barre de LED de réduction de gain : éteintes dans la façade, allumées à part
    for bar in cfg.get('ledbars', []):
        r = bar['r']; g = bar.get('glow', 16)
        leds = []
        orig = base.copy()
        full = np.zeros((H, W), np.uint8)
        for (cx, cy, v) in bar['leds']:
            cv2.circle(full, (cx, cy), r + g, 255, -1)
        # le texte au-dessus des LED est protégé
        for (px0, py0, px1, py1) in bar.get('protect', []):
            full[py0:py1, px0:px1] = 0
        base = cv2.inpaint(base, full, 6, cv2.INPAINT_TELEA)
        # plus de reflet coloré autour des LED : la zone de la barre repasse en gris
        xs = [l[0] for l in bar['leds']]; ys = [l[1] for l in bar['leds']]
        m = r + g + 14
        zx0, zx1, zy0, zy1 = min(xs) - m, max(xs) + m, min(ys) - m, max(ys) + m
        zone = np.zeros((H, W), np.float32); zone[zy0:zy1, zx0:zx1] = 1
        for (px0, py0, px1, py1) in bar.get('protect', []):
            zone[py0:py1, px0:px1] = 0
        zone = cv2.GaussianBlur(zone, (0, 0), 5)
        hsvz = cv2.cvtColor(base, cv2.COLOR_BGR2HSV).astype(np.float32)
        hsvz[..., 1] *= 1 - 0.85 * zone
        hsvz[..., 2] = hsvz[..., 2] * (1 - zone) + np.minimum(hsvz[..., 2], np.median(hsvz[zy0:zy1, zx0:zx1, 2]) + 6) * zone
        base = cv2.cvtColor(np.clip(hsvz, 0, 255).astype(np.uint8), cv2.COLOR_HSV2BGR)
        # LED éteintes dessinées : verre sombre teinté de la couleur de la LED, cerclage noir, petit reflet
        S = 4
        pil2 = Image.fromarray(cv2.cvtColor(base, cv2.COLOR_BGR2RGB)).convert('RGBA')
        lay = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))
        dl = ImageDraw.Draw(lay)
        for (cx, cy, v) in bar['leds']:
            col = orig[cy, cx].astype(int)[::-1]          # RGB du centre allumé
            off = tuple(int(c * 0.15 + 8) for c in col)
            rr = (r + 2.5) * S
            dl.ellipse([cx * S - rr, cy * S - rr, cx * S + rr, cy * S + rr], fill=(6, 6, 6, 255))
            rr = r * S
            dl.ellipse([cx * S - rr, cy * S - rr, cx * S + rr, cy * S + rr], fill=off + (255,))
            hr = r * 0.35 * S
            hx, hy = cx * S - r * 0.35 * S, cy * S - r * 0.4 * S
            dl.ellipse([hx - hr, hy - hr * 0.7, hx + hr, hy + hr * 0.7], fill=(255, 255, 255, 38))
        pil2.alpha_composite(lay.resize((W, H), Image.LANCZOS))
        base = cv2.cvtColor(np.array(pil2.convert('RGB')), cv2.COLOR_RGB2BGR)
        for i, (cx, cy, v) in enumerate(bar['leds']):
            R2 = r + g
            x0, y0 = cx - R2, cy - R2
            on = orig[y0:y0 + 2 * R2, x0:x0 + 2 * R2].astype(np.float32)
            off = base[y0:y0 + 2 * R2, x0:x0 + 2 * R2].astype(np.float32)
            a = np.clip(np.abs(on - off).max(axis=2) / 40, 0, 1)
            yy, xx = np.mgrid[0:2 * R2, 0:2 * R2]
            a *= np.clip((R2 - np.hypot(xx - R2, yy - R2)) / 3, 0, 1)
            rgba = np.dstack([cv2.cvtColor(on.astype(np.uint8), cv2.COLOR_BGR2RGB), (a * 255).astype(np.uint8)])
            Image.fromarray(rgba, 'RGBA').save(os.path.join(outdir_res, f"{model}_m_{bar['id']}_{i}.png"))
            leds.append([x0, y0, 2 * R2, 2 * R2, v])
        controls.append({'type': 'ledbar', 'id': bar['id'], 'channel': bar.get('channel', -1), 'leds': leds})

    # 3 ter) inverseurs à levier : levier effacé de la façade, redessiné pour chaque position
    for sw in cfg.get('switches', []):
        cx, cy = sw['center']; L = sw['len']; br = sw['ball_r']
        tx, ty = sw['tip_now']
        nr = sw['nut_r']
        m = np.zeros((H, W), np.uint8)
        cv2.circle(m, (int(cx), int(cy)), int(nr + 4), 255, -1)
        cv2.line(m, (int(cx), int(cy)), (int(tx), int(ty)), 255, int(sw.get('rod_w', 12) + 14))
        cv2.circle(m, (int(tx), int(ty)), int(br + 9), 255, -1)
        base = cv2.inpaint(base, m, 8, cv2.INPAINT_TELEA)
        # écrou redessiné : ombre, bague crantée, bague polie, canon
        pilb = Image.fromarray(cv2.cvtColor(base, cv2.COLOR_BGR2RGB)).convert('RGBA')
        S = 4; R = int(max(L + br, nr) + 16)
        col0 = np.array(sw.get('colour', [196, 160, 118]), float)
        nut = Image.new('RGBA', (2 * R * S, 2 * R * S), (0, 0, 0, 0))
        dn = ImageDraw.Draw(nut); C = R * S
        def disc(r, fill, dx=0, dy=0): dn.ellipse([C + dx - r, C + dy - r, C + dx + r, C + dy + r], fill=fill)
        from PIL import ImageFilter
        shn = Image.new('RGBA', nut.size, (0, 0, 0, 0)); ImageDraw.Draw(shn).ellipse([C + 3 * S - nr * S, C + 5 * S - nr * S, C + 3 * S + nr * S, C + 5 * S + nr * S], fill=(0, 0, 0, 170))
        shn = shn.filter(ImageFilter.GaussianBlur(4 * S))
        for k in range(36):            # bague crantée
            t = k / 36 * 2 * math.pi
            shade = 0.35 + 0.35 * (0.5 + 0.5 * math.cos(t + 2.2))
            cc = tuple(int(v * shade) for v in col0) + (255,)
            dn.pieslice([C - nr * S, C - nr * S, C + nr * S, C + nr * S], math.degrees(t), math.degrees(t) + 10, fill=cc)
        disc(nr * 0.8 * S, (10, 8, 6, 255))
        for k in range(28, 0, -1):     # bague polie (reflets concentriques)
            r = nr * 0.74 * S * k / 28
            f = 1 - k / 28
            shade = 0.3 + 0.95 * abs(math.sin(f * 7 + 0.6))
            disc(r, tuple(int(min(255, v * shade)) for v in col0) + (255,), -0.06 * r, -0.08 * r)
        disc(nr * 0.5 * S, tuple(int(v * 0.35) for v in col0) + (255,))
        disc(sw.get('rod_w', 12) * 0.7 * S, (14, 11, 8, 255))
        nutimg = Image.alpha_composite(shn, nut).resize((2 * R, 2 * R), Image.LANCZOS)
        pilb.alpha_composite(nutimg, (int(cx - R), int(cy - R)))
        base = cv2.cvtColor(np.array(pilb.convert('RGB')), cv2.COLOR_RGB2BGR)
        col = np.array(sw.get('colour', [196, 160, 118]), float)
        for i, ang in enumerate(sw['angles']):
            img = Image.new('RGBA', (2 * R * S, 2 * R * S), (0, 0, 0, 0))
            sh = Image.new('RGBA', img.size, (0, 0, 0, 0))
            a = math.radians(ang); ux, uy = math.sin(a), -math.cos(a); px_, py_ = -uy, ux
            c0 = np.array([R * S, R * S], float); tip = c0 + np.array([ux, uy]) * L * S
            w0, w1 = sw.get('rod_w', 12) * S, sw.get('rod_w', 12) * 0.75 * S
            ds = ImageDraw.Draw(sh)
            ds.line([tuple(c0 + [4 * S, 6 * S]), tuple(tip + [4 * S, 6 * S])], fill=(0, 0, 0, 150), width=int(w0))
            ds.ellipse([tip[0] + 4 * S - br * S, tip[1] + 6 * S - br * S, tip[0] + 4 * S + br * S, tip[1] + 6 * S + br * S], fill=(0, 0, 0, 150))
            from PIL import ImageFilter
            sh = sh.filter(ImageFilter.GaussianBlur(4 * S))
            d2 = ImageDraw.Draw(img)
            # tige : bandes parallèles, claire au centre
            n = 14
            for k in range(n):
                t = (k + 0.5) / n * 2 - 1
                shade = 0.45 + 0.75 * (1 - abs(t + 0.25)) ** 1.5
                cc = tuple(int(min(255, v * shade)) for v in col) + (255,)
                off0 = np.array([px_, py_]) * t * w0 / 2; off1 = np.array([px_, py_]) * t * w1 / 2
                d2.line([tuple(c0 + off0), tuple(tip + off1)], fill=cc, width=int(w0 / n * 1.6) + 1)
            # boule : dégradé radial
            for k in range(24, 0, -1):
                rr = br * S * k / 24
                f = 1 - k / 24
                shade = 0.5 + 0.9 * f
                cxk, cyk = tip[0] - br * S * 0.3 * f, tip[1] - br * S * 0.35 * f
                cc = tuple(int(min(255, v * shade)) for v in col) + (255,)
                d2.ellipse([cxk - rr, cyk - rr, cxk + rr, cyk + rr], fill=cc)
            out = Image.alpha_composite(sh, img).resize((2 * R, 2 * R), Image.LANCZOS)
            out.save(os.path.join(outdir_res, f"{model}_s_{sw['id']}_{i}.png"))
        controls.append({'type': 'switch', 'id': sw['id'], 'param': sw['param'], 'values': sw['values'],
                         'rect': [int(cx - R), int(cy - R), 2 * R, 2 * R], 'hit': sw.get('hit', [int(cx - R), int(cy - R), 2 * R, 2 * R])})

    # 4) boutons rotatifs
    for k in cfg['knobs']:
        spr = Image.open(f"out/{model}/knob_{k['id']}.png")
        spr.save(os.path.join(outdir_res, f"{model}_k_{k['id']}.png"))
        controls.append({'type': 'knob', 'id': k['id'], 'param': k['param'], 'cx': k['cx'], 'cy': k['cy'],
                         'rs': k['rs'], 'sprite_w': spr.width, 'cal': k['cal'], 'log': k.get('log', False),
                         'map': k.get('map', [1, 0]), 'auto_angle': k.get('auto_angle'),
                         'ptr_len': k.get('ptr_len', k['rs'] * 0.98), 'ptr_w': k.get('ptr_w', k['rs'] * 0.085),
                         'ptr_colour': k.get('ptr_colour', '#f4f4f6'), 'fmt': k.get('fmt', ''),
                         'ptr_in': k.get('ptr_in', k['rs'] * 0.04), 'steps': k.get('steps', [])})

    for m in cfg.get('meters', []):
        controls.append({'type': 'needle', 'id': m['id'], 'pivot': m['pivot'], 'len': m['len'],
                         'face': m['face'], 'scale': m['scale'], 'channel': 0 if m['id'].endswith('L') else 1,
                         'source': m.get('source', 'gr')})

    cv2.imwrite(os.path.join(outdir_res, f'{model}_panel.jpg'), base, [cv2.IMWRITE_JPEG_QUALITY, 90])
    json.dump({'model': model, 'size': [W, H], 'controls': controls}, open(os.path.join(outdir_layout, f'{model}.json'), 'w'), indent=1)
    return base


def paste_icon(dst, src, rect, box, lit):
    """Pictogramme sombre d'un bouton allumé d'origine, recentré dans box (couleur selon l'état)."""
    x, y, w, h = rect
    crop = cv2.cvtColor(src[y:y + h, x:x + w], cv2.COLOR_BGR2GRAY).astype(np.float32)
    med = cv2.medianBlur(src[y:y + h, x:x + w], 2 * (min(w, h) // 3) + 1)
    gm = cv2.cvtColor(med, cv2.COLOR_BGR2GRAY).astype(np.float32)
    a = np.clip((gm - crop - 20) / 50, 0, 1)
    m = max(4, int(min(w, h) * 0.14)); a[:m] = 0; a[-m:] = 0; a[:, :m] = 0; a[:, -m:] = 0
    col = (43, 24, 4) if lit else (232, 232, 234)
    icon = Image.fromarray(np.dstack([np.full((h, w), c, np.uint8) for c in col] + [(a * 255).astype(np.uint8)]), 'RGBA')
    bx, by, bw, bh = box
    dst.alpha_composite(icon, (int(bx + (bw - w) / 2), int(by + (bh - h) / 2)))


def nine(img, w, h, b=12):
    """Redimensionne un bouton en gardant ses coins et ses bords (9 parties)."""
    H0, W0 = img.shape[:2]
    if (W0, H0) == (w, h):
        return img.copy()
    b = min(b, W0 // 3, H0 // 3, w // 3, h // 3)
    xs0, xs1 = [0, b, W0 - b, W0], [0, b, w - b, w]
    ys0, ys1 = [0, b, H0 - b, H0], [0, b, h - b, h]
    out = np.zeros((h, w) + img.shape[2:], img.dtype)
    for i in range(3):
        for j in range(3):
            part = img[ys0[i]:ys0[i + 1], xs0[j]:xs0[j + 1]]
            tw, th = xs1[j + 1] - xs1[j], ys1[i + 1] - ys1[i]
            if tw > 0 and th > 0:
                out[ys1[i]:ys1[i + 1], xs1[j]:xs1[j + 1]] = cv2.resize(part, (tw, th), interpolation=cv2.INTER_AREA if tw * th < part.shape[0] * part.shape[1] else cv2.INTER_CUBIC)
    return out


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
