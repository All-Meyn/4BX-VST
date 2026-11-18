#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// Building blocks for the hardware-style faceplate.
namespace triexpand::ui
{

namespace colours
{
    inline const juce::Colour faceplate  { 0xff1c1c1e };
    inline const juce::Colour faceplate2 { 0xff151517 };
    inline const juce::Colour legend     { 0xffe4e4e4 };
    inline const juce::Colour bezel      { 0xff0c0c0d };
    inline const juce::Colour ledRed     { 0xffff3b2a };
    inline const juce::Colour ledYellow  { 0xffd9e04c };
    inline const juce::Colour buttonFace { 0xffd6d6d3 };
    inline const juce::Colour rearPanel  { 0xff2a2a2c };
}

// One rectangular LED. `amount` is how lit it is (0..1); `display` is the
// global brightness from the Display slider (0..1).
void drawLed (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour, float amount, float display);

// Legend text in the faceplate's style (bold, slightly spaced capitals).
void drawLegend (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                 float height, juce::Justification just = juce::Justification::centred,
                 juce::Colour colour = colours::legend);

// Light-grey rectangular push button.
class PanelButton final : public juce::Button
{
public:
    explicit PanelButton (const juce::String& name = {}) : juce::Button (name) {}
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
};

// A vertical column of five LEDs that is also a vertical slider:
// drag (relative) or scroll to change, double-click to type a value.
// Each LED stands for one value; LEDs between the current value's neighbours
// light partially, so in-between settings are visible.
class LedColumn final : public juce::Slider
{
public:
    LedColumn (std::array<double, 5> ledValuesBottomToTop, const float& displayLevel);
    ~LedColumn() override;

    void setStepSize (double s) noexcept { stepSize = s; }
    void step (int direction);

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void closeEditor (bool commit);

    std::array<double, 5> ledValues;
    const float& display;
    double stepSize = 1.0;
    std::unique_ptr<juce::TextEditor> valueEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LedColumn)
};

// Silver trim pot with a screwdriver slot, and a vertical "Display" fader cap.
class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

} // namespace triexpand::ui
