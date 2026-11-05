"""Analyse d'enregistrements du fichier de test, et comparaison entre plusieurs compresseurs.

   python3 analyse.py --out rapport ref.wav="API 2500 (réf.)" chenevieres.wav="A-2500"

Pour chaque fichier : courbe statique (seuil, ratio, knee ajustés), temps d'attaque et de relâchement,
réduction de gain selon la fréquence (filtres du sidechain), harmoniques, couplage stéréo, couleur sur bruit rose.
Sorties : <out>.txt (rapport), <out>.json, <out>.png (graphiques superposés).
"""
import argparse, json, os, sys
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly, hilbert
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, 'Chenevieres-Comp-test.wav')
SEG = json.load(open(os.path.join(HERE, 'segments.json')))
FS = SEG['fs']


def rms_db(x):
    return 10 * np.log10(np.mean(np.asarray(x, dtype=np.float64) ** 2) + 1e-20)


def load(path):
    y, fs = sf.read(path, dtype='float64', always_2d=True)
    if fs != FS:
        from math import gcd
        g = gcd(int(fs), FS)
        y = resample_poly(y, FS // g, int(fs) // g, axis=0)
    if y.shape[1] == 1:
        y = np.repeat(y, 2, axis=1)
    return y[:, :2]


def align(x, y):
    """Décalage de y par rapport à x (clics de synchronisation, puis affinage sur la courbe statique)."""
    clics = segs('clic')
    end = clics[-1]['start'] + clics[-1]['len']
    a, b = x[:end, 0], y[:end + int(3 * FS), 0]
    N = 1 << int(np.ceil(np.log2(len(a) + len(b))))
    c = np.fft.irfft(np.fft.rfft(b, N) * np.conj(np.fft.rfft(a, N)), N)
    lag = int(np.argmax(np.abs(c[:len(b)])))
    return lag


def segs(name):
    return [s for s in SEG['segments'] if s['name'] == name]


def gc_model(L, T, R, W):
    over = L - T
    s = 1 / R - 1
    out = np.where(2 * over < -W, 0.0, np.where(np.abs(2 * over) <= W, s * (over + W / 2) ** 2 / (2 * max(W, 1e-6)), s * over))
    return out


def analyse(path):
    x = load(REF)
    y = load(path)
    lag = align(x, y)
    y = y[lag:]
    if len(y) < len(x):
        y = np.pad(y, ((0, len(x) - len(y)), (0, 0)))
    y = y[:len(x)]
    res = {'fichier': os.path.basename(path), 'decalage_ech': lag}

    # --- courbe statique ---
    L, G = [], []
    for s in segs('statique'):
        a, n = s['start'], s['len']
        sl = slice(a + int(n * 0.55), a + int(n * 0.95))
        L.append(rms_db(x[sl, 0])); G.append(rms_db(y[sl, 0]) - rms_db(x[sl, 0]))
    L, G = np.array(L), np.array(G)
    makeup = np.median(G[:3])                       # niveaux les plus bas : pas de compression
    GR = G - makeup
    fit = least_squares(lambda p: gc_model(L, p[0], p[1], p[2]) - GR, [-20, 4, 6],
                        bounds=([-60, 1.01, 0], [0, 100, 40]))
    T, R, W = fit.x
    res['statique'] = {'entree_db': L.round(2).tolist(), 'reduction_db': GR.round(2).tolist(),
                       'gain_bas_niveau_db': round(float(makeup), 2),
                       'seuil_db': round(float(T), 1), 'ratio': round(float(R), 2), 'knee_db': round(float(W), 1)}

    # --- temps ---
    att, rel, curves = [], [], []
    for s in segs('temps'):
        a, n = s['start'], s['len']
        xs, ys = x[a:a + n, 0], y[a:a + n, 0]
        ex = np.abs(hilbert(xs)); ey = np.abs(hilbert(ys))
        k = int(0.0005 * FS)
        ker = np.ones(k) / k
        ex = np.convolve(ex, ker, 'same'); ey = np.convolve(ey, ker, 'same')
        g = 20 * np.log10((ey + 1e-12) / (ex + 1e-12)) - makeup
        up, down = s['step_up'], s['step_down']
        g_hi = np.median(g[down - int(0.2 * FS):down - int(0.02 * FS)])
        g_lo = np.median(g[up - int(0.3 * FS):up - int(0.05 * FS)])
        depth = g_hi - g_lo
        seg_a = g[up:down]
        seg_r = g[down:]
        ta = np.argmax(seg_a <= g_lo + 0.632 * depth) / FS * 1000
        tr = np.argmax(seg_r >= g_hi - 0.632 * depth) / FS * 1000
        att.append(ta); rel.append(tr)
        curves.append(g[up - int(0.1 * FS):].tolist()[::48])
    res['temps'] = {'attaque_ms_63pc': round(float(np.median(att)), 2), 'relachement_ms_63pc': round(float(np.median(rel)), 1),
                    'profondeur_db': round(float(depth), 1), 'courbe_db_1ms': curves[0]}

    # --- fréquence ---
    F, GF = [], []
    for s in segs('frequence'):
        a, n = s['start'], s['len']
        sl = slice(a + int(n * 0.5), a + int(n * 0.95))
        F.append(s['freq']); GF.append(rms_db(y[sl, 0]) - rms_db(x[sl, 0]) - makeup)
    res['frequence'] = {'hz': F, 'reduction_db': np.round(GF, 2).tolist()}

    # --- harmoniques ---
    H = []
    for s in segs('harmoniques'):
        a, n = s['start'], s['len']
        sl = slice(a + int(n * 0.5), a + int(n * 0.5) + FS // 2)    # 0,5 s -> résolution 2 Hz
        seg = y[sl, 0] * np.blackman(FS // 2)
        sp = np.abs(np.fft.rfft(seg))
        f0 = s['freq']
        def amp(f):
            i = int(round(f * (FS // 2) / FS))
            return sp[max(i - 3, 0):i + 4].max()
        a1 = amp(f0)
        hs = [20 * np.log10(amp(f0 * k) / a1 + 1e-12) for k in range(2, 8) if f0 * k < FS / 2 - 100]
        thd = 100 * np.sqrt(sum(10 ** (h / 10) for h in hs))
        H.append({'freq': f0, 'niveau': s['level'], 'thd_pc': round(float(thd), 3),
                  'h2_db': round(float(hs[0]), 1), 'h3_db': round(float(hs[1]), 1)})
    res['harmoniques'] = H

    # --- stéréo ---
    s = segs('stereo')[0]
    a = s['start']; b0, b1 = s['burst']
    pre = slice(a + int(0.1 * FS), a + b0 - int(0.05 * FS))
    mid = slice(a + b0 + int(0.5 * FS), a + b1 - int(0.02 * FS))
    grL = (rms_db(y[mid, 0]) - rms_db(x[mid, 0])) - (rms_db(y[pre, 0]) - rms_db(x[pre, 0]))
    grR = (rms_db(y[mid, 1]) - rms_db(x[mid, 1])) - (rms_db(y[pre, 1]) - rms_db(x[pre, 1]))
    res['stereo'] = {'reduction_gauche_db': round(float(grL), 2), 'reduction_droite_db': round(float(grR), 2),
                     'couplage_pc': round(float(100 * grR / grL), 0) if grL < -0.5 else None}

    # --- bruit rose : réponse en fréquence (tiers d'octave, normalisée à 1 kHz) ---
    s = segs('rose')[0]
    sl = slice(s['start'] + FS, s['start'] + s['len'] - FS // 2)
    nfft = 8192
    def psd(z):
        z = z[:len(z) // nfft * nfft].reshape(-1, nfft) * np.hanning(nfft)
        return (np.abs(np.fft.rfft(z, axis=1)) ** 2).mean(axis=0)
    f = np.fft.rfftfreq(nfft, 1 / FS)
    tf = 10 * np.log10(psd(y[sl, 0]) / psd(x[sl, 0]))
    centres = 1000 * 2 ** (np.arange(-15, 14) / 3)
    band = []
    for c in centres:
        m = (f >= c / 2 ** (1 / 6)) & (f < c * 2 ** (1 / 6))
        band.append(float(np.mean(tf[m])) if m.any() else np.nan)
    band = np.array(band); band -= band[15]
    res['couleur'] = {'hz': centres.round(0).tolist(), 'db': np.round(band, 2).tolist()}

    # --- batterie : facteur de crête ---
    s = segs('batterie')[0]
    sl = slice(s['start'], s['start'] + s['len'])
    crest = lambda z: 20 * np.log10(np.max(np.abs(z)) / (np.sqrt(np.mean(z ** 2)) + 1e-12) + 1e-12)
    res['batterie'] = {'crete_entree_db': round(float(crest(x[sl, 0])), 2), 'crete_sortie_db': round(float(crest(y[sl, 0])), 2)}
    return res


def report(results):
    lines = []
    for r in results:
        st, tm = r['statique'], r['temps']
        lines.append(f"=== {r['label']} ({r['fichier']}) ===")
        lines.append(f"  Courbe : seuil {st['seuil_db']} dB, ratio {st['ratio']}:1, knee {st['knee_db']} dB "
                     f"(gain à bas niveau {st['gain_bas_niveau_db']:+} dB)")
        lines.append(f"  Temps (63 %) : attaque {tm['attaque_ms_63pc']} ms, relâchement {tm['relachement_ms_63pc']} ms "
                     f"pour {tm['profondeur_db']} dB de réduction")
        lines.append('  Réduction selon la fréquence : ' + ', '.join(f"{h} Hz {g:+.1f}" for h, g in zip(r['frequence']['hz'], r['frequence']['reduction_db'])))
        lines.append('  Harmoniques : ' + ' ; '.join(f"{h['freq']} Hz à {h['niveau']} dB : THD {h['thd_pc']} % (H2 {h['h2_db']} dB, H3 {h['h3_db']} dB)" for h in r['harmoniques']))
        s = r['stereo']
        lines.append(f"  Stéréo : gauche {s['reduction_gauche_db']} dB, droite {s['reduction_droite_db']} dB (couplage {s['couplage_pc']} %)")
        c = r['couleur']
        pick = [(h, d) for h, d in zip(c['hz'], c['db']) if h in (50, 100, 250, 4000, 8000, 16000)]
        lines.append('  Couleur (bruit rose, réf. 1 kHz) : ' + ', '.join(f"{int(h)} Hz {d:+.2f}" for h, d in pick))
        b = r['batterie']
        lines.append(f"  Batterie : facteur de crête {b['crete_entree_db']} -> {b['crete_sortie_db']} dB")
        lines.append('')
    return '\n'.join(lines)


def plot(results, out_png):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(2, 3, figsize=(16, 9))
    for r in results:
        lab = r['label']
        st = r['statique']
        ax[0, 0].plot(st['entree_db'], st['reduction_db'], 'o-', label=lab, ms=3)
        g = np.array(r['temps']['courbe_db_1ms'])
        ax[0, 1].plot(np.arange(len(g)) - 100, g, label=lab)
        ax[0, 2].semilogx(r['frequence']['hz'], r['frequence']['reduction_db'], 'o-', label=lab, ms=3)
        c = r['couleur']
        ax[1, 0].semilogx(c['hz'], c['db'], label=lab)
        hs = r['harmoniques']
        ax[1, 1].bar([f"{h['freq']}Hz\n{h['niveau']}dB" for h in hs], [h['thd_pc'] for h in hs], alpha=0.5, label=lab)
        ax[1, 2].bar(lab, r['stereo']['couplage_pc'] or 0, alpha=0.7)
    titles = ['Réduction de gain (dB) selon le niveau d\'entrée (dB RMS)', 'Attaque puis relâchement (ms)',
              'Réduction selon la fréquence (dB)', 'Couleur sur bruit rose (dB, réf. 1 kHz)', 'THD (%)', 'Couplage stéréo (%)']
    for a, t in zip(ax.flat, titles):
        a.set_title(t, fontsize=10); a.grid(alpha=0.3)
    for a in [ax[0, 0], ax[0, 1], ax[0, 2], ax[1, 0], ax[1, 1]]:
        a.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(out_png, dpi=110)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='rapport')
    ap.add_argument('files', nargs='+', help='fichier.wav="Nom"')
    a = ap.parse_args()
    results = []
    for f in a.files:
        path, _, label = f.partition('=')
        r = analyse(path)
        r['label'] = label or os.path.basename(path)
        results.append(r)
    txt = report(results)
    print(txt)
    open(a.out + '.txt', 'w').write(txt)
    json.dump(results, open(a.out + '.json', 'w'), indent=1)
    plot(results, a.out + '.png')
