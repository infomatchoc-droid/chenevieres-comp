// Rendu hors ligne par le moteur (sans JUCE), pour les mesures :
//   CompRender entrée.f32 sortie.f32 fréquence modèle [param=valeur ...]
// Fichiers : float32 stéréo entrelacé, brut. tools/mesure/render.py s'occupe du WAV et du suréchantillonnage.
#include "../Source/CompEngine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace comp;

int main (int argc, char** argv)
{
    if (argc < 5) { std::fprintf (stderr, "usage : CompRender in.f32 out.f32 fs modele [param=valeur...]\n"); return 2; }
    FILE* in = std::fopen (argv[1], "rb");
    if (! in) return 3;
    std::vector<float> x;
    float buf[4096];
    size_t n;
    while ((n = std::fread (buf, sizeof (float), 4096, in)) > 0) x.insert (x.end(), buf, buf + n);
    std::fclose (in);

    const double fs = std::atof (argv[3]);
    Model model = Model::A2500;
    for (int m = 0; m < numModels; ++m)
        if (std::strcmp (argv[4], info ((Model) m).id) == 0) model = (Model) m;
    Values v;
    for (int i = 5; i < argc; ++i)
    {
        const std::string a (argv[i]);
        const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        for (int p = 0; p < numParams; ++p)
            if (a.substr (0, eq) == spec (p).id) v[p] = (float) std::atof (a.c_str() + eq + 1);
    }

    Engine e;
    e.prepare (fs, 2);
    e.setParams (interpret (model, v));
    const size_t frames = x.size() / 2;
    std::vector<float> L (frames), R (frames);
    for (size_t i = 0; i < frames; ++i) { L[i] = x[2 * i]; R[i] = x[2 * i + 1]; }
    const int block = 256;
    for (size_t i = 0; i < frames; i += block)
    {
        float* ch[2] = { L.data() + i, R.data() + i };
        e.process (ch, 2, (int) std::min<size_t> (block, frames - i), nullptr, 0);
    }
    for (size_t i = 0; i < frames; ++i) { x[2 * i] = L[i]; x[2 * i + 1] = R[i]; }
    FILE* out = std::fopen (argv[2], "wb");
    if (! out) return 4;
    std::fwrite (x.data(), sizeof (float), x.size(), out);
    std::fclose (out);
    return 0;
}
