#pragma once

#include "PluginProcessor.h"
#include "UI/Panels.h"

class TriExpandAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit TriExpandAudioProcessorEditor (TriExpandAudioProcessor&);
    ~TriExpandAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showRear (bool rear);
    void updateRenderingEngine();

    TriExpandAudioProcessor& processor;

    // LED brightness from the Display slider; UI-only, saved with the session
    // as a property of the plugin state (not a host-automatable parameter).
    float displayLevel = 1.0f;

    // true = Direct2D (GPU) drawing, false = JUCE software renderer. Windows only.
    // Defaults to software in Audacity (see PluginEditor.cpp); a choice made with
    // the switch is saved with the session as a state property, like displayLevel.
    bool gpuRendering = true;

    juce::Component content; // fixed 1000 x 210 space, scaled to the window
    triexpand::ui::FrontPanel front;
    triexpand::ui::RearPanel rear;
    juce::TooltipWindow tooltips { this, 600 };

    // Meter values shown, with decay so short peaks stay visible.
    std::array<float, triexpand::constants::numBands> shownExpDb {};
    float shownImpactDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriExpandAudioProcessorEditor)
};
