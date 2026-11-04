#include "CompEngine.h"
#include <algorithm>

namespace comp
{
static constexpr double pi = 3.14159265358979323846;

static void setRbj (Biquad& q, double b0, double b1, double b2, double a0, double a1, double a2)
{
    q.b0 = (float) (b0 / a0); q.b1 = (float) (b1 / a0); q.b2 = (float) (b2 / a0);
    q.a1 = (float) (a1 / a0); q.a2 = (float) (a2 / a0);
}

void Biquad::setHighPass (double fs, double f, double Q)
{
    const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
    setRbj (*this, (1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}
void Biquad::setLowPass (double fs, double f, double Q)
{
    const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
    setRbj (*this, (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}
void Biquad::setBandPass (double fs, double f, double Q)
{
    const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * Q);
    setRbj (*this, al, 0, -al, 1 + al, -2 * c, 1 - al);
}
void Biquad::setHighShelf (double fs, double f, double g)
{
    const double A = std::pow (10.0, g / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs;
    const double c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
    setRbj (*this, A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
            (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
}

static inline float dbToGain (float db) { return std::exp (db * 0.11512925464970229f); }
static inline float gainToDb (float g) { return 8.685889638065035f * std::log (std::max (g, 1e-9f)); }

void Engine::prepare (double sampleRate, int maxChannels)
{
    fs = sampleRate;
    maxCh = std::min (2, std::max (1, maxChannels));
    dirty = true;
    reset();
}

void Engine::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        rms[c] = env[c] = env2[c] = slow[c] = memory[c] = yPrev[c] = 0;
        gateEnv[c] = deEnv[c] = dcX[c] = dcY[c] = over[c] = 0;
        grPeak[c] = 0;
    }
    for (auto* q : { &scHpf, &scTilt, &scTilt2, &outLpf, &deEssBp, &deEssHp, &airShelf }) q->reset();
    curInGain = inGain; curOutGain = outGain * makeup; curMix = eff.mix;
}

float Engine::computer (float L, float T, float R, float W) const
{
    const float over = L - T;
    const float s = R >= 999.0f ? -1.0f : (1.0f / std::max (R, 1.0f) - 1.0f);
    if (W <= 0.01f) return over > 0 ? s * over : 0.0f;
    if (2 * over < -W) return 0.0f;
    if (2 * std::abs (over) <= W) return s * (over + W / 2) * (over + W / 2) / (2 * W);
    return s * over;
}

float Engine::staticGain (float L) const
{
    if (eff.detector == Detector::VariMu)
    {
        const float over = L - eff.thresholdDb, h = eff.kneeDb / 2;
        const float o = 0.5f * (over + std::sqrt (over * over + h * h));   // genou doux
        const float k = eff.varimuK;
        return std::log1p (k * o) / k - o;
    }
    float g = computer (L, eff.thresholdDb, eff.ratio, eff.kneeDb);
    if (eff.secondStage)
        g += computer (L + g, eff.thresholdDb + eff.stage2Offset, eff.stage2Ratio, 3.0f);
    return g;
}

static float coefMs (double fs, float ms)
{
    return (float) std::exp (-1.0 / (std::max (0.005, (double) ms) * 0.001 * fs));
}

void Engine::update()
{
    dirty = false;
    aAtt = coefMs (fs, eff.attackMs);
    aRel = coefMs (fs, eff.releaseMs);
    aRelSlow = coefMs (fs, eff.releaseSlowMs);
    float rmsMs = eff.rmsMs;
    if (eff.detector == Detector::Opto) rmsMs = 10.0f;
    if (eff.detector == Detector::VariMu) rmsMs = 3.0f;
    aRms = rmsMs > 0 ? coefMs (fs, rmsMs) : 0.0f;
    aSlowAvg = coefMs (fs, 400.0f);
    aMem = coefMs (fs, 2500.0f);
    a2Att = coefMs (fs, eff.stage2AttackMs);
    a2Rel = coefMs (fs, eff.stage2ReleaseMs);
    aGateA = coefMs (fs, 1.0f); aGateR = coefMs (fs, 80.0f);
    aDeA = coefMs (fs, 1.0f); aDeR = coefMs (fs, 60.0f);
    aDc = 1.0f - (float) (2 * pi * 8.0 / fs);

    useScHpf = eff.scHpfHz >= 10.0f;
    if (useScHpf) scHpf.setHighPass (fs, eff.scHpfHz, 0.707);
    useTilt = eff.punch > 0;
    if (useTilt)
    {
        static const float f[] = { 0, 60, 120, 240 }, g[] = { 0, 2, 3, 4.5f };
        scTilt.setHighPass (fs, f[eff.punch], 0.55);
        scTilt2.setHighShelf (fs, 2500.0, g[eff.punch]);
    }
    useLpf = eff.outLpfHz > 0;
    if (useLpf) outLpf.setLowPass (fs, eff.outLpfHz, 0.6);
    deEssBp.setBandPass (fs, eff.deEssHz, 1.4);
    deEssHp.setHighPass (fs, eff.deEssHz * 0.7, 0.707);
    useAir = eff.airDb > 0.05f;
    if (useAir) airShelf.setHighShelf (fs, 10000.0, eff.airDb);

    inGain = dbToGain (eff.inputDb);
    outGain = dbToGain (eff.outputDb);
    makeup = eff.autoGain ? dbToGain (-0.5f * staticGain (0.0f)) : 1.0f;
}

float Engine::saturate (float x, float gr, int c)
{
    if (eff.sat == Sat::None && eff.dist2 <= 0 && eff.dist3 <= 0) return x;
    const float drive = eff.satDrive * (1.0f + eff.satGrMod * (-gr) / 10.0f);
    float y = x;
    bool dc = false;
    if (eff.sat != Sat::None && drive > 0.001f)
    {
        const float k = 0.15f + 2.5f * drive;
        switch (eff.sat)
        {
            case Sat::Transformer:
            case Sat::Console:
                y = std::tanh (k * x) / k + 0.04f * drive * x * x; dc = true; break;
            case Sat::Tube:
            {
                const float b = 0.05f + 0.15f * drive, tb = std::tanh (k * b);
                y = (std::tanh (k * (x + b)) - tb) / (k * (1.0f - tb * tb)); dc = true; break;
            }
            case Sat::Fet:
                y = std::tanh (k * x) / k; break;
            case Sat::Diode:
            {
                const float u = x + 0.25f * drive * x * std::abs (x);
                y = std::tanh (k * u) / k; dc = true; break;
            }
            case Sat::Zener:
            {
                const float u = k * x;
                y = u / std::pow (1.0f + u * u * u * u, 0.25f) / k; break;
            }
            case Sat::Tape:
                y = std::tanh (k * (x + 0.03f)) / k - std::tanh (k * 0.03f) / k; dc = true; break;
            default: break;
        }
    }
    if (eff.dist2 > 0) { y += eff.dist2 * 0.3f * y * y; dc = true; }
    if (eff.dist3 > 0) { const float u = y * (1.0f + eff.dist3); y = std::tanh (u) / (1.0f + eff.dist3 * 0.5f); }
    if (dc)
    {
        const float o = y - dcX[c] + aDc * dcY[c];
        dcX[c] = y; dcY[c] = o;
        y = o;
    }
    return y;
}

void Engine::process (float* const* ch, int numCh, int n, const float* const* sc, int numScCh)
{
    if (dirty) update();
    numCh = std::min (numCh, 2);
    grPeak[0] = grPeak[1] = 0.0f;
    const float sm = 1.0f - coefMs (fs, 15.0f);
    const float tIn = inGain, tOut = outGain * makeup, tMix = eff.mix;
    const bool ext = eff.scExternal && sc != nullptr && numScCh > 0;
    const bool optoLike = eff.detector == Detector::Opto;

    for (int i = 0; i < n; ++i)
    {
        curInGain += sm * (tIn - curInGain);
        curOutGain += sm * (tOut - curOutGain);
        curMix += sm * (tMix - curMix);

        float xi[2], lv[2];
        for (int c = 0; c < numCh; ++c)
        {
            xi[c] = ch[c][i] * curInGain;
            float s = ext ? sc[std::min (c, numScCh - 1)][i] : (eff.feedback ? yPrev[c] : xi[c]);
            if (useScHpf) s = scHpf.process (s, c);
            if (useTilt) s = scTilt2.process (scTilt.process (s, c), c);
            if (aRms > 0)
            {
                rms[c] = s * s + aRms * (rms[c] - s * s);
                lv[c] = std::sqrt (rms[c]);
            }
            else lv[c] = std::abs (s);
        }
        if (numCh == 2 && eff.link > 0)
        {
            const float m = std::max (lv[0], lv[1]);
            for (int c = 0; c < 2; ++c) lv[c] = eff.link * m + (1 - eff.link) * lv[c];
        }

        for (int c = 0; c < numCh; ++c)
        {
            const float L = gainToDb (lv[c]);
            float target;
            if (eff.detector == Detector::VariMu)
            {
                const float over = L - eff.thresholdDb, h = eff.kneeDb / 2;
                const float o = 0.5f * (over + std::sqrt (over * over + h * h));
                target = std::log1p (eff.varimuK * o) / eff.varimuK - o;
            }
            else target = computer (L, eff.thresholdDb, eff.ratio, eff.kneeDb);
            if (eff.overshoot > 0) target *= (1.0f + eff.overshoot);

            float& e = env[c];
            if (target < e) e = target + aAtt * (e - target);
            else
            {
                float a = aRel;
                if (optoLike)
                {
                    // cellule optique : relâchement rapide puis lent, mémoire des fortes compressions
                    memory[c] = std::min (1.0f, -e / 10.0f) + aMem * (memory[c] - std::min (1.0f, -e / 10.0f));
                    const float w = std::clamp (0.35f + 0.65f * memory[c], 0.0f, 1.0f);
                    const float fastPart = (e - target) > 0.5f * std::abs (target) + 1.0f ? 1.0f : 0.0f;
                    a = fastPart > 0 ? aRel : (aRel * (1 - w) + aRelSlow * w);
                }
                else if (eff.autoRelease)
                {
                    slow[c] = e + aSlowAvg * (slow[c] - e);
                    const float w = std::clamp (-slow[c] / 6.0f, 0.0f, 1.0f);
                    a = aRel * (1 - w) + aRelSlow * w;
                }
                e = target + a * (e - target);
            }
            float total = e;
            if (eff.secondStage)
            {
                const float t2 = computer (L + e, eff.thresholdDb + eff.stage2Offset, eff.stage2Ratio, 3.0f);
                float& e2 = env2[c];
                e2 = t2 < e2 ? t2 + a2Att * (e2 - t2) : t2 + a2Rel * (e2 - t2);
                total += e2;
            }
            grPeak[c] = std::min (grPeak[c], total);

            const float g = dbToGain (total);
            yPrev[c] = xi[c] * g;
            float y = saturate (xi[c] * g, total, c);
            if (useLpf) y = outLpf.process (y, c);
            if (eff.deEss)
            {
                const float b = deEssBp.process (xi[c], c);
                const float a = std::abs (b);
                deEnv[c] = a > deEnv[c] ? a + aDeA * (deEnv[c] - a) : a + aDeR * (deEnv[c] - a);
                const float over = gainToDb (deEnv[c]) + 30.0f;          // seuil -30 dBFS
                const float red = over > 0 ? std::max (-12.0f, -over * 0.75f) : 0.0f;
                const float hp = deEssHp.process (y, c);
                y += hp * (dbToGain (red) - 1.0f);
            }
            if (useAir) y = airShelf.process (y, c);
            if (eff.gateDb > -79.5f)
            {
                const float gl = L < eff.gateDb ? std::max (-60.0f, (L - eff.gateDb) * 3.0f) : 0.0f;
                gateEnv[c] = gl < gateEnv[c] ? gl + aGateA * (gateEnv[c] - gl) : gl + aGateR * (gateEnv[c] - gl);
                y *= dbToGain (gateEnv[c]);
            }
            const float dry = ch[c][i];
            ch[c][i] = (dry * (1.0f - curMix) + y * curMix) * curOutGain;
        }
    }
    if (numCh == 1) grPeak[1] = grPeak[0];
}
} // namespace comp
