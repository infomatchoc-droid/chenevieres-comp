// Chènevières Comp — ce que chaque modèle fait des réglages partagés
//
// Les paramètres partagés ne sont jamais modifiés quand on change de modèle : chaque modèle les
// « lit » à sa façon (bornes, crans, temps fixes…). En revenant à un modèle, on retrouve donc
// exactement ses réglages. L'interface affiche les valeurs lues (pointeurs, boutons allumés).
#pragma once
#include "Models.h"
#include "Params.h"

namespace comp
{
enum class Detector { Peak, Rms, Opto, VariMu };
enum class Sat { None, Transformer, Console, Tube, Fet, Diode, Zener, Tape };

struct Eff
{
    // topologie et détection
    bool feedback = false;
    Detector detector = Detector::Peak;
    float rmsMs = 0.0f;           // fenêtre RMS (Detector::Rms)
    // courbe
    float thresholdDb = -18.0f;
    float ratio = 4.0f;           // >= 1 ; 1000 = infini
    float kneeDb = 6.0f;
    float varimuK = 0.25f;        // pente progressive (Detector::VariMu)
    // temps
    float attackMs = 10.0f;
    float releaseMs = 200.0f;
    bool autoRelease = false;     // relâchement dépendant du programme
    float releaseSlowMs = 1200.0f;// relâchement lent (auto, opto, vari-mu)
    float overshoot = 0.0f;       // FET « all buttons » : dépassement à l'attaque
    // gains
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    bool autoGain = false;
    float mix = 1.0f;
    // sidechain
    float scHpfHz = 0.0f;
    int punch = 0;                // filtre de pente du sidechain (A-2500)
    float link = 1.0f;
    bool scExternal = false;
    // couleur
    Sat sat = Sat::None;
    float satDrive = 0.0f;        // 0..1
    float satGrMod = 0.0f;        // la saturation augmente avec la réduction de gain
    float dist2 = 0.0f, dist3 = 0.0f; // harmoniques paires / impaires ajoutées
    float outLpfHz = 0.0f;        // Opto-3A HF rolloff
    // second étage (Zener-X DUAL, Shadow Master : optique puis VCA)
    bool secondStage = false;
    float stage2Ratio = 4.0f, stage2Offset = 0.0f, stage2AttackMs = 1.0f, stage2ReleaseMs = 100.0f;
    // RC-Vox
    float gateDb = -80.0f;
    bool deEss = false;
    float deEssHz = 6000.0f;
    float airDb = 0.0f;
};

// Temps des positions TIME CONSTANT (Fair-670, Vari-Mu V) : attaque, relâchement, lent (programme)
struct TimeConst { float attackMs, releaseMs, slowMs; bool program; };
inline TimeConst timeConst (int tc)
{
    static const TimeConst t[6] = {
        { 0.2f,  300.0f,   300.0f, false },
        { 0.2f,  800.0f,   800.0f, false },
        { 0.4f, 2000.0f,  2000.0f, false },
        { 0.4f, 5000.0f,  5000.0f, false },
        { 0.4f, 2000.0f, 10000.0f, true },
        { 0.2f,  300.0f, 25000.0f, true },
    };
    return t[(tc < 1 ? 1 : tc > 6 ? 6 : tc) - 1];
}

// Position TIME CONSTANT la plus proche d'un relâchement (en arrivant sur Fair-670 / Vari-Mu V)
int nearestTimeConst (float releaseMs, bool autoRelease);

// Valeur la plus proche dans une liste (crans)
float snap (float v, const float* list, int n, bool logScale);

Eff interpret (Model m, const Values& v);

// À appeler quand on change de modèle : ne touche qu'aux paramètres propres qui dépendent des
// réglages partagés (ex. TIME CONSTANT déduit du relâchement)
void onModelChanged (Values& v, Model from, Model to);

// Écriture d'un TIME CONSTANT depuis la façade : met aussi à jour attaque et relâchement partagés
void applyTimeConst (Values& v, int tc);
} // namespace comp
