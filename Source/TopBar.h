// Chènevières Comp — barre du haut (logo, modèle, presets, A/B, bypass, réglages, Pistache)
// et menu des modèles
#pragma once
#include "PluginProcessor.h"

class ModelBrowser : public juce::Component
{
public:
    ModelBrowser (CompProcessor&, std::function<void()> onClose);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    juce::Rectangle<float> box() const;

private:
    juce::Rectangle<float> tile (int i) const;
    static const std::array<int, comp::numModels>& order();
    CompProcessor& proc;
    std::function<void()> close;
    int hover = -1;
};

class TopBar : public juce::Component, private juce::Timer
{
public:
    TopBar (CompProcessor&, std::function<void()> openBrowser, std::function<void (float)> setScale);
    ~TopBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr int logicalHeight = 66;

private:
    enum Zone { None, ModelPrev, ModelBox, ModelNext, PresetPrev, PresetBox, PresetNext, SlotA, SlotB, Copy, Bypass, Gear, numZones };
    void timerCallback() override { repaint(); }
    Zone zoneAt (juce::Point<float>) const;
    void stepModel (int dir);
    void stepPreset (int dir);
    void showPresetMenu();
    void showGearMenu();

    CompProcessor& proc;
    std::function<void()> openBrowser;
    std::function<void (float)> setScale;
    std::array<juce::Rectangle<float>, numZones> zones;
    Zone hover = None;
    juce::Image wordmark, beagle;
};
