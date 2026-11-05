#include "PanelView.h"
#include "Look.h"

using namespace comp;

#include "Layouts.inc"

const PanelDef* panelFor (Model m) { return findPanel (info (m).id); }

static constexpr float degToRad = 0.017453292519943295f;

PanelView::PanelView (CompProcessor& p) : proc (p)
{
    setModel (proc.model());
    startTimerHz (30);
}

PanelView::~PanelView() { stopTimer(); }

int PanelView::paramIndex (const char* id) const
{
    for (int i = 0; i < numParams; ++i)
        if (std::strcmp (spec (i).id, id) == 0) return i;
    jassertfalse;
    return 0;
}

void PanelView::setModel (Model m)
{
    model = m;
    def = panelFor (m);
    imgs.clear();
    generic.clear(); genericAtt.clear(); genericNames.clear();
    if (def != nullptr)
    {
        const juce::String id (def->model);
        base = look::image (id + "_panel.jpg");
        for (auto& k : def->knobs) imgs["k_" + juce::String (k.id)] = look::image (id + "_k_" + k.id + ".png");
        for (auto& b : def->buttons)
            for (size_t i = 0; i < b.rects.size(); ++i)
                imgs["b_" + juce::String (b.id) + "_" + juce::String ((int) i)] = look::image (id + "_b_" + b.id + "_" + juce::String ((int) i) + ".png");
        for (auto& l : def->leds) imgs["l_" + juce::String (l.id)] = look::image (id + "_l_" + l.id + ".png");
        for (auto& b : def->ledbars)
            for (size_t i = 0; i < b.leds.size(); ++i)
                imgs["m_" + juce::String (b.id) + "_" + juce::String ((int) i)] = look::image (id + "_m_" + b.id + "_" + juce::String ((int) i) + ".png");
    }
    else
    {
        base = look::image (juce::String (info (m).id) + "_thumb.jpg");
        const int ids[] = { pThreshold, pRatio, pAttack, pRelease, pOutput, pMix };
        for (int p : ids)
        {
            auto* s = generic.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow));
            s->setColour (juce::Slider::rotarySliderFillColourId, look::gold);
            s->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a332b));
            s->setColour (juce::Slider::thumbColourId, look::text);
            s->setColour (juce::Slider::textBoxTextColourId, look::text);
            s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
            s->setNumDecimalPlacesToDisplay (p == pAttack ? 2 : p == pRelease || p == pMix ? 0 : 1);
            addAndMakeVisible (s);
            genericAtt.add (new juce::AudioProcessorValueTreeState::SliderAttachment (proc.apvts, spec (p).id, *s));
            const int dec = p == pAttack ? 2 : (p == pRelease || p == pMix) ? 0 : 1;
            const juce::String unit = spec (p).unit;
            s->textFromValueFunction = [dec, unit] (double x) { return juce::String (x, dec) + (unit.isEmpty() ? "" : " " + unit); };
            s->updateText();
            genericNames.add (juce::String (spec (p).name).toUpperCase());
        }
    }
    resized();
    repaint();
}

void PanelView::resized()
{
    if (def == nullptr && ! generic.isEmpty())
    {
        auto r = getLocalBounds().reduced (60, 0).withTrimmedTop (170).withTrimmedBottom (50);
        const int w = r.getWidth() / generic.size();
        for (auto* s : generic) s->setBounds (r.removeFromLeft (w).reduced (12, 0));
    }
}

// ---------- boutons rotatifs ----------

static float interp (const std::vector<std::pair<float, float>>& cal, float v, bool log, bool angleFromValue)
{
    // cal : (valeur, angle). angleFromValue : valeur -> angle, sinon angle -> valeur
    auto key = [&] (const std::pair<float, float>& p) { return angleFromValue ? p.first : p.second; };
    auto val = [&] (const std::pair<float, float>& p) { return angleFromValue ? p.second : p.first; };
    std::vector<std::pair<float, float>> pts (cal);
    std::sort (pts.begin(), pts.end(), [&] (auto& a, auto& b) { return key (a) < key (b); });
    const float lo = key (pts.front()), hi = key (pts.back());
    v = juce::jlimit (lo, hi, v);
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const float k0 = key (pts[i]), k1 = key (pts[i + 1]);
        if (v >= k0 && v <= k1)
        {
            float t;
            const bool logKey = log && angleFromValue && k0 > 0;
            t = k1 == k0 ? 0.0f : logKey ? (std::log (v) - std::log (k0)) / (std::log (k1) - std::log (k0)) : (v - k0) / (k1 - k0);
            const float v0 = val (pts[i]), v1 = val (pts[i + 1]);
            if (log && ! angleFromValue && v0 > 0 && v1 > 0) return std::exp (std::log (v0) + t * (std::log (v1) - std::log (v0)));
            return v0 + t * (v1 - v0);
        }
    }
    return val (pts.back());
}

static float snapStep (const KnobDef& k, float disp)
{
    if (k.steps.empty()) return disp;
    float best = k.steps.front(), bestD = 1e30f;
    for (float st : k.steps)
    {
        const float d = (k.log && st > 0 && disp > 0) ? std::abs (std::log (disp) - std::log (st)) : std::abs (disp - st);
        if (d < bestD) { bestD = d; best = st; }
    }
    return best;
}

static bool knobLogSafe (const KnobDef& k)
{
    // échelle log seulement si toutes les valeurs sont positives
    for (auto& c : k.cal) if (c.first <= 0) return false;
    return k.log;
}

float PanelView::knobAngle (const KnobDef& k) const
{
    const int p = paramIndex (k.param);
    if (! std::isnan (k.autoAngle) && proc.value (pAutoRelease) >= 0.5f) return k.autoAngle;
    const float disp = snapStep (k, k.mapA * proc.value (p) + k.mapB);
    return interp (k.cal, disp, knobLogSafe (k), true);
}

void PanelView::setKnobFromAngle (const KnobDef& k, float angle)
{
    const int p = paramIndex (k.param);
    float lastA = -1e9f, firstA = 1e9f;
    for (auto& c : k.cal) { lastA = juce::jmax (lastA, c.second); firstA = juce::jmin (firstA, c.second); }
    if (! std::isnan (k.autoAngle))
    {
        const bool autoOn = angle > (lastA + k.autoAngle) * 0.5f;
        proc.setValue (pAutoRelease, autoOn ? 1.0f : 0.0f);
        if (autoOn) return;
    }
    const float disp = snapStep (k, interp (k.cal, juce::jlimit (firstA, lastA, angle), knobLogSafe (k), false));
    proc.setValue (p, (disp - k.mapB) / k.mapA);
}

juce::String PanelView::knobText (const KnobDef& k) const
{
    if (! std::isnan (k.autoAngle) && proc.value (pAutoRelease) >= 0.5f) return "AUTO";
    const int p = paramIndex (k.param);
    float disp = k.mapA * proc.value (p) + k.mapB;
    float lo = 1e9f, hi = -1e9f;
    for (auto& c : k.cal) { lo = juce::jmin (lo, c.first); hi = juce::jmax (hi, c.first); }
    disp = snapStep (k, juce::jlimit (lo, hi, disp));
    if (p == pScHpf && disp < 20.0f) return "OFF";
    return juce::String::formatted (k.fmt, disp).replace ("-", juce::String::fromUTF8 ("\xe2\x88\x92"));
}

const KnobDef* PanelView::knobAt (juce::Point<float> pt) const
{
    if (def == nullptr) return nullptr;
    const float s = sc();
    for (auto& k : def->knobs)
        if (pt.getDistanceFrom ({ k.cx * s, k.cy * s }) <= k.rs * s * 1.15f) return &k;
    return nullptr;
}

// ---------- dessin ----------

static void drawPointer (juce::Graphics& g, float cx, float cy, float angleDeg, float rIn, float rOut, float wOut, juce::Colour col)
{
    const float a = angleDeg * degToRad;
    const juce::Point<float> u (std::sin (a), -std::cos (a)), n (-u.y, u.x);
    auto poly = [&] (juce::Point<float> off)
    {
        juce::Path p;
        const float wIn = wOut * 0.45f;
        const juce::Point<float> c (cx + off.x, cy + off.y);
        p.startNewSubPath (c + u * rIn + n * (wIn / 2));
        p.lineTo (c + u * rOut + n * (wOut / 2));
        p.lineTo (c + u * rOut - n * (wOut / 2));
        p.lineTo (c + u * rIn - n * (wIn / 2));
        p.closeSubPath();
        return p;
    };
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), (int) juce::jmax (2.0f, wOut * 0.6f), { 1, 2 }).drawForPath (g, poly ({ 0.5f, 1.0f }));
    g.setColour (col);
    g.fillPath (poly ({}));
    g.setColour (juce::Colour (0xff78787d));
    g.strokePath (poly ({}), juce::PathStrokeType (juce::jmax (0.6f, wOut * 0.12f)));
}

void PanelView::paint (juce::Graphics& g)
{
    if (def == nullptr) { paintGeneric (g); return; }
    const float s = sc();
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.setOpacity (1.0f);
    g.drawImage (base, getLocalBounds().toFloat());
    const auto v = proc.values();

    for (auto& b : def->buttons)
    {
        const int p = paramIndex (b.param);
        int best = 0; float bestD = 1e30f;
        for (size_t i = 0; i < b.values.size(); ++i)
        {
            const float d = std::abs (v[p] - b.values[i]);
            if (d < bestD) { bestD = d; best = (int) i; }
        }
        if (b.toggle)
        {
            const bool lit = std::isnan (b.litAbove) ? std::abs (v[p] - b.values[0]) < 0.01f : v[p] >= b.litAbove;
            if (! lit) continue;
            best = 0;
        }
        const auto& img = imgs["b_" + juce::String (b.id) + "_" + juce::String (best)];
        const auto r = b.rects[(size_t) best].expanded (b.glow).toFloat();
        g.setOpacity (1.0f);
        g.drawImage (img, { r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s });
    }
    for (auto& l : def->leds)
        if (v.on (paramIndex (l.param)))
        {
            const auto r = l.rect.toFloat();
            g.setOpacity (1.0f);
            g.drawImage (imgs["l_" + juce::String (l.id)], { r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s });
        }

    for (auto& n : def->needles)
    {
        const float val = needle[n.channel];
        const float a = interp (n.scale, val, false, true) * degToRad;
        juce::Graphics::ScopedSaveState ss (g);
        const auto face = n.face.toFloat();
        g.reduceClipRegion (juce::Rectangle<float> (face.getX() * s, face.getY() * s, face.getWidth() * s, face.getHeight() * s).toNearestInt());
        const juce::Point<float> piv (n.px * s, n.py * s), tip = piv + juce::Point<float> (std::sin (a), -std::cos (a)) * (n.len * s);
        g.setColour (juce::Colour (0x40502800));
        g.drawLine ({ piv.translated (2.0f * s, 1.5f * s), tip.translated (2.0f * s, 1.5f * s) }, 3.0f * s);
        g.setColour (juce::Colour (0xff141210));
        g.drawLine ({ piv, tip }, 3.2f * s);
    }

    for (auto& lb : def->ledbars)
    {
        const float gr = lb.channel < 0 ? juce::jmax (needle[0], needle[1]) : needle[lb.channel];
        for (size_t i = 0; i < lb.leds.size(); ++i)
            if (gr >= lb.leds[i].second * 0.92f)
            {
                const auto r = lb.leds[i].first.toFloat();
                g.drawImage (imgs["m_" + juce::String (lb.id) + "_" + juce::String ((int) i)], { r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s });
            }
    }

    for (auto& k : def->knobs)
    {
        const auto& spr = imgs["k_" + juce::String (k.id)];
        const float half = (float) (k.spriteW - 1) * 0.5f;
        const float x0 = std::round (k.cx) - std::floor (half), y0 = std::round (k.cy) - std::floor (half);
        g.setOpacity (1.0f);
        g.drawImage (spr, { x0 * s, y0 * s, (float) spr.getWidth() * s, (float) spr.getHeight() * s });
        drawPointer (g, k.cx * s, k.cy * s, knobAngle (k), k.ptrIn * s, k.ptrLen * s, k.ptrW * s, juce::Colour (k.ptrColour));
    }

    // bulle de valeur
    if (auto* k = dragKnob != nullptr ? dragKnob : hoverKnob)
    {
        const auto txt = knobText (*k);
        const auto f = look::font (15.0f, 700);
        const float w = juce::GlyphArrangement::getStringWidth (f, txt) + 18.0f, h = 22.0f;
        juce::Rectangle<float> r (k->cx * s - w / 2, (k->cy + k->rs) * s + 4.0f, w, h);
        if (r.getBottom() > (float) getHeight() - 2) r.setY ((k->cy - k->rs) * s - h - 4.0f);
        g.setColour (juce::Colour (0xe0100e0c));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (look::gold.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (look::text);
        g.setFont (f);
        g.drawText (txt, r, juce::Justification::centred);
    }
}

void PanelView::paintGeneric (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0c0b));
    if (base.isValid())
    {
        g.setOpacity (0.16f);
        g.drawImage (base, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination);
        g.setOpacity (1.0f);
    }
    g.setColour (look::text);
    g.setFont (look::font (34.0f, 700));
    g.drawText (info (model).name, getLocalBounds().withHeight (70).withTrimmedTop (24), juce::Justification::centred);
    g.setColour (look::gold);
    g.setFont (look::font (15.0f, 600));
    g.drawText (juce::String::fromUTF8 ("FAÇADE EN PRÉPARATION — LE SON EST DÉJÀ CELUI DU MODÈLE"),
                getLocalBounds().withY (88).withHeight (24), juce::Justification::centred);
    g.setColour (look::label);
    g.setFont (look::font (14.0f, 600));
    for (int i = 0; i < generic.size(); ++i)
        g.drawText (genericNames[i], generic[i]->getBounds().withY (generic[i]->getY() - 24).withHeight (20), juce::Justification::centred);

    // réduction de gain
    auto r = juce::Rectangle<float> (60.0f, (float) getHeight() - 30.0f, (float) getWidth() - 120.0f, 8.0f);
    for (int c = 0; c < 2; ++c)
    {
        auto bar = r.translated (0, (float) c * 11.0f - 8.0f);
        g.setColour (juce::Colour (0xff221e1a));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (look::gold);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, needle[c] / 24.0f)), 3.0f);
    }
}

void PanelView::timerCallback()
{
    for (int c = 0; c < 2; ++c)
    {
        const float target = -proc.grDb[c].load();
        const float k = target > needle[c] ? 0.45f : 0.12f;   // inertie d'aiguille (VU)
        needle[c] += k * (target - needle[c]);
    }
    if (def != nullptr && def->needles.empty() && def->ledbars.empty() && generic.isEmpty()) return;
    repaint();
}

// ---------- souris ----------

void PanelView::mouseDown (const juce::MouseEvent& e)
{
    if (def == nullptr) return;
    const float s = sc();
    if (auto* k = knobAt (e.position))
    {
        dragKnob = k;
        dragAngle = knobAngle (*k);
        lastDrag = e.position;
        if (auto* pr = proc.param (paramIndex (k->param))) pr->beginChangeGesture();
        repaint();
        return;
    }
    for (auto& b : def->buttons)
        for (size_t i = 0; i < b.rects.size(); ++i)
        {
            const auto r = b.rects[i].toFloat();
            if (juce::Rectangle<float> (r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s).contains (e.position))
            {
                const int p = paramIndex (b.param);
                if (b.toggle)
                {
                    const float cur = proc.value (p);
                    const bool lit = std::isnan (b.litAbove) ? std::abs (cur - b.values[i]) < 0.01f : cur >= b.litAbove;
                    proc.setValue (p, lit ? (std::isnan (b.off) ? spec (p).def : b.off) : b.values[i]);
                }
                else proc.setValue (p, b.values[i]);
                for (auto& [id, val] : b.also) proc.setValue (paramIndex (id), val);
                repaint();
                return;
            }
        }
    for (auto& l : def->leds)
    {
        const auto r = l.rect.toFloat();
        if (juce::Rectangle<float> (r.getX() * s, r.getY() * s, r.getWidth() * s, r.getHeight() * s).contains (e.position))
        {
            const int p = paramIndex (l.param);
            proc.setValue (p, proc.value (p) >= 0.5f ? 0.0f : 1.0f);
            repaint();
            return;
        }
    }
}

void PanelView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragKnob == nullptr) return;
    const auto d = e.position - lastDrag;
    lastDrag = e.position;
    const float speed = e.mods.isShiftDown() ? 0.18f : 0.9f;   // degrés par pixel
    dragAngle += (d.x - d.y) * speed;
    float lo = 1e9f, hi = -1e9f;
    for (auto& c : dragKnob->cal) { lo = juce::jmin (lo, c.second); hi = juce::jmax (hi, c.second); }
    if (! std::isnan (dragKnob->autoAngle)) hi = dragKnob->autoAngle;
    dragAngle = juce::jlimit (lo, hi, dragAngle);
    setKnobFromAngle (*dragKnob, dragAngle);
    repaint();
}

void PanelView::mouseUp (const juce::MouseEvent&)
{
    if (dragKnob != nullptr)
        if (auto* pr = proc.param (paramIndex (dragKnob->param))) pr->endChangeGesture();
    dragKnob = nullptr;
    repaint();
}

void PanelView::mouseMove (const juce::MouseEvent& e)
{
    auto* k = knobAt (e.position);
    if (k != hoverKnob) { hoverKnob = k; repaint(); }
    setMouseCursor (k != nullptr ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void PanelView::mouseExit (const juce::MouseEvent&)
{
    if (hoverKnob != nullptr) { hoverKnob = nullptr; repaint(); }
}

void PanelView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* k = knobAt (e.position))
    {
        const int p = paramIndex (k->param);
        proc.setValue (p, spec (p).def);
        if (! std::isnan (k->autoAngle)) proc.setValue (pAutoRelease, 0.0f);
        repaint();
    }
}

void PanelView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (auto* k = knobAt (e.position))
    {
        float a = knobAngle (*k) + (w.deltaY + w.deltaX) * 40.0f;
        float lo = 1e9f, hi = -1e9f;
        for (auto& c : k->cal) { lo = juce::jmin (lo, c.second); hi = juce::jmax (hi, c.second); }
        if (! std::isnan (k->autoAngle)) hi = k->autoAngle;
        setKnobFromAngle (*k, juce::jlimit (lo, hi, a));
        repaint();
    }
}
