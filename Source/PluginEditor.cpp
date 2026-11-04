#include "PluginEditor.h"

using namespace comp;

EditorContent::EditorContent (CompProcessor& p, std::function<void (float)> setScale)
    : proc (p),
      bar (p, [this] { openBrowser(); }, std::move (setScale)),
      panel (p)
{
    addAndMakeVisible (bar);
    addAndMakeVisible (panel);
    setSize (W, H);
}

void EditorContent::resized()
{
    auto r = getLocalBounds();
    bar.setBounds (r.removeFromTop (TopBar::logicalHeight));
    panel.setBounds (r);
    if (browser) browser->setBounds (getLocalBounds().withTrimmedTop (TopBar::logicalHeight - 4));
}

void EditorContent::openBrowser()
{
    if (browser) { browser.reset(); return; }
    browser = std::make_unique<ModelBrowser> (proc, [this] {
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<EditorContent> (this)] {
            if (sp) { sp->browser.reset(); sp->repaint(); }
        });
    });
    addAndMakeVisible (*browser);
    resized();
    browser->grabKeyboardFocus();
}

void EditorContent::refreshModel()
{
    panel.setModel (proc.model());
    bar.repaint();
    if (browser) browser->repaint();
}

CompEditor::CompEditor (CompProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p, [this] (float s) { applyScale (s); })
{
    addAndMakeVisible (content);
    proc.apvts.addParameterListener (spec (pModel).id, this);
    applyScale (proc.uiScale);
}

CompEditor::~CompEditor()
{
    proc.apvts.removeParameterListener (spec (pModel).id, this);
}

void CompEditor::applyScale (float s)
{
    proc.uiScale = juce::jlimit (0.6f, 2.0f, s);
    content.setTransform (juce::AffineTransform::scale (proc.uiScale));
    setSize (juce::roundToInt (EditorContent::W * proc.uiScale), juce::roundToInt (EditorContent::H * proc.uiScale));
}

void CompEditor::resized()
{
    content.setBounds (0, 0, EditorContent::W, EditorContent::H);
}
