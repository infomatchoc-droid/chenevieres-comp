#pragma once
#include <JuceHeader.h>
#include "CompEngine.h"
#include "Presets.h"

class CompProcessor : public juce::AudioProcessor,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater
{
public:
    CompProcessor();
    ~CompProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chenevieres Comp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- pour l'interface ---
    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param (int p) const { return params[(size_t) p]; }
    float value (int p) const { return raw[(size_t) p]->load(); }
    comp::Values values() const;
    void setValue (int p, float v);                 // en unités réelles, notifie le DAW
    void setValues (const comp::Values& v, bool includeModel);
    comp::Model model() const { return (comp::Model) juce::jlimit (0, comp::numModels - 1, (int) std::lround (value (comp::pModel))); }
    void setModel (comp::Model m);

    // comparaison A/B
    int abSlot = 0;
    void selectSlot (int slot);
    void copyAToB();

    int presetIndex = 0;
    void loadPreset (int index);

    float uiScale = 1.0f;

    // mesures (dB) lues par l'interface
    std::atomic<float> grDb[2] { 0.0f, 0.0f }, inDb[2] { -100.0f, -100.0f }, outDb[2] { -100.0f, -100.0f };

private:
    void parameterChanged (const juce::String& id, float v) override;
    void handleAsyncUpdate() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::array<juce::RangedAudioParameter*, comp::numParams> params {};
    std::array<std::atomic<float>*, comp::numParams> raw {};

    comp::Engine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> scUp;
    int lastModel = -1;
    std::atomic<int> pendingModelFrom { -1 };
    juce::ValueTree slots[2];
};
