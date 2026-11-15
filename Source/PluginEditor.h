#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
// Look & feel: ritual "seal" knobs - ticked ring, glowing value arc, dark sigil body.
class PsychickLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PsychickLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float startAngle, float endAngle, juce::Slider&) override;
    juce::Font getLabelFont(juce::Label&) override;
};

//==============================================================================
class PsychickEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PsychickEditor(PsychickProcessor&);
    ~PsychickEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Group
    {
        juce::String title, sub;
        std::vector<int> knobs;
        juce::Colour accent;
        float weight = 1.f;
        bool eye = false;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    Knob& addKnob(const char* paramId, const juce::String& name, juce::Colour accent);

    PsychickProcessor& proc;
    PsychickLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::vector<Group>> rows;

    juce::ToggleButton limitButton { "SAFETY LIMITER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limitAttachment;

    juce::Image skull, scanlines;
    float phase = 0.f, level = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsychickEditor)
};
