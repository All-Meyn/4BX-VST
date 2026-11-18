#pragma once

#include "PluginProcessor.h"
#include "UI/Panels.h"

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
    void showRear (bool rear);
    void updateRenderingEngine();

    FourBXAudioProcessor& processor;

    // LED brightness from the Display slider; UI-only, saved with the session
    // as a property of the plugin state (not a host-automatable parameter).
    float displayLevel = 1.0f;

    // true = Direct2D (GPU) drawing, false = JUCE software renderer. Windows only;
    // saved with the session as a state property, like displayLevel.
    bool gpuRendering = true;

    juce::Component content; // fixed 1000 x 210 space, scaled to the window
    fourbx::ui::FrontPanel front;
    fourbx::ui::RearPanel rear;
    juce::TooltipWindow tooltips { this, 600 };

    // TEMPORARY diagnostics strip.
    juce::Label diagLabel;
    int diagTicks = 0, diagTicksAtLast = 0, diagPaintsAtLast = 0;
    uint32_t diagBlocksAtLast = 0;
    juce::uint32 diagLastMs = 0;
    void updateDiagnostics();

    // Meter values shown, with decay so short peaks stay visible.
    std::array<float, fourbx::constants::numBands> shownExpDb {};
    float shownImpactDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FourBXAudioProcessorEditor)
};
