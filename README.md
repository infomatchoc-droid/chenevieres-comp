# Chènevières Comp

Compresseur multi-modèles du Studio des Chènevières : 16 modèles dans un seul plugin
(VST3 / AU / AAX), on passe de l'un à l'autre en gardant les réglages autant que possible.

| Modèle | Famille |
|---|---|
| FET-76 | FET |
| Opto-2A, Opto-3A | Optique |
| Tube-One B | Optique à lampes |
| Bus-Comp G, A-2500, DB-160X, Pressor-X | VCA |
| Fair-670, Vari-Mu V | Vari-Mu |
| Shadow Master | Hybride |
| N-3309 | Pont de diodes |
| Zener-X | Zener |
| Pro-CX, RC-Ren | Numérique |
| RC-Vox | Voix |

## Façades

`design/facades-originales/` : visuels d'origine. `tools/facades/` : nettoyage (pointeurs, aiguilles,
graduations fausses) et aperçus. Les boutons, aiguilles et graduations sont redessinés par le plugin.

```
cd tools/facades
python3 clean.py a2500        # -> out/a2500/panel.png + knob_*.png
python3 compose.py a2500 apercu.png '{"threshold":-18,"ratio":4,...}'
```
