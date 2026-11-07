"""RC-Vox : prépare la façade « plate » (rcvox.png -> rcvox_prep.png).

Retire tout ce qui bouge : arcs orange autour des boutons, pointeurs, valeurs affichées, contenu du
vu-mètre de compression, faders Gate et Gain, ligne et pastille de seuil. Le plugin redessine tout en direct.
Titre et plaque renommés « RC-Vox ».
"""
import os
import cv2
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ORIG = os.path.join(HERE, '..', '..', 'design', 'facades-originales')

# boutons : (centre x, centre y, rayon de l'anneau, rayon du capuchon)
BOTTOM = [(158, 877), (371, 877), (585, 876), (802, 877), (1022, 877), (1389, 878)]
RIGHT = [(1376, 219), (1377, 405), (1377, 601)]
RING_B, CAP_B = (48, 68), 46
RING_R, CAP_R = (37, 54), 39


def main():
    im = cv2.imread(os.path.join(ORIG, 'rcvox.png'))
    H, W = im.shape[:2]
    hsv = cv2.cvtColor(im, cv2.COLOR_BGR2HSV)
    g = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
    orange = (hsv[..., 0] > 4) & (hsv[..., 0] < 30) & (hsv[..., 1] > 60) & (hsv[..., 2] > 120)
    yy, xx = np.mgrid[0:H, 0:W]
    out = im.copy()

    # 1) anneaux : effacés (le plugin redessine le rail gris et l'arc orange)
    rmask = np.zeros((H, W), np.uint8)
    for pts, (r0, r1) in [(BOTTOM, RING_B), (RIGHT, RING_R)]:
        for cx, cy in pts:
            d = np.hypot(xx - cx, yy - cy)
            ang = np.degrees(np.arctan2(xx - cx, -(yy - cy)))
            rmask[(d > r0) & (d < r1) & (np.abs(ang) < 150)] = 255
            rmask[(d > r0) & (d < r1) & orange] = 255
    out = cv2.inpaint(out, cv2.dilate(rmask, np.ones((3, 3), np.uint8)), 6, cv2.INPAINT_TELEA)
    # 2) pointeurs (traits sombres sur capuchons blancs)
    mask = np.zeros((H, W), np.uint8)
    for pts, cap in [(BOTTOM, CAP_B), (RIGHT, CAP_R)]:
        for cx, cy in pts:
            d = np.hypot(xx - cx, yy - cy)
            mask[(d < cap * 0.92) & (g < 175)] = 255
    mask = cv2.dilate(mask, np.ones((5, 5), np.uint8))
    out = cv2.inpaint(out, mask, 5, cv2.INPAINT_TELEA)

    # 2 bis) capuchons lissés : médiane glissante sur l'angle (efface les traces locales, garde l'éclairage)
    for pts, cap, fac in [(BOTTOM, CAP_B, 1.1), (RIGHT, CAP_R, 1.22)]:
        for cx, cy in pts:
            R = int(cap * fac); NA = 720
            crop = out[cy - R:cy + R + 1, cx - R:cx + R + 1].astype(np.float32)
            pol = cv2.warpPolar(crop, (R * 2, NA), (R, R), R, cv2.WARP_POLAR_LINEAR + cv2.INTER_CUBIC)
            k = NA // 8
            ext = np.concatenate([pol[-k:], pol, pol[:k]], axis=0)
            from scipy.ndimage import median_filter
            sm = median_filter(ext, size=(2 * k + 1, 1, 1))[k:-k]
            back = np.nan_to_num(cv2.warpPolar(sm, (crop.shape[1], crop.shape[0]), (R, R), R, cv2.WARP_POLAR_LINEAR + cv2.INTER_LINEAR + cv2.WARP_INVERSE_MAP), nan=0.0)
            yy2, xx2 = np.mgrid[0:crop.shape[0], 0:crop.shape[1]]
            d = np.hypot(xx2 - R, yy2 - R)
            a = np.clip((R - 3 - d) / 3, 0, 1)[..., None]
            out[cy - R:cy + R + 1, cx - R:cx + R + 1] = np.clip(back * a + crop * (1 - a), 0, 255).astype(np.uint8)

    # 3) valeurs affichées, faders, ligne de seuil
    mask = np.zeros((H, W), np.uint8)
    VALUES = [(100, 948, 216, 978), (330, 948, 414, 978), (545, 948, 630, 978), (720, 948, 856, 978),
              (374, 180, 437, 213), (380, 672, 452, 706), (1084, 180, 1152, 212), (1046, 672, 1192, 706),
              (646, 648, 903, 700)]
    mask[248:612, 390:432] = 255                       # rail Gate
    cv2.circle(mask, (410, 614), 30, 255, -1)          # poignée Gate
    mask[226:622, 1098:1144] = 255                     # rail Gain + barres de sortie
    cv2.circle(mask, (1120, 260), 32, 255, -1)         # poignée Gain
    # halo orange autour des faders
    zone = np.zeros((H, W), np.uint8); zone[230:660, 370:450] = 1; zone[200:640, 1080:1165] = 1
    mask |= (cv2.dilate((orange & (zone > 0)).astype(np.uint8) * 255, np.ones((9, 9), np.uint8)))
    cv2.circle(mask, (410, 614), 32, 255, -1)
    cv2.circle(mask, (1120, 260), 38, 255, -1)
    line = np.zeros((H, W), np.uint8)
    line[392:418, 636:912] = 255                       # ligne de seuil
    line[360:452, 696:852] = 255                       # pastille
    mask |= line
    out = cv2.inpaint(out, mask, 7, cv2.INPAINT_TELEA)

    # faders : fond refait par interpolation horizontale entre les bords (panneau uni)
    for x0, y0, x1, y1 in [(392, 236, 448, 646), (1078, 214, 1166, 642)] + VALUES:
        L = out[y0:y1, x0 - 1].astype(np.float32); R = out[y0:y1, x1].astype(np.float32)
        t = np.linspace(0, 1, x1 - x0)[None, :, None]
        out[y0:y1, x0:x1] = (L[:, None] * (1 - t) + R[:, None] * t).astype(np.uint8)
    # 4) intérieur du vu-mètre : fond bleu nuit refait (le rouge est dessiné en direct)
    x0, y0, x1, y1 = 673, 240, 876, 606
    nav = np.array([80, 55, 19], np.float32)
    t = np.linspace(0, 1, y1 - y0)[:, None, None]
    grad = nav * (1.15 - 0.3 * t)
    inner = np.broadcast_to(grad, (y1 - y0, x1 - x0, 3)).copy()
    rr = 22
    m = np.zeros((y1 - y0, x1 - x0), np.uint8)
    cv2.rectangle(m, (rr, 0), (x1 - x0 - rr, y1 - y0), 255, -1)
    cv2.rectangle(m, (0, rr), (x1 - x0, y1 - y0 - rr), 255, -1)
    for cx, cy in [(rr, rr), (x1 - x0 - rr, rr), (rr, y1 - y0 - rr), (x1 - x0 - rr, y1 - y0 - rr)]:
        cv2.circle(m, (cx, cy), rr, 255, -1)
    m = cv2.GaussianBlur(m.astype(np.float32) / 255, (0, 0), 0.8)[..., None]
    reg = out[y0:y1, x0:x1].astype(np.float32)
    # le fond autour (cadre sombre) : bleu plus foncé
    frame = np.array([44, 24, 6], np.float32)
    reg = frame * (1 - m) + inner * m
    for db in range(6, 37, 6):                          # lignes de graduation
        y = int(round(241.6 + db * 10.07)) - y0
        if 0 < y < y1 - y0:
            reg[y, 4:-4] = reg[y, 4:-4] * 0.6 + np.array([120, 100, 70]) * 0.4
    out[y0:y1, x0:x1] = np.clip(reg, 0, 255).astype(np.uint8)
    # bande de cadre autour de l'intérieur, sous l'ancienne ligne de seuil : même bleu foncé
    for xa, xb in [(628, 673), (876, 922)]:            # cadre sous l'ancienne ligne de seuil : recopié de plus haut
        out[384:430, xa:xb] = im[318:364, xa:xb]

    # titre : logo et filet retirés, « R-Comp Vox » effacé (redessiné « RC-Vox » par build_assets)
    for (x0, y0, x1, y1), (s0, s1) in [((78, 12, 476, 102), (482, 566)), ((578, 10, 990, 73), (478, 560))]:
        strip = out[y0:y1, s0:s1]
        parts, k = [], 0
        while sum(p.shape[1] for p in parts) < x1 - x0:
            parts.append(strip if k % 2 == 0 else strip[:, ::-1]); k += 1
        out[y0:y1, x0:x1] = np.concatenate(parts, axis=1)[:, :x1 - x0]
    cv2.imwrite(os.path.join(ORIG, 'rcvox_prep.png'), out)
    print('ok rcvox_prep')


if __name__ == '__main__':
    main()
