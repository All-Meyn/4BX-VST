#pragma once

#include "PanelWidgets.h"
#include "../PluginProcessor.h"

namespace triexpand::ui
{

// Both panels are laid out in a fixed coordinate space that the editor scales.
// 3 bands: 1000 x 210 (the hardware faceplate's proportions). More bands add
// gain-change meter rows, so the faceplate grows taller; everything except the
// meter block is then centred vertically (sideOffset).
inline constexpr int   panelWidth      = 1000;
inline constexpr float meterRowSpacing = constants::numBands == 3 ? 29.0f : 20.0f;
inline constexpr float firstMeterRowY  = 34.0f;
inline constexpr float lastMeterRowY   = firstMeterRowY + meterRowSpacing * (float) (constants::numBands - 1);
inline constexpr float impactRowY      = lastMeterRowY + 50.0f;
inline constexpr int   panelHeight     = (int) (impactRowY + 14.0f + 54.0f);
inline constexpr float sideOffset      = (float) (panelHeight - 210) * 0.5f;
static_assert (constants::numBands != 3 || panelHeight == 210, "3-band layout must stay 1000 x 210");

class FrontPanel final : public juce::Component
{
public:
    FrontPanel (TriExpandAudioProcessor&, float& displayLevel);
    ~FrontPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Called from the editor's timer (message thread).
    void setMeterValues (const std::array<float, constants::numBands>& expansionDb, float impactDb);

    std::function<void()> onShowRear;
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

    std::array<float, constants::numBands> meterExpDb {};
    float meterImpactDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrontPanel)
};

class RearPanel final : public juce::Component
{
public:
    RearPanel (TriExpandAudioProcessor&, const float& displayLevel);
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

} // namespace triexpand::ui
