// Chènevières Comp — la façade d'un modèle : image de base, boutons rotatifs, boutons lumineux,
// voyants et aiguilles, tous liés aux paramètres
#pragma once
#include "PluginProcessor.h"

struct KnobDef
{
    const char* id; const char* param;
    float cx, cy, rs; int spriteW;
    std::vector<std::pair<float, float>> cal;   // (valeur affichée, angle en degrés, 0 = haut)
    bool log; float mapA, mapB;                 // affichée = mapA * paramètre + mapB
    float autoAngle;                            // cran AUTO en fin de course (NAN = aucun)
    float ptrIn, ptrLen, ptrW; juce::uint32 ptrColour;
    const char* fmt;
    std::vector<float> steps;                   // crans (valeurs affichées) ; vide = continu
};
struct ButtonsDef
{
    const char* id; const char* param; std::vector<float> values; std::vector<juce::Rectangle<int>> rects; int glow;
    bool toggle;                                          // un seul bouton qui s'allume / s'éteint
    std::vector<std::pair<const char*, float>> also;      // paramètres remis à une valeur au clic
    float off;                                            // valeur « éteint » d'un bouton seul (NAN = défaut)
    float litAbove;                                       // allumé au-dessus de cette valeur (NAN = égalité)
};
struct LedBarDef { const char* id; int channel; std::vector<std::pair<juce::Rectangle<int>, float>> leds; };
struct LedDef { const char* id; const char* param; juce::Rectangle<int> rect; };
struct NeedleDef { const char* id; int channel; float px, py, len; juce::Rectangle<int> face; std::vector<std::pair<float, float>> scale; const char* source; };
struct PanelDef
{
    const char* model = ""; int width = 0, height = 0;
    std::vector<KnobDef> knobs; std::vector<ButtonsDef> buttons; std::vector<LedDef> leds; std::vector<NeedleDef> needles; std::vector<LedBarDef> ledbars;
};

const PanelDef* panelFor (comp::Model m);

class PanelView : public juce::Component, private juce::Timer
{
public:
    explicit PanelView (CompProcessor&);
    ~PanelView() override;
    void setModel (comp::Model m);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int logicalWidth = 1086;
    void settleMeters() { for (int i = 0; i < 60; ++i) timerCallback(); }

private:
    void timerCallback() override;
    int paramIndex (const char* id) const;
    float knobAngle (const KnobDef&) const;
    void setKnobFromAngle (const KnobDef&, float angle);
    juce::String knobText (const KnobDef&) const;
    const KnobDef* knobAt (juce::Point<float>) const;
    float sc() const { return def ? (float) getWidth() / (float) def->width : 1.0f; }
    void paintGeneric (juce::Graphics&);

    CompProcessor& proc;
    comp::Model model = comp::Model::A2500;
    const PanelDef* def = nullptr;
    juce::Image base;
    std::map<juce::String, juce::Image> imgs;

    const KnobDef* dragKnob = nullptr;
    const KnobDef* hoverKnob = nullptr;
    float dragAngle = 0;
    juce::Point<float> lastDrag;
    float needle[2] { 0, 0 }, inLvl[2] { 40, 40 }, outLvl[2] { 40, 40 };
    std::map<juce::String, float> ui;          // réglages d'affichage (« @meter »…), non automatisés
    float getV (const char* param) const;
    void setV (const char* param, float v);

    // façade générique tant que le visuel n'est pas prêt
    juce::OwnedArray<juce::Slider> generic;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> genericAtt;
    juce::StringArray genericNames;
};
