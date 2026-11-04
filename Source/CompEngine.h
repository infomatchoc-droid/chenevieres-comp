// Chènevières Comp — moteur de compression (C++ pur, sans JUCE : testable seul)
#pragma once
#include "Interpret.h"
#include <atomic>
#include <cmath>
#include <vector>

namespace comp
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    float process (float x, int c)
    {
        const float y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void setHighPass (double fs, double f, double q);
    void setLowPass (double fs, double f, double q);
    void setBandPass (double fs, double f, double q);
    void setHighShelf (double fs, double f, double gainDb);
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
};

class Engine
{
public:
    void prepare (double sampleRate, int maxChannels);
    void reset();
    void setParams (const Eff& e) { eff = e; dirty = true; }

    // Traitement en place. sc = sidechain externe (peut être nul)
    void process (float* const* ch, int numCh, int n, const float* const* sc, int numScCh);

    // Réduction de gain maximale du dernier bloc (dB, négative), par canal
    float lastGr (int c) const { return grPeak[c < 2 ? c : 1]; }

    // Courbe statique (sans temps) : utile pour l'affichage et les tests
    float staticGain (float levelDb) const;

private:
    void update();
    float computer (float levelDb, float thr, float ratio, float knee) const;
    float saturate (float x, float grDb, int c);

    Eff eff;
    bool dirty = true;
    double fs = 48000.0;
    int maxCh = 2;

    Biquad scHpf, scTilt, scTilt2, outLpf, deEssBp, deEssHp, airShelf;
    bool useScHpf = false, useTilt = false, useLpf = false, useAir = false;

    // états par canal
    float rms[2] {}, env[2] {}, env2[2] {}, slow[2] {}, memory[2] {}, yPrev[2] {};
    float gateEnv[2] {}, deEnv[2] {}, dcX[2] {}, dcY[2] {}, over[2] {};
    float grPeak[2] {};

    // coefficients
    float aAtt = 0, aRel = 0, aRelSlow = 0, aRms = 0, aSlowAvg = 0, aMem = 0;
    float a2Att = 0, a2Rel = 0, aGateA = 0, aGateR = 0, aDeA = 0, aDeR = 0, aDc = 0;
    float inGain = 1, outGain = 1, makeup = 1;
    float curInGain = 1, curOutGain = 1, curMix = 1;
};
} // namespace comp
