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
inline constexpr float meterRowSpacing = constants::numBands == 3 ? 29.0f : (constants::numBands == 4 ? 24.0f : 20.0f);
inline constexpr float firstMeterRowY  = 34.0f;
inline constexpr float lastMeterRowY   = firstMeterRowY + meterRowSpacing * (float) (constants::numBands - 1);
inline constexpr float impactRowY      = lastMeterRowY + 50.0f;
// Hepta-Expand adds a Bass Restoration meter row under the Impact row.
inline constexpr bool  hasBassRow      = constants::numBassBands > 0;
inline constexpr bool  hasHeadroomUi   = constants::hasAutoHeadroom; // OVER LED + Auto Headroom switch
inline constexpr bool  hasAutoTransitionUi = constants::hasAutoTransition; // AUTO LED + Auto Transition switch
inline constexpr float bassRowY        = impactRowY + 36.0f;
inline constexpr float lastLegendY     = (hasBassRow ? bassRowY : impactRowY) + 14.0f;
inline constexpr int   panelHeight     = (int) (lastLegendY + 54.0f);
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
    void setMeterValues (const std::array<float, constants::numBands>& expansionDb, float impactDb, float bassDb = 0.0f);
    void setOverLit (bool lit);
    void setAutoTransitionLit (bool lit);

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
    Stepped bass; // Bass Restoration (only shown when hasBassRow)

    PanelButton rearButton, bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::Slider displaySlider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };

    std::array<float, constants::numBands> meterExpDb {};
    float meterImpactDb = 0.0f;
    float meterBassDb = 0.0f;
    bool overLit = false;
    bool autoTransitionLit = false;

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

    // Auto Headroom trim readout (dB, <= 0), from the editor's timer.
    void setHeadroomTrimDb (float db);

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
    juce::Slider bassPot    { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow }; // only when hasBassRow
    PanelButton softClipButton, frontButton, gpuButton, autoHeadroomButton, autoTransitionButton;
    float headroomTrimDb = 0.0f;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment, hfAttachment, bassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> softClipAttachment, autoHeadroomAttachment, autoTransitionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RearPanel)
};

} // namespace triexpand::ui
