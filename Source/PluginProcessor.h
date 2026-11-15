#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PsychickEngine.h"

class PsychickProcessor : public juce::AudioProcessor
{
public:
    PsychickProcessor();
    ~PsychickProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "THEE PSYCHICK MACHINE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; } // shimmer tail
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // read by the editor for animation
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    psy::Engine engine;
    float meterIn = 0.f, meterOut = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsychickProcessor)
};
