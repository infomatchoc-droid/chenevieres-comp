#include "Interpret.h"
#include <algorithm>
#include <cmath>

namespace comp
{
static float clampf (float v, float lo, float hi) { return std::min (hi, std::max (lo, v)); }

float snap (float v, const float* list, int n, bool logScale)
{
    float best = list[0], bestD = 1e30f;
    for (int i = 0; i < n; ++i)
    {
        const float d = logScale ? std::abs (std::log (std::max (v, 1e-6f)) - std::log (std::max (list[i], 1e-6f)))
                                 : std::abs (v - list[i]);
        if (d < bestD) { bestD = d; best = list[i]; }
    }
    return best;
}

int nearestTimeConst (float releaseMs, bool autoRelease)
{
    if (autoRelease) return 5;
    int best = 1; float bestD = 1e30f;
    for (int tc = 1; tc <= 4; ++tc)
    {
        const float d = std::abs (std::log (releaseMs) - std::log (timeConst (tc).releaseMs));
        if (d < bestD) { bestD = d; best = tc; }
    }
    return best;
}

void onModelChanged (Values& v, Model from, Model to)
{
    const bool tcTo = (to == Model::Fair670 || to == Model::VariMuV);
    const bool tcFrom = (from == Model::Fair670 || from == Model::VariMuV);
    if (tcTo && ! tcFrom)
        v[pTimeConst] = (float) nearestTimeConst (v[pRelease], v.on (pAutoRelease));
}

void applyTimeConst (Values& v, int tc)
{
    const auto t = timeConst (tc);
    v[pTimeConst] = (float) tc;
    v[pAttack] = t.attackMs;
    v[pRelease] = t.releaseMs;
    v[pAutoRelease] = t.program ? 1.0f : 0.0f;
}

Eff interpret (Model m, const Values& v)
{
    Eff e;
    // valeurs communes : chaque modèle borne ou remplace ensuite
    e.thresholdDb = v[pThreshold];
    e.ratio = v[pRatio];
    e.kneeDb = v[pKnee];
    e.attackMs = v[pAttack];
    e.releaseMs = v[pRelease];
    e.autoRelease = v.on (pAutoRelease);
    e.inputDb = 0.0f;               // seuls les modèles qui ont un bouton INPUT l'utilisent
    e.outputDb = v[pOutput];
    e.mix = clampf (v[pMix] / 100.0f, 0.0f, 1.0f);
    e.scHpfHz = v[pScHpf];
    e.link = 1.0f;                  // couplé, sauf modèles avec LINK
    e.scExternal = v.on (pScExt);

    const bool limit = v.on (pLimit);

    switch (m)
    {
        case Model::FET76:
        {
            static const float r[] = { 4, 8, 12, 20 };
            e.detector = Detector::Peak;
            e.ratio = snap (e.ratio, r, 4, true);
            e.attackMs = clampf (e.attackMs, 0.02f, 0.8f);
            e.releaseMs = clampf (e.releaseMs, 50.0f, 1100.0f);
            e.autoRelease = false;
            e.kneeDb = e.ratio <= 4.0f ? 6.0f : 3.0f;
            e.sat = Sat::Fet; e.satDrive = 0.25f; e.satGrMod = 0.6f;
            if (v.on (pFetAll))
            {
                e.ratio = 30.0f; e.kneeDb = 0.0f; e.overshoot = 0.35f;
                e.satDrive = 0.55f; e.satGrMod = 1.2f;
            }
            e.scHpfHz = 0.0f;
            break;
        }
        case Model::Opto2A:
        case Model::Opto3A:
        {
            const bool la3 = (m == Model::Opto3A);
            e.feedback = true;
            e.detector = Detector::Opto;
            e.ratio = limit ? 12.0f : 3.0f;
            e.kneeDb = limit ? 6.0f : 12.0f;
            e.attackMs = la3 ? 5.0f : 10.0f;
            e.releaseMs = la3 ? 40.0f : 60.0f;
            e.releaseSlowMs = la3 ? 1200.0f : 3000.0f;
            e.autoRelease = true;
            e.sat = la3 ? Sat::Transformer : Sat::Tube;
            e.satDrive = la3 ? 0.15f : 0.2f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 200.0f);
            if (la3 && v.on (pHfRolloff)) e.outLpfHz = 12000.0f;
            break;
        }
        case Model::TubeOneB:
            e.detector = Detector::Opto;
            e.ratio = clampf (e.ratio, 1.5f, 12.0f);
            e.attackMs = clampf (e.attackMs, 1.0f, 100.0f);
            e.releaseMs = clampf (e.releaseMs, 100.0f, 2500.0f);
            e.releaseSlowMs = e.releaseMs * 3.0f;
            e.kneeDb = 8.0f;
            e.sat = Sat::Tube; e.satDrive = 0.28f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 1000.0f);
            break;

        case Model::BusCompG:
        {
            static const float r[] = { 2, 3, 4, 6, 10 };
            e.detector = Detector::Rms; e.rmsMs = 1.0f;
            e.ratio = snap (e.ratio, r, 5, true);
            static const float a[] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };
            e.attackMs = snap (e.attackMs, a, 6, true);
            static const float rel[] = { 100.0f, 300.0f, 600.0f, 1200.0f };
            e.releaseMs = snap (e.releaseMs, rel, 4, true);
            if (e.autoRelease) { e.releaseMs = 100.0f; e.releaseSlowMs = 1500.0f; }
            e.kneeDb = 2.0f;
            e.autoGain = v.on (pAutoGain);
            e.link = v[pLink] >= 50.0f ? 1.0f : 0.0f;
            e.outputDb = clampf (e.outputDb, -9.0f, 12.0f);
            e.thresholdDb = clampf (e.thresholdDb, -40.0f, 0.0f);
            e.sat = Sat::Console; e.satDrive = 0.15f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 400.0f);
            break;
        }
        case Model::A2500:
        {
            static const float k[] = { 0.0f, 6.0f, 12.0f };
            static const float l[] = { 0.0f, 50.0f, 100.0f };
            e.feedback = v.on (pFeedback);
            e.detector = Detector::Rms; e.rmsMs = 0.5f;
            e.kneeDb = snap (e.kneeDb, k, 3, false);
            e.ratio = clampf (e.ratio, 1.5f, 10.0f);
            e.attackMs = clampf (e.attackMs, 0.03f, 30.0f);
            e.releaseMs = clampf (e.releaseMs, 50.0f, 3000.0f);
            if (e.autoRelease) { e.releaseMs = 80.0f; e.releaseSlowMs = 1500.0f; }
            e.thresholdDb = clampf (e.thresholdDb, -40.0f, 0.0f);
            e.outputDb = clampf (e.outputDb, -20.0f, 20.0f);
            e.punch = v.choice (pPunch);
            e.link = snap (v[pLink], l, 3, false) / 100.0f;
            e.sat = Sat::Transformer; e.satDrive = 0.2f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 250.0f);
            break;
        }
        case Model::DB160X:
        {
            static const float k[] = { 0.0f, 12.0f };
            e.detector = Detector::Rms; e.rmsMs = 15.0f;
            e.kneeDb = snap (e.kneeDb, k, 2, false);
            e.ratio = clampf (e.ratio, 1.5f, 20.0f);
            e.attackMs = clampf (e.attackMs, 0.1f, 30.0f);
            e.releaseMs = clampf (e.releaseMs, 50.0f, 1000.0f);
            e.autoRelease = false;
            e.link = v[pLink] >= 50.0f ? 1.0f : 0.0f;
            e.sat = Sat::Transformer; e.satDrive = 0.08f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 320.0f);
            break;
        }
        case Model::PressorX:
        {
            static const float r[] = { 1, 2, 3, 4, 6, 10, 20 };
            const int det = v.choice (pDetector);
            e.detector = det == 2 ? Detector::Opto : det == 1 ? Detector::Rms : Detector::Peak;
            e.rmsMs = 8.0f;
            e.ratio = snap (e.ratio, r, 7, true);
            if (v.on (pFetAll)) { e.ratio = 1000.0f; e.overshoot = 0.2f; }
            e.attackMs = clampf (e.attackMs, 0.05f, 30.0f);
            e.releaseMs = clampf (e.releaseMs, 50.0f, 5000.0f);
            e.releaseSlowMs = e.releaseMs * 3.0f;
            e.kneeDb = 3.0f;
            e.inputDb = 0.0f;
            switch (v.choice (pPressorMode))
            {
                case 0: e.sat = Sat::None; break;                                              // clean
                case 1: e.sat = Sat::Tube; e.satDrive = 0.2f; e.autoRelease = true; break;      // classic
                case 2: e.sat = Sat::Transformer; e.satDrive = 0.3f; e.kneeDb = 1.0f; break;   // brit
                case 3: e.sat = Sat::Tape; e.satDrive = 0.35f; e.kneeDb = 8.0f; break;         // tape
                default: e.sat = Sat::Fet; e.satDrive = 0.5f; e.ratio = 1000.0f;                // nuke
                         e.kneeDb = 0.0f; e.attackMs = std::min (e.attackMs, 0.3f); break;
            }
            e.dist2 = v.on (pDist2) ? 0.35f : 0.0f;
            e.dist3 = v.on (pDist3) ? 0.35f : 0.0f;
            e.link = v[pLink] >= 50.0f ? 1.0f : 0.0f;
            e.scHpfHz = e.scHpfHz > 0.0f ? 100.0f : 0.0f;  // voyant HP : 100 Hz fixe
            break;
        }
        case Model::Fair670:
        case Model::VariMuV:
        {
            const auto t = timeConst (v.choice (pTimeConst));
            e.feedback = true;
            e.detector = Detector::VariMu;
            e.varimuK = limit ? 0.9f : (m == Model::Fair670 ? 0.3f : 0.22f);
            e.kneeDb = 6.0f;
            e.attackMs = t.attackMs;
            e.releaseMs = t.releaseMs;
            e.releaseSlowMs = t.slowMs;
            e.autoRelease = t.program;
            e.inputDb = clampf (v[pInput], -20.0f, 20.0f);
            e.outputDb = clampf (e.outputDb, -20.0f, 20.0f);
            e.thresholdDb = clampf (e.thresholdDb, -30.0f, 0.0f);
            e.sat = Sat::Tube; e.satDrive = m == Model::Fair670 ? 0.25f : 0.2f; e.satGrMod = 0.2f;
            if (m == Model::VariMuV) e.link = v[pLink] >= 50.0f ? 1.0f : 0.0f;
            else e.scExternal = false;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, m == Model::Fair670 ? 400.0f : 320.0f);
            break;
        }
        case Model::ShadowMaster:
            // étage optique doux, puis VCA plus rapide
            e.detector = Detector::Opto;
            e.ratio = clampf (e.ratio, 1.5f, 6.0f);
            e.kneeDb = 10.0f;
            e.attackMs = 20.0f; e.releaseMs = 150.0f; e.releaseSlowMs = 1500.0f; e.autoRelease = true;
            e.secondStage = true;
            e.stage2Ratio = e.ratio;
            e.stage2Offset = 4.0f;
            e.stage2AttackMs = clampf (v[pAttack], 0.1f, 30.0f);
            e.stage2ReleaseMs = clampf (v[pRelease], 50.0f, 1200.0f);
            e.ratio = 2.0f;
            e.inputDb = clampf (v[pInput], -20.0f, 20.0f);
            e.sat = v.on (pShadowTube) ? Sat::Tube : Sat::None;
            e.satDrive = v.on (pShadowTube) ? 0.3f : 0.0f;
            e.dist3 = v[pZenerDrive] / 10.0f * 0.5f;      // ZENER DRIVE
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 400.0f);
            break;

        case Model::N3309:
            e.detector = Detector::Rms; e.rmsMs = 2.0f;
            e.ratio = limit ? 20.0f : clampf (e.ratio, 1.5f, 10.0f);
            e.kneeDb = limit ? 2.0f : 6.0f;
            e.attackMs = limit ? 0.3f : clampf (e.attackMs, 0.3f, 30.0f);
            e.releaseMs = clampf (e.releaseMs, 100.0f, 2500.0f);
            e.autoRelease = false;
            e.thresholdDb = clampf (e.thresholdDb, -30.0f, 0.0f);
            e.outputDb = clampf (e.outputDb, -12.0f, 12.0f);
            e.sat = Sat::Diode; e.satDrive = 0.2f; e.satGrMod = 0.8f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 220.0f);
            break;

        case Model::ZenerX:
        {
            const bool dual = v.on (pZenerDual);
            e.detector = Detector::Peak;
            e.ratio = (limit || dual) ? 20.0f : 4.0f;
            e.kneeDb = (limit || dual) ? 2.0f : 6.0f;
            e.attackMs = clampf (e.attackMs, 0.1f, 30.0f);
            e.releaseMs = clampf (e.releaseMs, 100.0f, 10000.0f);
            e.autoRelease = false;
            if (dual)
            {
                e.secondStage = true; e.stage2Ratio = 3.0f; e.stage2Offset = -8.0f;
                e.stage2AttackMs = 10.0f; e.stage2ReleaseMs = 400.0f;
            }
            e.sat = Sat::Zener; e.satDrive = v[pThd] / 10.0f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 1000.0f);
            break;
        }
        case Model::ProCX:
            e.inputDb = clampf (v[pInput], -24.0f, 24.0f);
            e.autoGain = v.on (pAutoGain);
            e.detector = v.choice (pDetector) == 1 ? Detector::Rms : Detector::Peak;
            e.rmsMs = 10.0f;
            switch (v.choice (pProMode))
            {
                case 1: e.feedback = true; e.sat = Sat::Transformer; e.satDrive = 0.2f; break; // vintage
                case 2: e.detector = Detector::Rms; e.rmsMs = 20.0f; e.autoRelease = true;    // bus
                        e.releaseSlowMs = e.releaseMs * 4.0f; break;
                case 3: e.attackMs = std::max (e.attackMs, 10.0f); e.kneeDb = std::min (e.kneeDb, 3.0f); break; // punch
                default: break;
            }
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 400.0f);
            break;

        case Model::RCRen:
            e.detector = Detector::Rms; e.rmsMs = 5.0f;
            e.ratio = limit ? 20.0f : clampf (e.ratio, 1.5f, 10.0f);
            e.kneeDb = limit ? 3.0f : 10.0f;
            e.attackMs = clampf (e.attackMs, 0.3f, 30.0f);
            e.releaseMs = clampf (e.releaseMs, 100.0f, 2500.0f);
            e.autoRelease = true; e.releaseSlowMs = e.releaseMs * 2.5f; // ARC
            e.thresholdDb = clampf (e.thresholdDb, -30.0f, 0.0f);
            e.outputDb = clampf (e.outputDb, -12.0f, 12.0f);
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 220.0f);
            break;

        case Model::RCVox:
            e.detector = Detector::Rms; e.rmsMs = 5.0f;
            e.ratio = clampf (e.ratio, 1.0f, 20.0f);
            e.attackMs = clampf (e.attackMs, 0.1f, 100.0f);
            e.releaseMs = clampf (e.releaseMs, 10.0f, 1000.0f);
            switch (v.choice (pVoxMode))
            {
                case 0: e.detector = Detector::Opto; e.autoRelease = true; e.releaseSlowMs = 800.0f; break; // vox
                case 2: e.kneeDb = std::max (e.kneeDb, 12.0f); e.autoRelease = true; e.releaseSlowMs = 1500.0f; break; // smooth
                case 3: e.attackMs = std::max (e.attackMs, 15.0f); break; // punch
                default: e.autoRelease = false; break;                    // clean
            }
            e.gateDb = v[pGate];
            e.deEss = v.on (pDeEss);
            e.deEssHz = v[pDeEssFreq];
            e.airDb = v[pAir] / 100.0f * 6.0f;
            e.scHpfHz = clampf (e.scHpfHz, 0.0f, 200.0f);
            break;

        default: break;
    }

    // Les modèles à bouton INPUT
    switch (m)
    {
        case Model::Fair670: case Model::VariMuV: case Model::ShadowMaster: case Model::ProCX: break;
        default: e.inputDb = 0.0f; break;
    }
    // LINK réglable seulement sur A-2500, Bus-Comp G, DB-160X, Pressor-X, Vari-Mu V
    return e;
}
} // namespace comp
