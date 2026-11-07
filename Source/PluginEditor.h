#pragma once
#include "PluginProcessor.h"
#include "PanelView.h"
#include "TopBar.h"

// Contenu à taille fixe (1086 x 428), agrandi par une transformation
class EditorContent : public juce::Component
{
public:
    explicit EditorContent (CompProcessor&, std::function<void (float)> setScale);
    void resized() override;
    void openBrowser();
    void refreshModel();
    PanelView& getPanel() { return panel; }

    static constexpr int W = PanelView::logicalWidth;
    int heightNow() const { return TopBar::logicalHeight + panel.preferredHeight(); }
    std::function<void()> onSizeChange;

private:
    CompProcessor& proc;
    TopBar bar;
    PanelView panel;
    std::unique_ptr<ModelBrowser> browser;
};

class CompEditor : public juce::AudioProcessorEditor, private juce::AudioProcessorValueTreeState::Listener, private juce::AsyncUpdater
{
public:
    explicit CompEditor (CompProcessor&);
    ~CompEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    void applyScale (float s);

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { content.refreshModel(); applyScale (proc.uiScale); }

    CompProcessor& proc;
    EditorContent content;
};
