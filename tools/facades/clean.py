"""Nettoyage d'une façade : retire les pointeurs des boutons (sprites), les aiguilles
des vu-mètres, et efface les textes à remplacer. Produit :
  out/<model>/panel.png        façade sans aiguilles ni textes effacés
  out/<model>/knob_<id>.png    capuchon de bouton sans pointeur (RGBA, centré)
"""
import cv2, numpy as np, json, os, sys, math

HERE = os.path.dirname(os.path.abspath(__file__))
ORIG = os.path.join(HERE, "..", "..", "design", "facades-originales")
os.chdir(HERE)

def remove_band_at(pol, NA, sc, ang_deg, half_deg):
    """Efface une bande d'angle connu (0 = haut, sens horaire), par interpolation radiale à chaque rayon."""
    centre = ((ang_deg - 90) % 360) / 360 * NA
    lo, hi = int(centre - half_deg / 360 * NA), int(centre + half_deg / 360 * NA)
    for j in range(pol.shape[1]):
        r = max(j / sc, 1.0)
        extra = int(min(NA // 6, 6.0 / (2 * math.pi * r) * NA))
        l2, h2 = lo - extra, hi + extra
        L = h2 - l2
        A, B = pol[l2 % NA, j].copy(), pol[h2 % NA, j].copy()
        for k in range(1, L):
            t = k / L
            pol[(l2 + k) % NA, j] = A * (1 - t) + B * t
    return pol


def remove_band(pol, NA, sc, rptr):
    rr0, rr1 = int(rptr * 0.25 * sc), int(rptr * 0.95 * sc)
    lum = pol[:, rr0:rr1].mean(axis=2)
    med = np.median(lum, axis=0, keepdims=True)
    dev = np.abs(lum - med).mean(axis=1)
    dev = np.convolve(np.concatenate([dev[-20:], dev, dev[:20]]), np.ones(9) / 9, 'same')[20:-20]
    a = int(np.argmax(dev))
    # largeur de bande : là où l'écart dépasse un seuil
    thr = np.median(dev) + 0.35 * (dev[a] - np.median(dev))
    lo = a
    while dev[(lo - 1) % NA] > thr and (a - lo) < NA // 8: lo -= 1
    hi = a
    while dev[(hi + 1) % NA] > thr and (hi - a) < NA // 8: hi += 1
    w = max(hi - lo, 8)
    lo -= int(w * 0.6) + 6; hi += int(w * 0.6) + 6
    # près du centre, le pointeur occupe un angle plus large : bande élargie rayon par rayon
    ptr_px = rptr * 0.12
    for j in range(pol.shape[1]):
        r = max(j / sc, 1.0)
        extra = int(min(NA // 4, ptr_px / (2 * math.pi * r) * NA))
        l2, h2 = lo - extra, hi + extra
        L = h2 - l2
        A, B = pol[l2 % NA, j].copy(), pol[h2 % NA, j].copy()
        for k in range(1, L):
            t = k / L
            pol[(l2 + k) % NA, j] = A * (1 - t) + B * t

    return pol


def knob_sprite(img, cx, cy, rs, rptr, bands=1, erase_at=None):
    """Capuchon + jupe de rayon rs, pointeur supprimé par interpolation angulaire."""
    pad = 4
    R = int(math.ceil(rs)) + pad
    x0, y0 = int(round(cx)) - R, int(round(cy)) - R
    crop = img[y0:y0 + 2 * R + 1, x0:x0 + 2 * R + 1].astype(np.float32)
    c = (cx - x0, cy - y0)
    NA = 1440
    pol = cv2.warpPolar(crop, (R * 2, NA), c, R, cv2.WARP_POLAR_LINEAR + cv2.INTER_CUBIC)
    # pol[angle, radius]; radius index scale = R*2 / R
    sc = (R * 2) / R
    if bands == 0:
        pass
    elif erase_at:
        pol = remove_band_at(pol, NA, sc, erase_at[0], erase_at[1])
    else:
        for _ in range(bands):
            pol = remove_band(pol, NA, sc, rptr)
    out = cv2.warpPolar(pol, (crop.shape[1], crop.shape[0]), c, R,
                        cv2.WARP_POLAR_LINEAR + cv2.INTER_CUBIC + cv2.WARP_INVERSE_MAP)
    # le centre (quelques px) est mal défini en polaire : moyenne locale
    yy, xx = np.mgrid[0:crop.shape[0], 0:crop.shape[1]]
    d = np.hypot(xx - c[0], yy - c[1])
    ann = (d > rptr * 0.2) & (d < rptr * 0.28)
    mean = out[ann].mean(axis=0)
    w = np.clip((d - rptr * 0.13) / (rptr * 0.09), 0, 1)[..., None]
    out = out * w + mean * (1 - w)
    alpha = np.clip(rs + 0.5 - d, 0, 1)
    rgba = np.dstack([np.nan_to_num(np.clip(out, 0, 255)), alpha * 255]).astype(np.uint8)
    return rgba, (c[0], c[1]), 0.0


def remove_needle(img, mask_poly_pts, pivot, tip, face_rect, dark_thr=90, arc_y=0):
    x0, y0, x1, y1 = face_rect
    m = np.zeros(img.shape[:2], np.uint8)
    cv2.line(m, tuple(map(int, pivot)), tuple(map(int, tip)), 255, 9)
    face = np.zeros_like(m); face[y0:y1, x0:x1] = 255
    g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    dark = m & face & ((img.max(axis=2) < 75).astype(np.uint8) * 255)
    dark = cv2.dilate(dark, np.ones((5, 5), np.uint8))
    # sous l'arc de l'échelle : on efface toute la largeur (ombre et reflet de l'aiguille)
    full = np.zeros_like(m)
    cv2.line(full, tuple(map(int, pivot)), tuple(map(int, tip)), 255, 15)
    full[:int(arc_y), :] = 0
    return (dark | (full & face))


def run(model):
    cfg = json.load(open(f'layouts/{model}.json'))
    img = cv2.imread(os.path.join(ORIG, f'{model}.png'))
    os.makedirs(f'out/{model}', exist_ok=True)
    mask = np.zeros(img.shape[:2], np.uint8)
    info = {}
    for k in cfg['knobs']:
        rgba, c, ang = knob_sprite(img, k['cx'], k['cy'], k['rs'], k['rptr'], k.get('bands', 1), k.get('erase_at'))
        cv2.imwrite(f"out/{model}/knob_{k['id']}.png", rgba)
        info[k['id']] = {'orig_pointer_deg': ang}
    needle_fill = []
    for n in cfg.get('meters', []):
        if n.get('band'):
            # aiguille repérée par écart au fond lissé du cadran, dans une bande autour de sa position
            band = np.zeros(img.shape[:2], np.uint8)
            cv2.line(band, tuple(n['needle_from']), tuple(n['needle_to']), 255, n.get('band_w', 28))
            if n.get('arc_y'):
                # au-dessus de l'arc (graduations, chiffres) : bande étroite seulement
                band[:int(n['arc_y']), :] = 0
                narrow = np.zeros_like(band)
                cv2.line(narrow, tuple(n['needle_from']), tuple(n['needle_to']), 255, 9)
                narrow[int(n['arc_y']):, :] = 0
                band |= narrow
            fx0, fy0, fx1, fy1 = n['face']; face = np.zeros_like(band); face[fy0:fy1, fx0:fx1] = 255
            smooth = cv2.medianBlur(img, 41).astype(np.int32)
            dev = np.abs(img.astype(np.int32) - smooth).max(axis=2)
            m = ((dev > n.get("dev", 16)).astype(np.uint8) * 255) & band & face
            m = cv2.dilate(m, np.ones((5, 5), np.uint8))
            mask |= m
            needle_fill.append((m, cv2.GaussianBlur(cv2.medianBlur(img, 61), (0, 0), 3)))
        else:
            mask |= remove_needle(img, None, n['needle_from'], n['needle_to'], n['face'], arc_y=n.get('arc_y', 0))
    for t in cfg.get('texts', []):
        if 'erase' in t:
            cfg.setdefault('erase', []).append({'rect': t['erase'], 'mode': t.get('erase_mode', 'all'), 'thr': t.get('erase_thr', 120),
                                                'keep_circle': t.get('keep_circle')})
    for e in cfg.get('erase', []):
        # zone rectangulaire : on efface les pixels de texte (clairs ou foncés selon 'mode')
        x0, y0, x1, y1 = e['rect']
        sub = cv2.cvtColor(img[y0:y1, x0:x1], cv2.COLOR_BGR2GRAY)
        if e.get('mode', 'all') == 'light':
            mm = (sub > e.get('thr', 120)).astype(np.uint8) * 255
            mm = cv2.dilate(mm, np.ones((5, 5), np.uint8))
        else:
            mm = np.full(sub.shape, 255, np.uint8)
        if e.get('keep_circle'):
            kx, ky, kr = e['keep_circle']
            yy, xx = np.mgrid[y0:y1, x0:x1]
            mm[np.hypot(xx - kx, yy - ky) < kr] = 0
        mask[y0:y1, x0:x1] |= mm
    g = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    H, W = g.shape
    yy, xx = np.mgrid[0:H, 0:W]
    for e in cfg.get('erase_ring', []):
        d = np.hypot(xx - e['cx'], yy - e['cy'])
        ang = np.degrees(np.arctan2(xx - e['cx'], -(yy - e['cy'])))
        sel = (d > e['r0']) & (d < e['r1']) & (g > e['thr']) & (np.abs(ang) < 180 - e.get('skip_bottom', 0))
        mm = cv2.dilate(sel.astype(np.uint8) * 255, np.ones((7, 7), np.uint8))
        mask |= mm
    clean = cv2.inpaint(img, mask, 5, cv2.INPAINT_TELEA)
    for m, fillimg in needle_fill:   # cadran : fond lisse du cadran plutôt que bavure des graduations
        soft = cv2.GaussianBlur(m.astype(np.float32) / 255, (0, 0), 1.2)[..., None]
        clean = (clean.astype(np.float32) * (1 - soft) + fillimg.astype(np.float32) * soft).astype(np.uint8)
    # grain : on réinjecte la texture fine de la façade dans les zones reconstruites
    if mask.any():
        blur = cv2.GaussianBlur(img, (0, 0), 3).astype(np.float32)
        hf = img.astype(np.float32) - blur
        sx0, sy0, sx1, sy1 = cfg.get('texture_sample', [0, 0, 0, 0])
        if sx1 > sx0:
            tile = hf[sy0:sy1, sx0:sx1]
            th, tw = tile.shape[:2]
            reps = np.tile(tile, (img.shape[0] // th + 1, img.shape[1] // tw + 1, 1))[:img.shape[0], :img.shape[1]]
            soft = cv2.GaussianBlur(mask.astype(np.float32) / 255, (0, 0), 1.5)[..., None]
            clean = np.clip(clean.astype(np.float32) + reps * soft, 0, 255).astype(np.uint8)
    hd = cfg.get('header')
    if hd:
        # retire le logo de la façade (il est dans la barre du haut) et recale le titre à gauche.
        # Le fond est refait en répétant une bande propre de la même façade (fill_from = colonnes x0..x1),
        # en miroir pour éviter les raccords.
        x0, y0, x1, y1 = hd['erase']
        fx0, fx1 = hd['fill_from']
        strip = clean[y0:y1, fx0:fx1].copy()
        if 'fill_rows' in hd:   # bande propre répétée verticalement (en miroir)
            r0, r1 = hd['fill_rows']
            src_rows = clean[r0:r1, fx0:fx1]
            parts, k = [], 0
            while sum(p.shape[0] for p in parts) < y1 - y0:
                parts.append(src_rows if k % 2 == 0 else src_rows[::-1]); k += 1
            strip = np.concatenate(parts, axis=0)[:y1 - y0].copy()
        tiles = []
        k = 0
        while sum(t.shape[1] for t in tiles) < x1 - x0:
            tiles.append(strip if k % 2 == 0 else strip[:, ::-1]); k += 1
        fill = np.concatenate(tiles, axis=1)[:, :x1 - x0].astype(np.float32)
        # même luminosité que la façade de part et d'autre (rangée par rangée, interpolée de gauche à droite)
        def rowmean(z): return cv2.GaussianBlur(z.mean(axis=1, keepdims=True), (0, 0), 2)
        left = rowmean(clean[y0:y1, max(0, x0 - 24):x0].astype(np.float32))
        right = rowmean(clean[y0:y1, x1:x1 + 24].astype(np.float32))
        smean = rowmean(strip.astype(np.float32))
        t = np.linspace(0, 1, x1 - x0)[None, :, None]
        if 'match_left' in hd:   # colonnes de référence à gauche (hors vis)
            l0, l1 = hd['match_left']
            left = rowmean(clean[y0:y1, l0:l1].astype(np.float32))
            target = left * (1 - t) + right * t
        else:
            target = right + 0 * t
        if hd.get('match_bg'):
            # décalage unique : fond d'origine de la zone (hors texte clair) contre la bande recopiée
            orig = clean[y0:y1, x0:x1].reshape(-1, 3).astype(np.float32)
            bgpx = orig[orig.max(axis=1) < hd['match_bg']]
            fill = fill + (np.median(bgpx, axis=0) - np.median(fill.reshape(-1, 3), axis=0))
        elif not hd.get('no_match'):
            fill = fill + (target - smean)
        bg = clean.astype(np.float32).copy()
        bg[y0:y1, x0:x1] = fill
        m = np.zeros(img.shape[:2], np.float32); m[y0:y1, x0:x1] = 1
        m = cv2.GaussianBlur(m, (0, 0), sigmaX=6, sigmaY=0.3)
        fb = hd.get('fade_bottom', 0)   # fondu vertical sur le bas de la zone
        if fb:
            m[y1 - fb:y1, :] *= np.linspace(1, 0, fb)[:, None]
        m = m[..., None]
        bg = bg * m + clean.astype(np.float32) * (1 - m)
        bgfill = bg.copy()
        mvs = hd.get('move')
        mvs = [] if not mvs else (mvs if isinstance(mvs, list) else [mvs])
        for mv in mvs:
            ax0, ay0, ax1, ay1 = mv['rect']; dx = mv['dx']
            src = img[ay0:ay1, ax0:ax1].astype(np.float32)
            ref = bgfill[ay0:ay1, ax0:ax1]
            diff = np.abs(src - ref).max(axis=2)
            a = np.clip((diff - 28) / 60, 0, 1)
            a = cv2.GaussianBlur(a, (0, 0), 0.6)[..., None]
            dst = bg[ay0:ay1, ax0 + dx:ax1 + dx]
            bg[ay0:ay1, ax0 + dx:ax1 + dx] = dst * (1 - a) + src * a
        clean = np.clip(bg, 0, 255).astype(np.uint8)
    cv2.imwrite(f'out/{model}/panel.png', clean)
    cv2.imwrite(f'out/{model}/mask.png', mask)
    json.dump(info, open(f'out/{model}/info.json', 'w'), indent=1)


if __name__ == '__main__':
    for m in sys.argv[1:]:
        run(m)
