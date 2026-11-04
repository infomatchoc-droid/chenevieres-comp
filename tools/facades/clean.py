"""Nettoyage d'une façade : retire les pointeurs des boutons (sprites), les aiguilles
des vu-mètres, et efface les textes à remplacer. Produit :
  out/<model>/panel.png        façade sans aiguilles ni textes effacés
  out/<model>/knob_<id>.png    capuchon de bouton sans pointeur (RGBA, centré)
"""
import cv2, numpy as np, json, os, sys, math

HERE = os.path.dirname(os.path.abspath(__file__))
ORIG = os.path.join(HERE, "..", "..", "design", "facades-originales")
os.chdir(HERE)

def knob_sprite(img, cx, cy, rs, rptr):
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
    angle = (a / NA) * 360.0  # 0 = droite (sens horaire, image)
    return rgba, (c[0], c[1]), math.degrees(math.radians(angle)) + 90  # 0 = haut


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
        rgba, c, ang = knob_sprite(img, k['cx'], k['cy'], k['rs'], k['rptr'])
        cv2.imwrite(f"out/{model}/knob_{k['id']}.png", rgba)
        info[k['id']] = {'orig_pointer_deg': ang}
    for n in cfg.get('meters', []):
        mask |= remove_needle(img, None, n['needle_from'], n['needle_to'], n['face'], arc_y=n.get('arc_y', 0))
    for e in cfg.get('erase', []):
        # zone rectangulaire : on efface les pixels de texte (clairs ou foncés selon 'mode')
        x0, y0, x1, y1 = e['rect']
        sub = cv2.cvtColor(img[y0:y1, x0:x1], cv2.COLOR_BGR2GRAY)
        if e.get('mode', 'all') == 'light':
            mm = (sub > e.get('thr', 120)).astype(np.uint8) * 255
            mm = cv2.dilate(mm, np.ones((5, 5), np.uint8))
        else:
            mm = np.full(sub.shape, 255, np.uint8)
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
        # retire le logo de la façade (il est dans la barre du haut) et recale le titre à gauche
        x0, y0, x1, y1 = hd['erase']
        hm = np.zeros(img.shape[:2], np.uint8); hm[y0:y1, x0:x1] = 255
        bg = cv2.inpaint(clean, hm, 7, cv2.INPAINT_TELEA).astype(np.float32)
        blur = cv2.GaussianBlur(img, (0, 0), 3).astype(np.float32)
        hf = img.astype(np.float32) - blur
        sx0, sy0, sx1, sy1 = cfg['texture_sample']
        tile = hf[sy0:sy1, sx0:sx1]; th, tw = tile.shape[:2]
        reps = np.tile(tile, (img.shape[0] // th + 1, img.shape[1] // tw + 1, 1))[:img.shape[0], :img.shape[1]]
        soft = cv2.GaussianBlur(hm.astype(np.float32) / 255, (0, 0), 2)[..., None]
        bg = bg + reps * soft
        mv = hd.get('move')
        if mv:
            ax0, ay0, ax1, ay1 = mv['rect']; dx = mv['dx']
            src = img[ay0:ay1, ax0:ax1].astype(np.float32)
            ref = cv2.inpaint(img, hm, 7, cv2.INPAINT_TELEA)[ay0:ay1, ax0:ax1].astype(np.float32)
            diff = np.abs(src - ref).max(axis=2)
            a = np.clip((diff - 18) / 90, 0, 1)[..., None]
            dst = bg[ay0:ay1, ax0 + dx:ax1 + dx]
            bg[ay0:ay1, ax0 + dx:ax1 + dx] = dst * (1 - a) + src * a
        clean = np.clip(bg, 0, 255).astype(np.uint8)
    cv2.imwrite(f'out/{model}/panel.png', clean)
    cv2.imwrite(f'out/{model}/mask.png', mask)
    json.dump(info, open(f'out/{model}/info.json', 'w'), indent=1)


if __name__ == '__main__':
    for m in sys.argv[1:]:
        run(m)
