#include "Presets.h"

namespace comp
{
const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list {
        { "Réglage d'usine", {} },
        { "Voix — douce",            { { pThreshold, -22 }, { pRatio, 3 },  { pAttack, 8 },   { pRelease, 180 }, { pKnee, 12 }, { pScHpf, 80 }, { pOutput, 4 } } },
        { "Voix — en avant",         { { pThreshold, -26 }, { pRatio, 6 },  { pAttack, 2 },   { pRelease, 90 },  { pKnee, 6 },  { pScHpf, 100 }, { pOutput, 7 } } },
        { "Batterie — punch",        { { pThreshold, -20 }, { pRatio, 4 },  { pAttack, 25 },  { pRelease, 120 }, { pKnee, 0 },  { pOutput, 3 } } },
        { "Batterie — parallèle",    { { pThreshold, -32 }, { pRatio, 10 }, { pAttack, 0.3f },{ pRelease, 80 },  { pKnee, 0 },  { pMix, 40 }, { pOutput, 4 } } },
        { "Basse — serrée",          { { pThreshold, -24 }, { pRatio, 5 },  { pAttack, 4 },   { pRelease, 150 }, { pKnee, 6 },  { pOutput, 5 } } },
        { "Guitare acoustique",      { { pThreshold, -20 }, { pRatio, 3 },  { pAttack, 12 },  { pRelease, 250 }, { pKnee, 12 }, { pScHpf, 120 }, { pOutput, 3 } } },
        { "Piano",                   { { pThreshold, -18 }, { pRatio, 2 },  { pAttack, 20 },  { pRelease, 400 }, { pKnee, 12 }, { pOutput, 2 } } },
        { "Bus mix — colle",         { { pThreshold, -14 }, { pRatio, 2 },  { pAttack, 30 },  { pRelease, 300 }, { pAutoRelease, 1 }, { pKnee, 6 }, { pScHpf, 60 }, { pOutput, 1.5f } } },
        { "Mastering — léger",       { { pThreshold, -10 }, { pRatio, 1.5f },{ pAttack, 30 }, { pRelease, 600 }, { pAutoRelease, 1 }, { pKnee, 12 }, { pScHpf, 40 }, { pOutput, 1 } } },
        { "Limiteur de crêtes",      { { pThreshold, -6 },  { pRatio, 20 }, { pAttack, 0.05f },{ pRelease, 60 }, { pKnee, 0 },  { pLimit, 1 }, { pOutput, 3 } } },
    };
    return list;
}

Values applyPreset (const Values& current, const Preset& p)
{
    Values v = current;
    const Values d;
    for (int i = pThreshold; i <= pScExt; ++i) v[i] = d[i];
    for (auto& [id, val] : p.values) v[id] = val;
    return v;
}
} // namespace comp
