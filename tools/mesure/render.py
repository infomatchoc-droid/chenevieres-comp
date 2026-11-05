"""Passe le fichier de test dans le moteur de Chènevières Comp (suréchantillonné x2, comme le plugin).
   python3 render.py sortie.wav a2500 threshold=-20 ratio=4 attack=1 release=300 knee=0
"""
import os, subprocess, sys, tempfile
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.environ.get('COMP_RENDER', os.path.join(HERE, '..', '..', 'build', 'CompRender'))


def render(out_wav, model, params, src=os.path.join(HERE, 'Chenevieres-Comp-test.wav')):
    x, fs = sf.read(src, dtype='float32')
    up = resample_poly(x, 2, 1, axis=0).astype(np.float32)
    with tempfile.TemporaryDirectory() as d:
        a, b = os.path.join(d, 'in.f32'), os.path.join(d, 'out.f32')
        up.tofile(a)
        subprocess.run([BIN, a, b, str(fs * 2), model] + params, check=True)
        y = np.fromfile(b, dtype=np.float32).reshape(-1, 2)
    y = resample_poly(y, 1, 2, axis=0)
    sf.write(out_wav, y, fs, subtype='FLOAT')


if __name__ == '__main__':
    render(sys.argv[1], sys.argv[2], sys.argv[3:])
