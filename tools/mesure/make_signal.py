"""Fichier de test pour mesurer un compresseur (référence matérielle ou plugin).

Produit Chenevieres-Comp-test.wav (48 kHz, 24 bits, stéréo) et segments.json (positions des segments).
"""
import json, os
import numpy as np
import soundfile as sf

FS = 48000
HERE = os.path.dirname(os.path.abspath(__file__))


def db(x):
    return 10 ** (x / 20)


def sine(f, dur, level_db_rms):
    t = np.arange(int(dur * FS)) / FS
    return np.sqrt(2) * db(level_db_rms) * np.sin(2 * np.pi * f * t)


def fade(x, ms=5):
    n = int(ms * FS / 1000)
    w = np.ones(len(x)); r = np.linspace(0, 1, n)
    w[:n] = r; w[-n:] = r[::-1]
    return x * w


def build():
    L, R, segs = [], [], []
    t = [0]

    def add(name, l, r=None, **meta):
        r = l if r is None else r
        segs.append({'name': name, 'start': t[0], 'len': len(l), **meta})
        L.append(l); R.append(r); t[0] += len(l)

    def silence(d):
        z = np.zeros(int(d * FS)); L.append(z); R.append(z); t[0] += len(z)

    # 1) synchronisation : 3 clics
    silence(0.5)
    for _ in range(3):
        c = np.zeros(int(0.25 * FS)); c[0] = 0.5
        add('clic', c)
    silence(1.0)

    # 2) courbe statique : 1 kHz de -40 à -4 dB RMS (crête -1 dBFS), par pas de 2 dB, 1,2 s chacun (régime établi)
    for lv in range(-40, -3, 2):
        add('statique', fade(sine(1000, 1.2, lv)), level=lv)
    silence(2.0)

    # 3) attaque / relâchement : -40 dB -> -6 dB (1,5 s) -> -40 dB (3 s), deux fois
    for k in range(2):
        a = sine(1000, 1.0, -40); b = sine(1000, 1.5, -6); c = sine(1000, 3.0, -40)
        # continuité de phase : un seul sinus, enveloppe en marches
        n = len(a) + len(b) + len(c)
        tt = np.arange(n) / FS
        env = np.concatenate([np.full(len(a), db(-40)), np.full(len(b), db(-6)), np.full(len(c), db(-40))])
        add('temps', fade(np.sqrt(2) * env * np.sin(2 * np.pi * 1000 * tt)), step_up=len(a), step_down=len(a) + len(b))
    silence(1.0)

    # 4) détection selon la fréquence (filtres du sidechain, Thrust) : -10 dB RMS
    for f in [40, 60, 100, 150, 250, 500, 1000, 2000, 5000, 10000]:
        add('frequence', fade(sine(f, 1.0, -10)), freq=f)
    silence(1.0)

    # 5) harmoniques : 100 Hz et 1 kHz, à -18 et -6 dB RMS
    for f in [100, 1000]:
        for lv in [-18, -6]:
            add('harmoniques', fade(sine(f, 2.0, lv)), freq=f, level=lv)
    silence(1.0)

    # 6) couplage stéréo : salve forte à gauche seulement, sinus faible continu à droite
    n = int(2.5 * FS)
    tt = np.arange(n) / FS
    left = np.where((tt > 0.5) & (tt < 1.5), db(-6), db(-30)) * np.sqrt(2) * np.sin(2 * np.pi * 1000 * tt)
    right = db(-30) * np.sqrt(2) * np.sin(2 * np.pi * 700 * tt)
    add('stereo', fade(left), fade(right), burst=[int(0.5 * FS), int(1.5 * FS)])
    silence(1.0)

    # 7) bruit rose -20 dB RMS (couleur / réponse en fréquence en compression)
    rng = np.random.default_rng(7)
    w = rng.standard_normal(int(6 * FS))
    W = np.fft.rfft(w); f = np.fft.rfftfreq(len(w), 1 / FS); W[1:] /= np.sqrt(f[1:]); W[0] = 0
    p = np.fft.irfft(W, len(w)); p = p / np.sqrt(np.mean(p ** 2)) * db(-20)
    add('rose', fade(p, 20))
    silence(1.0)

    # 8) motif de batterie synthétique (écoute, comparaison)
    bar = np.zeros(int(2.0 * FS))
    def kick(at):
        k = int(0.35 * FS); tt = np.arange(k) / FS
        s = np.sin(2 * np.pi * (45 + 90 * np.exp(-tt * 25)) * tt) * np.exp(-tt * 9) * 0.8
        bar[at:at + k] += s[:len(bar) - at]
    def snare(at):
        k = int(0.25 * FS); tt = np.arange(k) / FS
        s = (rng.standard_normal(k) * 0.45 + 0.4 * np.sin(2 * np.pi * 190 * tt)) * np.exp(-tt * 18)
        bar[at:at + k] += s[:len(bar) - at]
    def hat(at):
        k = int(0.05 * FS)
        s = np.diff(rng.standard_normal(k + 1)) * 0.12 * np.exp(-np.arange(k) / FS * 80)
        bar[at:at + k] += s[:len(bar) - at]
    beat = int(0.5 * FS)
    for i in range(4):
        hat(i * beat); hat(i * beat + beat // 2)
        (kick if i % 2 == 0 else snare)(i * beat)
    drums = np.tile(bar, 3)
    drums *= 0.7 / np.max(np.abs(drums))
    add('batterie', drums)
    silence(1.0)

    x = np.stack([np.concatenate(L), np.concatenate(R)], axis=1)
    assert np.max(np.abs(x)) < 1.0
    return x, segs


if __name__ == '__main__':
    x, segs = build()
    out = os.path.join(HERE, 'Chenevieres-Comp-test.wav')
    sf.write(out, x, FS, subtype='PCM_24')
    json.dump({'fs': FS, 'segments': segs}, open(os.path.join(HERE, 'segments.json'), 'w'), indent=1)
    print(out, f'{len(x) / FS:.1f} s')
