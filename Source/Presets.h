// Chènevières Comp — presets d'usine
// Ils ne règlent que les paramètres partagés : ils marchent donc sur tous les modèles.
#pragma once
#include "Params.h"
#include <vector>
#include <utility>

namespace comp
{
struct Preset
{
    const char* name;
    std::vector<std::pair<int, float>> values;   // les autres paramètres partagés reviennent au défaut
};

const std::vector<Preset>& factoryPresets();

// Applique un preset : paramètres partagés au défaut puis valeurs du preset (le modèle ne change pas)
Values applyPreset (const Values& current, const Preset& p);
} // namespace comp
