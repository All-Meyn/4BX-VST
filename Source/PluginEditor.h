#pragma once

#include "PluginProcessor.h"

class FourBXAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit FourBXAudioProcessorEditor (FourBXAudioProcessor&);
    ~FourBXAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawGainMeter (juce::Graphics&, juce::Rectangle<int> area, const char* label, float expDb) const;
    void drawImpactMeter (juce::Graphics&, juce::Rectangle<int> area, float impactDb) const;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    FourBXAudioProcessor& processor;

    std::array<Knob, 6> knobs;
    juce::ToggleButton bypassButton { "Bypass" }, softClipButton { "Soft Clip" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, softClipAttachment;

    // Meter values shown, with simple decay so short peaks stay visible.
    std::array<float, fourbx::constants::numBands> shownExpDb {};
    float shownImpactDb = 0.0f;

    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourBXAudioProcessorEditor)
};
