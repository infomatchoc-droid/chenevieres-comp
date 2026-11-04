#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace comp;

static juce::NormalisableRange<float> rangeFor (const ParamSpec& s)
{
    juce::NormalisableRange<float> r (s.min, s.max);
    if (s.centre > 0.0f) r.setSkewForCentre (s.centre);
    return r;
}

juce::AudioProcessorValueTreeState::ParameterLayout CompProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < numParams; ++i)
    {
        const auto& s = spec (i);
        const juce::ParameterID id { s.id, 1 };
        switch (s.kind)
        {
            case Kind::Float:
                layout.add (std::make_unique<juce::AudioParameterFloat> (id, s.name, rangeFor (s), s.def,
                    juce::AudioParameterFloatAttributes().withLabel (s.unit)));
                break;
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (id, s.name, s.def >= 0.5f));
                break;
            case Kind::Int:
                layout.add (std::make_unique<juce::AudioParameterInt> (id, s.name, (int) s.min, (int) s.max, (int) s.def));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (id, s.name,
                    juce::StringArray::fromTokens (s.choices, "|", ""), (int) s.def));
                break;
        }
    }
    return layout;
}

CompProcessor::CompProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "COMP", createLayout())
{
    for (int i = 0; i < numParams; ++i)
    {
        params[(size_t) i] = apvts.getParameter (spec (i).id);
        raw[(size_t) i] = apvts.getRawParameterValue (spec (i).id);
    }
    apvts.addParameterListener (spec (pModel).id, this);
    lastModel = (int) model();
}

CompProcessor::~CompProcessor()
{
    apvts.removeParameterListener (spec (pModel).id, this);
}

bool CompProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void CompProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int ch = juce::jmax (1, getMainBusNumInputChannels());
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) ch, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    engine.prepare (sampleRate * 2.0, ch);
    scUp.setSize (2, samplesPerBlock * 2 + 8);
    setLatencySamples ((int) std::lround (oversampling->getLatencyInSamples()));
}

Values CompProcessor::values() const
{
    Values v;
    for (int i = 0; i < numParams; ++i) v[i] = raw[(size_t) i]->load();
    return v;
}

void CompProcessor::setValue (int p, float v)
{
    if (auto* pr = params[(size_t) p])
    {
        const float n = pr->convertTo0to1 (v);
        if (std::abs (pr->getValue() - n) > 1e-7f)
        {
            pr->beginChangeGesture();
            pr->setValueNotifyingHost (n);
            pr->endChangeGesture();
        }
    }
}

void CompProcessor::setValues (const Values& v, bool includeModel)
{
    for (int i = includeModel ? 0 : 1; i < numParams; ++i)
        if (i != pBypass) setValue (i, v[i]);
}

void CompProcessor::setModel (Model m)
{
    setValue (pModel, (float) (int) m);
}

void CompProcessor::parameterChanged (const juce::String&, float)
{
    // peut venir du fil audio (automation) : le reste se fait sur le fil de l'interface
    triggerAsyncUpdate();
}

void CompProcessor::handleAsyncUpdate()
{
    const int now = (int) model();
    if (now == lastModel) return;
    Values v = values();
    const Values before = v;
    if (lastModel >= 0) onModelChanged (v, (Model) lastModel, (Model) now);
    for (int i = pPunch; i < numParams; ++i)
        if (v[i] != before[i]) setValue (i, v[i]);
    lastModel = now;
}

void CompProcessor::selectSlot (int slot)
{
    if (slot == abSlot) return;
    slots[abSlot] = apvts.copyState();
    abSlot = slot;
    if (slots[slot].isValid()) apvts.replaceState (slots[slot].createCopy());
}

void CompProcessor::copyAToB()
{
    slots[abSlot] = apvts.copyState();
    slots[1 - abSlot] = slots[abSlot].createCopy();
}

void CompProcessor::loadPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    presetIndex = index;
    setValues (applyPreset (values(), list[(size_t) index]), false);
}

static float blockLevelDb (const float* x, int n)
{
    float m = 0.0f;
    for (int i = 0; i < n; ++i) m = juce::jmax (m, std::abs (x[i]));
    return juce::Decibels::gainToDecibels (m, -100.0f);
}

void CompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, getMainBusNumInputChannels());
    const int n = buffer.getNumSamples();
    for (int c = getMainBusNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);
    if (numCh == 0 || n == 0 || oversampling == nullptr) return;

    auto main = getBusBuffer (buffer, true, 0);
    const Values v = values();
    Eff e = interpret (model(), v);
    if (v.on (pBypass))
    {
        e.mix = 0.0f; e.outputDb = 0.0f; e.inputDb = 0.0f; e.autoGain = false;
    }
    engine.setParams (e);

    for (int c = 0; c < numCh; ++c) inDb[c].store (blockLevelDb (main.getReadPointer (c), n));

    // sidechain externe : suréchantillonné par répétition (sert seulement à la détection)
    const float* scPtr[2] = { nullptr, nullptr };
    int numSc = 0;
    if (e.scExternal && getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        numSc = juce::jmin (2, sc.getNumChannels());
        if (scUp.getNumSamples() < n * 2) scUp.setSize (2, n * 2, false, false, true);
        for (int c = 0; c < numSc; ++c)
        {
            const float* s = sc.getReadPointer (c);
            float* d = scUp.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[2 * i] = d[2 * i + 1] = s[i];
            scPtr[c] = d;
        }
    }

    juce::dsp::AudioBlock<float> block (main.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = oversampling->processSamplesUp (block);
    float* chs[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
    engine.process (chs, numCh, (int) up.getNumSamples(), numSc > 0 ? scPtr : nullptr, numSc);
    oversampling->processSamplesDown (block);

    for (int c = 0; c < numCh; ++c)
    {
        grDb[c].store (engine.lastGr (c));
        outDb[c].store (blockLevelDb (main.getReadPointer (c), n));
    }
    if (numCh == 1) { grDb[1].store (grDb[0].load()); inDb[1].store (inDb[0].load()); outDb[1].store (outDb[0].load()); }
}

void CompProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale, nullptr);
    state.setProperty ("abSlot", abSlot, nullptr);
    state.setProperty ("preset", presetIndex, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void CompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType (apvts.state.getType()))
        {
            uiScale = (float) state.getProperty ("uiScale", 1.0f);
            abSlot = (int) state.getProperty ("abSlot", 0);
            presetIndex = (int) state.getProperty ("preset", 0);
            apvts.replaceState (state);
            lastModel = (int) model();
        }
    }
}

juce::AudioProcessorEditor* CompProcessor::createEditor() { return new CompEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CompProcessor(); }
