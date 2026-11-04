# Chènevières Comp — notes de développement

## Principe

- **16 modèles dans un seul plugin.** Le choix du modèle est un paramètre (`model`), automatisable.
- **Réglages partagés** (`Source/Params.h`) : seuil, ratio, attaque, relâchement, knee, gains, mix, HPF sidechain,
  link, comp/limit… Ils ne changent **jamais** quand on change de modèle. Chaque modèle les « lit » à sa façon dans
  `Source/Interpret.cpp` (bornes, crans, temps fixes). En revenant sur un modèle, on retrouve ses réglages à l'identique,
  et l'automation du DAW continue de marcher.
- **Réglages propres** (Punch, Feed back, Time constant, Zener drive…) : mémorisés par modèle.
- Seul cas où un changement de modèle écrit un paramètre : en arrivant sur Fair-670 / Vari-Mu V, le TIME CONSTANT est
  déduit du relâchement (`onModelChanged`). Tourner le TIME CONSTANT écrit attaque et relâchement partagés.
- Façades : les commandes « Input » des FET-76, Pressor-X et Zener-X, la Peak Reduction des Opto, etc. seront liées au
  seuil partagé par une correspondance `map` (affichée = a × paramètre + b) dans le fichier de façade.

## Moteur (`Source/CompEngine.cpp`, C++ pur)

Suréchantillonnage ×2 (JUCE, FIR, latence annoncée au DAW), puis par échantillon : gain d'entrée, sidechain (interne,
retour de sortie en feed back, ou externe), filtres du sidechain (HPF, pente Punch), détection crête / RMS / optique /
vari-mu, courbe (seuil, ratio, knee), temps (relâchement auto ou dépendant du programme, cellule optique à deux temps
avec mémoire), second étage (Shadow Master, Zener-X Dual), saturation propre au modèle, de-esser / air / gate (RC-Vox),
mix parallèle, gain de sortie. Bypass = mix 0 avec les mêmes lissages (pas de clic, latence conservée).

Tests : `ctest` (EngineTest) — pas de NaN, compression présente sur les 16 modèles, courbe statique du A-2500,
passage des réglages entre modèles, mix 0 %.

## Façades (`tools/facades/`)

1. `design/facades-originales/<modèle>.png` : visuel d'origine (2172 × 724).
2. `tools/facades/layouts/<modèle>.json` : position des boutons rotatifs (centre, rayon, calibration valeur → angle),
   boutons lumineux (rectangles, valeurs), voyants, vu-mètres, textes à refaire, en-tête (logo retiré, titre recalé).
3. `python3 build_assets.py ../../Resources/panels ../../Source/layouts <modèle>` : façade de base (boutons éteints,
   sans pointeurs ni aiguilles), capuchons sans pointeur, boutons allumés avec halo, voyants.
4. `python3 gen_layouts.py` : `Source/Layouts.inc`.
Tant qu'un modèle n'a pas son fichier de façade, le plugin affiche une façade générique (6 boutons) avec le vrai son.

État : A-2500 fait. Les 15 autres à préparer.

## Publier

Comme l'EQ : la version est dans `CMakeLists.txt`, les installeurs sont dans les artefacts du build GitHub
(Windows `.exe`, Mac `.pkg`, kits de signature AAX).
