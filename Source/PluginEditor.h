#pragma once

#include "PluginProcessor.h"

class FourBXAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FourBXAudioProcessorEditor (FourBXAudioProcessor&);
    ~FourBXAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FourBXAudioProcessor& processor;

    juce::Slider gainSlider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label gainLabel;
    // Declared after the slider so it is destroyed first.
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourBXAudioProcessorEditor)
};
