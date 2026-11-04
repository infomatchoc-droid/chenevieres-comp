// Chènevières Comp — les 16 modèles
#pragma once
#include <array>
#include <cstddef>

namespace comp
{
enum class Model : int
{
    FET76 = 0, Opto2A, Opto3A, TubeOneB,
    BusCompG, A2500, DB160X, PressorX,
    Fair670, VariMuV, ShadowMaster, N3309,
    ZenerX, ProCX, RCRen, RCVox,
    count
};

constexpr int numModels = (int) Model::count;

struct ModelInfo
{
    const char* id;        // identifiant stable (fichiers, presets)
    const char* name;      // nom affiché
    const char* family;    // famille (menu)
    const char* subtitle;  // sous-titre court de la barre du haut
};

inline const ModelInfo& info (Model m)
{
    static const std::array<ModelInfo, numModels> table {{
        { "fet76",        "FET-76",        "FET",             "FET · limiteur" },
        { "opto2a",       "Opto-2A",       "OPTIQUE",         "Optique · lampes" },
        { "opto3a",       "Opto-3A",       "OPTIQUE",         "Optique · transistors" },
        { "tubeoneb",     "Tube-One B",    "OPTIQUE À LAMPES","Optique · lampes" },
        { "buscompg",     "Bus-Comp G",    "VCA",             "VCA · bus" },
        { "a2500",        "A-2500",        "VCA",             "VCA · stéréo" },
        { "db160x",       "DB-160X",       "VCA",             "VCA · RMS" },
        { "pressorx",     "Pressor-X",     "VCA",             "VCA · couleurs" },
        { "fair670",      "Fair-670",      "VARI-MU",         "Vari-mu · lampes" },
        { "varimuv",      "Vari-Mu V",     "VARI-MU",         "Vari-mu · lampes" },
        { "shadowmaster", "Shadow Master", "HYBRIDE",         "Optique + VCA" },
        { "n3309",        "N-3309",        "PONT DE DIODES",  "Pont de diodes" },
        { "zenerx",       "Zener-X",       "ZENER",           "Zener · limiteur" },
        { "procx",        "Pro-CX",        "NUMÉRIQUE",       "Multi-mode" },
        { "rcren",        "RC-Ren",        "NUMÉRIQUE",       "Numérique · doux" },
        { "rcvox",        "RC-Vox",        "VOIX",            "Voix" },
    }};
    return table[(std::size_t) m];
}
} // namespace comp
