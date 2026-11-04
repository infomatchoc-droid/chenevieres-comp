#include "TopBar.h"
#include "Look.h"
#include "PanelView.h"

using namespace comp;

// ================= Barre du haut =================

TopBar::TopBar (CompProcessor& p, std::function<void()> ob, std::function<void (float)> ss)
    : proc (p), openBrowser (std::move (ob)), setScale (std::move (ss))
{
    wordmark = look::image ("wordmark_comp.png");
    beagle = look::image ("logo_beagle.png");
    startTimerHz (8);
}

TopBar::~TopBar() { stopTimer(); }

void TopBar::resized()
{
    // positions en unités logiques (barre de 1086 x 66)
    const float cy = 33.0f, h = 34.0f, y = cy - h / 2;
    float x = 13.0f + (wordmark.isValid() ? 38.0f * wordmark.getWidth() / wordmark.getHeight() : 190.0f) + 15.0f;
    zones[ModelPrev] = { x, y, 27, h };
    zones[ModelBox] = { x + 33, y, 167, h };
    zones[ModelNext] = { x + 206, y, 27, h };
    x += 206 + 27 + 13;
    zones[PresetPrev] = { x, y, 27, h };
    zones[PresetBox] = { x + 33, y, 120, h };
    zones[PresetNext] = { x + 159, y, 27, h };
    x += 159 + 27 + 13;
    zones[SlotA] = { x, y, 31, h };
    zones[SlotB] = { x + 37, y, 31, h };
    zones[Copy] = { x + 74, y, 82, h };
    x += 156 + 10;
    zones[Bypass] = { x, y, 70, h };
    zones[Gear] = { x + 77, y, 35, h };
}

static void box (juce::Graphics& g, juce::Rectangle<float> r, bool lit, bool hover, juce::Colour line = look::boxLine)
{
    g.setColour (lit ? juce::Colour (0xff46321a) : look::boxFill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (lit ? look::gold : hover ? line.brighter (0.5f) : line);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
}

static void arrow (juce::Graphics& g, juce::Rectangle<float> r, bool left, bool hover)
{
    box (g, r, false, hover);
    juce::Path p;
    const auto c = r.getCentre();
    if (left) p.addTriangle (c.x + 3.5f, c.y - 5.5f, c.x + 3.5f, c.y + 5.5f, c.x - 3.5f, c.y);
    else p.addTriangle (c.x - 3.5f, c.y - 5.5f, c.x - 3.5f, c.y + 5.5f, c.x + 3.5f, c.y);
    g.setColour (look::gold);
    g.fillPath (p);
}

static void caret (juce::Graphics& g, float x, float y)
{
    juce::Path p;
    p.addTriangle (x - 4.5f, y - 3.0f, x + 4.5f, y - 3.0f, x, y + 3.0f);
    g.setColour (look::gold);
    g.fillPath (p);
}

void TopBar::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e1b18), 0, 0, juce::Colour (0xff0d0c0b), 0, b.getHeight(), false));
    g.fillRect (b);
    g.setColour (look::goldDim.withAlpha (0.6f));
    g.fillRect (0.0f, b.getHeight() - 1.0f, b.getWidth(), 1.0f);

    // logo
    if (wordmark.isValid())
    {
        const float wh = 38.0f, ww = wh * wordmark.getWidth() / wordmark.getHeight();
        g.setOpacity (1.0f);
        g.drawImage (wordmark, { 13.0f, 33.0f - wh / 2 + 1, ww, wh });
    }

    auto lbl = [&] (juce::Rectangle<float> r, const char* t)
    {
        g.setColour (look::label);
        g.setFont (look::font (9.5f, 600));
        g.drawText (juce::String::fromUTF8 (t), r.withY (r.getY() - 13).withHeight (11).withWidth (200), juce::Justification::left);
    };

    // modèle
    lbl (zones[ModelPrev], "MODÈLE");
    arrow (g, zones[ModelPrev], true, hover == ModelPrev);
    box (g, zones[ModelBox], false, hover == ModelBox, look::goldDim);
    {
        const auto& mi = info (proc.model());
        auto r = zones[ModelBox].reduced (12, 0);
        g.setColour (look::text);
        g.setFont (look::font (20.0f, 700));
        const float nw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), mi.name);
        g.drawText (mi.name, r, juce::Justification::centredLeft);
        g.setColour (look::textDim);
        g.setFont (look::font (11.0f, 500));
        g.drawText (juce::String::fromUTF8 (mi.subtitle), r.withTrimmedLeft (nw + 8).withTrimmedRight (14), juce::Justification::centredLeft);
        caret (g, zones[ModelBox].getRight() - 13, zones[ModelBox].getCentreY());
    }
    arrow (g, zones[ModelNext], false, hover == ModelNext);

    // preset
    lbl (zones[PresetPrev], "PRESET");
    arrow (g, zones[PresetPrev], true, hover == PresetPrev);
    box (g, zones[PresetBox], false, hover == PresetBox);
    {
        const auto& list = factoryPresets();
        const auto name = juce::String::fromUTF8 (list[(size_t) juce::jlimit (0, (int) list.size() - 1, proc.presetIndex)].name);
        g.setColour (look::text);
        g.setFont (look::font (13.0f, 500));
        g.drawFittedText (name, zones[PresetBox].reduced (10, 0).withTrimmedRight (12).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        caret (g, zones[PresetBox].getRight() - 11, zones[PresetBox].getCentreY());
    }
    arrow (g, zones[PresetNext], false, hover == PresetNext);

    // A / B
    lbl (zones[SlotA], "COMPARAISON");
    for (int s = 0; s < 2; ++s)
    {
        const auto r = zones[s == 0 ? SlotA : SlotB];
        const bool on = proc.abSlot == s;
        box (g, r, on, hover == (s == 0 ? SlotA : SlotB));
        g.setColour (on ? juce::Colour (0xffffd68c) : look::textDim);
        g.setFont (look::font (16.0f, 700));
        g.drawText (s == 0 ? "A" : "B", r, juce::Justification::centred);
    }
    box (g, zones[Copy], false, hover == Copy);
    g.setColour (look::textDim);
    g.setFont (look::font (12.0f, 500));
    g.drawText (juce::String::fromUTF8 (proc.abSlot == 0 ? "COPIER A \xe2\x86\x92 B" : "COPIER B \xe2\x86\x92 A"), zones[Copy], juce::Justification::centred);

    // bypass
    {
        const bool on = proc.value (pBypass) >= 0.5f;
        const auto r = zones[Bypass];
        box (g, r, false, hover == Bypass);
        const juce::Rectangle<float> led (r.getX() + 9, r.getCentreY() - 4, 8, 8);
        g.setColour (on ? juce::Colour (0xffff6a3a) : juce::Colour (0xff3c342c));
        g.fillEllipse (led);
        if (on) { g.setColour (juce::Colour (0x55ff6a3a)); g.fillEllipse (led.expanded (3)); }
        g.setColour (on ? look::text : look::textDim);
        g.setFont (look::font (13.0f, 600));
        g.drawText ("BYPASS", r.withTrimmedLeft (18), juce::Justification::centred);
    }

    // réglages (roue dentée)
    {
        const auto r = zones[Gear];
        box (g, r, false, hover == Gear);
        const auto c = r.getCentre();
        juce::Path gear;
        for (int k = 0; k < 8; ++k)
        {
            juce::Path tooth;
            tooth.addRoundedRectangle (-2.2f, -10.5f, 4.4f, 6.0f, 1.0f);
            gear.addPath (tooth, juce::AffineTransform::rotation (k * juce::MathConstants<float>::pi / 4).translated (c));
        }
        gear.addEllipse (c.x - 7.5f, c.y - 7.5f, 15, 15);
        g.setColour (juce::Colour (0xffcdc3b4));
        g.fillPath (gear);
        g.setColour (look::boxFill);
        g.fillEllipse (c.x - 3.2f, c.y - 3.2f, 6.4f, 6.4f);
    }

    // Pistache, qui regarde depuis le bord droit
    if (beagle.isValid())
    {
        const float bh = b.getHeight() - 1.0f, bw = bh * beagle.getWidth() / beagle.getHeight() * 1.04f;
        const juce::Rectangle<float> r (b.getRight() - bw - 2.0f, 0.0f, bw, bh);
        // fondu sur la gauche
        juce::Graphics::ScopedSaveState ss (g);
        g.setOpacity (1.0f);
        g.drawImage (beagle, r);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff171513), r.getX(), 0, juce::Colour (0x00171513), r.getX() + r.getWidth() * 0.24f, 0, false));
        g.fillRect (r);
    }
}

TopBar::Zone TopBar::zoneAt (juce::Point<float> p) const
{
    for (int z = ModelPrev; z < numZones; ++z)
        if (zones[(size_t) z].contains (p)) return (Zone) z;
    return None;
}

void TopBar::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z != hover) { hover = z; repaint(); }
    setMouseCursor (z != None ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void TopBar::mouseExit (const juce::MouseEvent&) { hover = None; repaint(); }

void TopBar::stepModel (int dir)
{
    const int m = ((int) proc.model() + dir + numModels) % numModels;
    proc.setModel ((Model) m);
}

void TopBar::stepPreset (int dir)
{
    const int n = (int) factoryPresets().size();
    proc.loadPreset ((proc.presetIndex + dir + n) % n);
}

void TopBar::showPresetMenu()
{
    juce::PopupMenu m;
    const auto& list = factoryPresets();
    for (int i = 0; i < (int) list.size(); ++i)
        m.addItem (i + 1, juce::String::fromUTF8 (list[(size_t) i].name), true, i == proc.presetIndex);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (zones[PresetBox].toNearestInt())),
                     [this] (int r) { if (r > 0) { proc.loadPreset (r - 1); repaint(); } });
}

void TopBar::showGearMenu()
{
    juce::PopupMenu m, size;
    const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    for (int i = 0; i < 6; ++i)
        size.addItem (100 + i, juce::String (juce::roundToInt (scales[i] * 100)) + " %", true, std::abs (proc.uiScale - scales[i]) < 0.01f);
    m.addSubMenu ("Taille", size);
    m.addSeparator();
    m.addItem (1, juce::String::fromUTF8 ("Chènevières Comp ") + JucePlugin_VersionString, false);
    m.addItem (2, juce::String::fromUTF8 ("Studio des Chènevières — Division Digitale & Créative"), false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (zones[Gear].toNearestInt())),
                     [this, scales] (int r) { if (r >= 100) setScale (scales[r - 100]); });
}

void TopBar::mouseDown (const juce::MouseEvent& e)
{
    switch (zoneAt (e.position))
    {
        case ModelPrev: stepModel (-1); break;
        case ModelNext: stepModel (1); break;
        case ModelBox: openBrowser(); break;
        case PresetPrev: stepPreset (-1); break;
        case PresetNext: stepPreset (1); break;
        case PresetBox: showPresetMenu(); break;
        case SlotA: proc.selectSlot (0); break;
        case SlotB: proc.selectSlot (1); break;
        case Copy: proc.copyAToB(); break;
        case Bypass: proc.setValue (pBypass, proc.value (pBypass) >= 0.5f ? 0.0f : 1.0f); break;
        case Gear: showGearMenu(); break;
        default: break;
    }
    repaint();
}

// ================= Menu des modèles =================

ModelBrowser::ModelBrowser (CompProcessor& p, std::function<void()> c) : proc (p), close (std::move (c))
{
    setWantsKeyboardFocus (true);
}

const std::array<int, numModels>& ModelBrowser::order()
{
    // par famille
    static const std::array<int, numModels> o { {
        (int) Model::FET76, (int) Model::Opto2A, (int) Model::Opto3A, (int) Model::TubeOneB,
        (int) Model::BusCompG, (int) Model::A2500, (int) Model::DB160X, (int) Model::PressorX,
        (int) Model::Fair670, (int) Model::VariMuV, (int) Model::ShadowMaster, (int) Model::N3309,
        (int) Model::ZenerX, (int) Model::ProCX, (int) Model::RCRen, (int) Model::RCVox } };
    return o;
}

juce::Rectangle<float> ModelBrowser::box() const
{
    return { 46.0f, 4.0f, 4 * 144.0f + 3 * 13.0f + 30.0f, 4 * (48.0f + 30.0f) + 3 * 6.0f + 26.0f };
}

juce::Rectangle<float> ModelBrowser::tile (int i) const
{
    const auto b = box();
    return { b.getX() + 15 + (float) (i % 4) * (144 + 13), b.getY() + 13 + (float) (i / 4) * (48 + 30 + 6), 144.0f, 48.0f + 28.0f };
}

void ModelBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.5f));
    const auto b = box();
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 14, { 3, 5 }).drawForRectangle (g, b.toNearestInt());
    g.setColour (juce::Colour (0xfc141210));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (look::goldDim);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);
    const int cur = (int) proc.model();
    for (int i = 0; i < numModels; ++i)
    {
        const int m = order()[(size_t) i];
        const auto t = tile (i);
        const bool sel = m == cur;
        if (sel || i == hover)
        {
            g.setColour (sel ? juce::Colour (0xff3c2c16) : juce::Colour (0xff221e1a));
            g.fillRoundedRectangle (t.expanded (4), 5.0f);
            if (sel) { g.setColour (look::gold); g.drawRoundedRectangle (t.expanded (4), 5.0f, 1.5f); }
        }
        const auto img = look::image (juce::String (info ((Model) m).id) + "_thumb.jpg");
        g.setOpacity (1.0f);
        g.drawImage (img, t.withHeight (48.0f), juce::RectanglePlacement::centred);
        g.setColour (sel ? juce::Colour (0xffffd68c) : look::text);
        g.setFont (look::font (13.5f, 700));
        g.drawText (info ((Model) m).name, t.withTrimmedTop (51).withHeight (15), juce::Justification::left);
        g.setColour (look::label);
        g.setFont (look::font (9.0f, 600));
        g.drawText (juce::String::fromUTF8 (info ((Model) m).family), t.withTrimmedTop (66).withHeight (11), juce::Justification::left);
    }
}

void ModelBrowser::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (int i = 0; i < numModels; ++i) if (tile (i).expanded (4).contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint(); }
}

void ModelBrowser::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < numModels; ++i)
        if (tile (i).expanded (4).contains (e.position))
        {
            proc.setModel ((Model) order()[(size_t) i]);
            break;
        }
    close();
}

bool ModelBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    return false;
}
