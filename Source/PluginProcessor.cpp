#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using Range = juce::NormalisableRange<float>;

static std::unique_ptr<juce::AudioParameterFloat> mk(const juce::String& id, const juce::String& name,
                                                     Range r, float def, const juce::String& suffix = {})
{
    return std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withLabel(suffix));
}

APVTS::ParameterLayout PsychickProcessor::createLayout()
{
    APVTS::ParameterLayout l;

    // TRANSMUTE
    l.add(mk("pitch", "Pitch", Range(-24.f, 24.f, 0.01f), 0.f, "st"));
    l.add(mk("grain", "Grain", Range(20.f, 150.f, 0.1f, 0.6f), 50.f, "ms"));
    // SIGIL
    l.add(mk("ringHz", "Ring Freq", Range(20.f, 4000.f, 0.1f, 0.35f), 220.f, "Hz"));
    l.add(mk("ringMix", "Ring Mix", Range(0.f, 1.f, 0.001f), 0.f));
    // FIRE
    l.add(mk("drive", "Drive", Range(0.f, 1.f, 0.001f), 0.f));
    l.add(mk("fold", "Fold", Range(0.f, 1.f, 0.001f), 0.f));
    // ELIXIR
    l.add(mk("acidCut", "Acid Cutoff", Range(50.f, 12000.f, 1.f, 0.3f), 1200.f, "Hz"));
    l.add(mk("acidRes", "Acid Reso", Range(0.f, 1.f, 0.001f), 0.35f));
    l.add(mk("acidRate", "Acid Rate", Range(0.02f, 12.f, 0.01f, 0.4f), 0.5f, "Hz"));
    l.add(mk("acidDepth", "Acid Depth", Range(0.f, 1.f, 0.001f), 0.4f));
    l.add(mk("acidMix", "Acid Amount", Range(0.f, 1.f, 0.001f), 0.f));
    // VORTEX
    l.add(mk("swirlRate", "Swirl Rate", Range(0.02f, 8.f, 0.01f, 0.4f), 0.4f, "Hz"));
    l.add(mk("swirlDepth", "Swirl Depth", Range(0.f, 1.f, 0.001f), 0.6f));
    l.add(mk("swirlFb", "Swirl Feedback", Range(0.f, 0.95f, 0.001f), 0.4f));
    l.add(mk("swirlMix", "Swirl Amount", Range(0.f, 1.f, 0.001f), 0.f));
    // SCRYING
    l.add(mk("crysMix", "Crystal Amount", Range(0.f, 1.f, 0.001f), 0.f));
    l.add(mk("crysFb", "Crystal Feedback", Range(0.f, 0.92f, 0.001f), 0.5f));
    l.add(mk("crysPitch", "Crystal Pitch", Range(-12.f, 24.f, 0.01f), 12.f, "st"));
    l.add(mk("shatter", "Shatter", Range(0.f, 1.f, 0.001f), 0.f));
    // ASTRAL
    l.add(mk("width", "Width", Range(0.f, 3.f, 0.001f), 1.f));
    l.add(mk("spread", "Spread", Range(0.f, 1.f, 0.001f), 0.f));
    // MACRO / OUTPUT
    l.add(mk("trip", "TRIP", Range(0.f, 1.f, 0.001f), 0.f));
    l.add(mk("mix", "Dry/Wet", Range(0.f, 1.f, 0.001f), 1.f));
    l.add(mk("out", "Output", Range(-24.f, 12.f, 0.1f), 0.f, "dB"));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "limit", 1 }, "Safety Limiter", true));
    return l;
}

PsychickProcessor::PsychickProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PSYCHICK", createLayout())
{
}

bool PsychickProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void PsychickProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    meterIn = meterOut = 0.f;
}

void PsychickProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    auto get = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };

    psy::Params p;
    p.pitchSt = get("pitch");       p.grainMs = get("grain");
    p.ringHz = get("ringHz");       p.ringMix = get("ringMix");
    p.drive = get("drive");         p.fold = get("fold");
    p.acidCut = get("acidCut");     p.acidRes = get("acidRes");
    p.acidRate = get("acidRate");   p.acidDepth = get("acidDepth");  p.acidMix = get("acidMix");
    p.swirlRate = get("swirlRate"); p.swirlDepth = get("swirlDepth");
    p.swirlFb = get("swirlFb");     p.swirlMix = get("swirlMix");
    p.crysMix = get("crysMix");     p.crysFb = get("crysFb");
    p.crysPitchSt = get("crysPitch"); p.shatter = get("shatter");
    p.width = get("width");         p.spread = get("spread");
    p.trip = get("trip");           p.mix = get("mix");  p.outDb = get("out");
    p.limit = get("limit") > 0.5f;

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getWritePointer(1);

    float pin = 0.f;
    for (int i = 0; i < n; ++i)
        pin = juce::jmax(pin, std::abs(l[i]), std::abs(r[i]));

    engine.process(l, r, n, p);

    float pout = 0.f;
    for (int i = 0; i < n; ++i)
        pout = juce::jmax(pout, std::abs(l[i]), std::abs(r[i]));

    // simple peak meters with decay, consumed by the GUI
    meterIn  = juce::jmax(pin,  meterIn  * 0.9f);
    meterOut = juce::jmax(pout, meterOut * 0.9f);
    inLevel.store(meterIn);
    outLevel.store(meterOut);
}

juce::AudioProcessorEditor* PsychickProcessor::createEditor() { return new PsychickEditor(*this); }

void PsychickProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PsychickProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PsychickProcessor(); }
