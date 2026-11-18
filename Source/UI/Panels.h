#pragma once

#include "PanelWidgets.h"
#include "../PluginProcessor.h"

namespace fourbx::ui
{

// Both panels are laid out in a fixed 1000 x 210 coordinate space
// (the hardware faceplate's proportions); the editor scales them.
inline constexpr int panelWidth  = 1000;
inline constexpr int panelHeight = 210;

class FrontPanel final : public juce::Component
{
public:
    FrontPanel (FourBXAudioProcessor&, float& displayLevel);
    ~FrontPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Called from the editor's timer (message thread).
    void setMeterValues (const std::array<float, 3>& expansionDb, float impactDb);

    std::function<void()> onShowRear;
    int diagPaints = 0; // TEMPORARY diagnostics
    std::function<void (float)> onDisplayChanged;

private:
    struct Stepped
    {
        Stepped (std::array<double, 5> leds, const float& display) : column (leds, display) {}
        LedColumn column;
        PanelButton down, up;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setUpStepped (Stepped&, juce::AudioProcessorValueTreeState&, const char* paramId, double step);
    void drawGainRow (juce::Graphics&, float y, float expansionDb) const;
    void drawImpactRow (juce::Graphics&, float y, float impactDb) const;
    void drawArrows (juce::Graphics&, const Stepped&) const;
    void drawColumnScale (juce::Graphics&, const LedColumn&, const std::array<const char*, 5>& labelsTopToBottom,
                          bool leftSide, float width) const;

    float& display;
    PanelLookAndFeel lnf;

    Stepped volume, expansion, transition, impact;

    PanelButton rearButton, bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::Slider displaySlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };

    std::array<float, 3> meterExpDb {};
    float meterImpactDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrontPanel)
};

class RearPanel final : public juce::Component
{
public:
    RearPanel (FourBXAudioProcessor&, const float& displayLevel);
    ~RearPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onShowFront;

    // GPU (Direct2D) vs software drawing switch. Only shown where the platform
    // offers both renderers (Windows).
    std::function<void (bool)> onGpuChanged;
    void setGpuSwitchState (bool gpuOn);
    void setGpuSwitchAvailable (bool available);

private:
    const float& display;
    PanelLookAndFeel lnf;

    juce::Slider releasePot { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Slider hfPot      { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    PanelButton softClipButton, frontButton, gpuButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment, hfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> softClipAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RearPanel)
};

} // namespace fourbx::ui
