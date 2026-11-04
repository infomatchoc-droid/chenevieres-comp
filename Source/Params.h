// Chènevières Comp — paramètres
//
// Les paramètres « partagés » sont les mêmes pour tous les modèles : c'est ce qui permet de
// passer d'un compresseur à l'autre en gardant les réglages (et l'automation du DAW).
// Chaque façade les affiche à sa façon (ex. Peak Reduction de l'Opto-2A = seuil).
// Les paramètres « propres » n'existent que sur un modèle : ils restent mémorisés quand on le quitte.
#pragma once
#include <array>
#include <cstddef>
#include <cmath>

namespace comp
{
enum P : int
{
    // partagés
    pModel = 0, pThreshold, pRatio, pAttack, pRelease, pAutoRelease, pKnee,
    pInput, pOutput, pMix, pScHpf, pLink, pLimit, pAutoGain, pScExt, pBypass,
    // propres à un modèle
    pPunch,       // A-2500 (0 off, 1 low, 2 med, 3 high)
    pFeedback,    // A-2500 : 0 feed forward, 1 feed back
    pFetAll,      // FET-76 : tous les ratios enfoncés
    pTimeConst,   // Fair-670, Vari-Mu V : 1..6
    pPressorMode, // Pressor-X : clean, classic, brit, tape, nuke
    pDetector,    // Pressor-X (peak, rms, opto), Pro-CX (peak, rms)
    pDist2, pDist3, // Pressor-X
    pHfRolloff,   // Opto-3A
    pShadowTube,  // Shadow Master : 1 tube, 0 clean
    pZenerDrive,  // Shadow Master : 0..10
    pThd,         // Zener-X : 0..10
    pZenerDual,   // Zener-X : mode DUAL
    pProMode,     // Pro-CX : clean, vintage, bus, punch
    pVoxMode,     // RC-Vox : vox, clean, smooth, punch
    pGate,        // RC-Vox : seuil de gate (-80 = coupé)
    pAir,         // RC-Vox : 0..100
    pDeEss,       // RC-Vox
    pDeEssFreq,   // RC-Vox : 2..10 kHz
    numParams
};

enum class Kind { Float, Bool, Choice, Int };

struct ParamSpec
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    float centre;          // valeur au milieu de la course (0 = linéaire)
    const char* choices;   // séparées par '|'
    const char* unit;
};

inline const ParamSpec& spec (int p)
{
    static const std::array<ParamSpec, numParams> t {{
        { "model",      "Modèle",       Kind::Choice, 0, 15, 5, 0,
          "FET-76|Opto-2A|Opto-3A|Tube-One B|Bus-Comp G|A-2500|DB-160X|Pressor-X|Fair-670|Vari-Mu V|Shadow Master|N-3309|Zener-X|Pro-CX|RC-Ren|RC-Vox", "" },
        { "threshold",  "Threshold",    Kind::Float, -60.0f, 0.0f, -18.0f, 0, nullptr, "dB" },
        { "ratio",      "Ratio",        Kind::Float,  1.0f, 40.0f, 4.0f, 4.0f, nullptr, ":1" },
        { "attack",     "Attack",       Kind::Float,  0.02f, 300.0f, 10.0f, 3.0f, nullptr, "ms" },
        { "release",    "Release",      Kind::Float,  5.0f, 5000.0f, 200.0f, 250.0f, nullptr, "ms" },
        { "autorel",    "Auto release", Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "knee",       "Knee",         Kind::Float,  0.0f, 24.0f, 6.0f, 0, nullptr, "dB" },
        { "input",      "Input",        Kind::Float, -20.0f, 20.0f, 0.0f, 0, nullptr, "dB" },
        { "output",     "Output",       Kind::Float, -40.0f, 40.0f, 0.0f, 0, nullptr, "dB" },
        { "mix",        "Mix",          Kind::Float,  0.0f, 100.0f, 100.0f, 0, nullptr, "%" },
        { "schpf",      "Sidechain HPF",Kind::Float,  0.0f, 1000.0f, 0.0f, 120.0f, nullptr, "Hz" },
        { "link",       "Link",         Kind::Float,  0.0f, 100.0f, 100.0f, 0, nullptr, "%" },
        { "limit",      "Limit",        Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "autogain",   "Auto gain",    Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "scext",      "Sidechain ext",Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "bypass",     "Bypass",       Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "punch",      "Punch",        Kind::Choice, 0, 3, 0, 0, "Off|Low|Med|High", "" },
        { "feedback",   "Feed back",    Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "fetall",     "All buttons",  Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "timeconst",  "Time constant",Kind::Int,    1, 6, 2, 0, nullptr, "" },
        { "pressmode",  "Pressor mode", Kind::Choice, 0, 4, 1, 0, "Clean|Classic|Brit|Tape|Nuke", "" },
        { "detector",   "Detector",     Kind::Choice, 0, 2, 0, 0, "Peak|RMS|Opto", "" },
        { "dist2",      "Dist 2",       Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "dist3",      "Dist 3",       Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "hfroll",     "HF rolloff",   Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "shtube",     "Tube",         Kind::Bool,   0, 1, 1, 0, nullptr, "" },
        { "zdrive",     "Zener drive",  Kind::Float,  0.0f, 10.0f, 3.0f, 0, nullptr, "" },
        { "thd",        "THD drive",    Kind::Float,  0.0f, 10.0f, 2.0f, 0, nullptr, "" },
        { "zdual",      "Dual",         Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "promode",    "Pro mode",     Kind::Choice, 0, 3, 0, 0, "Clean|Vintage|Bus|Punch", "" },
        { "voxmode",    "Vox mode",     Kind::Choice, 0, 3, 0, 0, "Vox|Clean|Smooth|Punch", "" },
        { "gate",       "Gate",         Kind::Float, -80.0f, 0.0f, -80.0f, 0, nullptr, "dB" },
        { "air",        "Air",          Kind::Float,  0.0f, 100.0f, 0.0f, 0, nullptr, "%" },
        { "deess",      "De-esser",     Kind::Bool,   0, 1, 0, 0, nullptr, "" },
        { "deessfreq",  "De-esser freq",Kind::Float,  2000.0f, 10000.0f, 6000.0f, 5000.0f, nullptr, "Hz" },
    }};
    return t[(std::size_t) p];
}

// Valeurs brutes de tous les paramètres (unités réelles)
struct Values
{
    std::array<float, numParams> v {};
    Values() { for (int i = 0; i < numParams; ++i) v[(std::size_t) i] = spec (i).def; }
    float& operator[] (int p) { return v[(std::size_t) p]; }
    float operator[] (int p) const { return v[(std::size_t) p]; }
    bool on (int p) const { return v[(std::size_t) p] >= 0.5f; }
    int choice (int p) const { return (int) std::lround (v[(std::size_t) p]); }
};
} // namespace comp
