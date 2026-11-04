// Tests du moteur (sans JUCE) : ctest
#include "../Source/CompEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>

using namespace comp;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("ÉCHEC : " __VA_ARGS__); std::printf ("\n"); } } while (0)

struct Result { float grDb, outDb, thdPct; bool finite; };

static Result run (Model m, Values v, float levelDb, double fs = 96000.0, float seconds = 1.5f)
{
    Engine e;
    e.prepare (fs, 2);
    e.setParams (interpret (m, v));
    const int n = (int) (fs * seconds), block = 256;
    std::vector<float> L (n), R (n);
    const float a = std::pow (10.0f, levelDb / 20.0f) * std::sqrt (2.0f);   // niveau RMS
    for (int i = 0; i < n; ++i) L[i] = R[i] = a * (float) std::sin (2 * 3.14159265358979 * 1000.0 * i / fs);
    float gr = 0;
    for (int i = 0; i < n; i += block)
    {
        float* chs[2] = { L.data() + i, R.data() + i };
        e.process (chs, 2, std::min (block, n - i), nullptr, 0);
        gr = e.lastGr (0);
    }
    // mesure sur les 0,5 dernières secondes
    const int s0 = n - (int) (fs * 0.5);
    double sum = 0, fund = 0, fq = 0; bool fin = true;
    for (int i = s0; i < n; ++i)
    {
        if (! std::isfinite (L[i])) fin = false;
        sum += (double) L[i] * L[i];
        const double ph = 2 * 3.14159265358979 * 1000.0 * i / fs;
        fund += L[i] * std::sin (ph); fq += L[i] * std::cos (ph);
    }
    const double N = n - s0;
    const double rms = std::sqrt (sum / N);
    const double fundRms = std::sqrt ((fund * fund + fq * fq) * 2.0) / N;
    const double thd = std::sqrt (std::max (0.0, rms * rms - fundRms * fundRms)) / std::max (1e-12, fundRms) * 100.0;
    return { gr, (float) (20 * std::log10 (std::max (rms, 1e-12))), (float) thd, fin };
}

int main()
{
    // 1) Tous les modèles : pas de NaN, compression présente, niveau raisonnable
    for (int mi = 0; mi < numModels; ++mi)
    {
        Values v;
        const auto r = run ((Model) mi, v, -6.0f);
        std::printf ("%-14s GR %6.2f dB  sortie %6.2f dBFS  THD %5.2f %%\n", info ((Model) mi).name, r.grDb, r.outDb, r.thdPct);
        CHECK (r.finite, "%s : valeurs non finies", info ((Model) mi).name);
        CHECK (r.grDb < -1.0f, "%s : pas de compression (%.2f dB)", info ((Model) mi).name, r.grDb);
        CHECK (r.outDb < 0.0f && r.outDb > -30.0f, "%s : sortie %.2f dBFS", info ((Model) mi).name, r.outDb);
        CHECK (r.thdPct < 10.0f, "%s : distorsion %.2f %%", info ((Model) mi).name, r.thdPct);
    }

    // 2) A-2500, courbe statique : seuil -18, 4:1, knee dur, 1 kHz à -6 dB RMS -> 12 dB au-dessus -> -9 dB
    {
        Values v; v[pKnee] = 0; v[pAttack] = 1; v[pRelease] = 100;
        const auto r = run (Model::A2500, v, -6.0f);
        CHECK (std::abs (r.grDb + 9.0f) < 0.8f, "A-2500 : GR %.2f dB au lieu de -9", r.grDb);
    }

    // 3) Passage des réglages : les partagés ne bougent pas, TIME CONSTANT déduit du relâchement
    {
        Values v; v[pThreshold] = -24; v[pRatio] = 6; v[pRelease] = 900; v[pModel] = (float) Model::A2500;
        const Values before = v;
        onModelChanged (v, Model::A2500, Model::Fair670);
        CHECK (v.choice (pTimeConst) == 2, "TC attendu 2, obtenu %d", v.choice (pTimeConst));
        onModelChanged (v, Model::Fair670, Model::FET76);
        onModelChanged (v, Model::FET76, Model::A2500);
        for (int p = pThreshold; p <= pBypass; ++p)
            CHECK (v[p] == before[p], "paramètre %s modifié en passant d'un modèle à l'autre", spec (p).id);
        // le FET-76 lit 6:1 comme 8:1 (cran le plus proche) sans changer le réglage partagé
        CHECK (interpret (Model::FET76, v).ratio == 8.0f, "FET-76 : ratio lu %.1f", interpret (Model::FET76, v).ratio);
        // TIME CONSTANT écrit depuis la façade : met à jour attaque et relâchement partagés
        applyTimeConst (v, 4);
        CHECK (v[pRelease] == 5000.0f, "TC4 : relâchement %.0f", v[pRelease]);
    }

    // 4) Mix à 0 % = signal intact (au gain de sortie près)
    {
        Values v; v[pMix] = 0; v[pOutput] = 0;
        const auto r = run (Model::N3309, v, -6.0f);
        CHECK (std::abs (r.outDb + 6.0f) < 0.1f, "Mix 0 %% : sortie %.2f dB au lieu de -6", r.outDb);
    }

    if (failures == 0) std::printf ("Tous les tests passent.\n");
    return failures == 0 ? 0 : 1;
}
