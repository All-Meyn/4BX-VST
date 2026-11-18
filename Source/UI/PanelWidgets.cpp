#include "PanelWidgets.h"

namespace fourbx::ui
{

void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float amount, float display)
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    const float lit = amount * (0.25f + 0.75f * display);

    // Unlit lens still shows a little colour, as on the real panel.
    const auto unlit = colour.withMultipliedSaturation (0.55f).withMultipliedBrightness (0.28f);
    const auto face = unlit.interpolatedWith (colour, lit);

    if (lit > 0.02f)
    {
        g.setColour (colour.withAlpha (0.28f * lit));
        g.fillRoundedRectangle (r.expanded (1.6f), 2.0f);
    }

    g.setColour (face);
    g.fillRoundedRectangle (r, 1.0f);

    // Lens highlight.
    g.setColour (juce::Colours::white.withAlpha (0.06f + 0.14f * lit));
    g.fillRect (r.withHeight (r.getHeight() * 0.35f).reduced (1.0f, 0.5f));
}

void drawLegend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                 float height, juce::Justification just, juce::Colour colour)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (height, juce::Font::bold)).withExtraKerningFactor (0.06f));
    g.drawText (text, area, just, false);
}

//==============================================================================
void PanelButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool pressed = isDown || getToggleState();

    // Drop shadow / recess.
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 1.5f);

    auto face = colours::buttonFace;
    if (pressed)            face = face.darker (0.25f);
    else if (isHighlighted) face = face.brighter (0.06f);

    g.setGradientFill (juce::ColourGradient (face.brighter (0.12f), r.getX(), r.getY(),
                                             face.darker (0.18f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (pressed ? r.translated (0.0f, 0.6f) : r, 1.5f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 1.5f, 0.6f);
}

//==============================================================================
LedColumn::LedColumn (std::array<double, 5> values, const float& displayLevel)
    : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox),
      ledValues (values), display (displayLevel)
{
    setSliderSnapsToMousePosition (false); // a stray click must not jump the value
    setMouseDragSensitivity (150);
    setRepaintsOnMouseActivity (false);
}

LedColumn::~LedColumn()
{
    if (valueEditor != nullptr)
        if (auto* p = valueEditor->getParentComponent())
            p->removeChildComponent (valueEditor.get());
}

void LedColumn::step (int direction)
{
    setValue (getValue() + direction * stepSize, juce::sendNotificationSync);
}

void LedColumn::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::bezel);
    g.fillRoundedRectangle (r, 1.5f);

    const double v = getValue();
    const double spacing = std::abs (ledValues[1] - ledValues[0]);
    const float ledH = (r.getHeight() - 4.0f) / 5.0f;

    for (int i = 0; i < 5; ++i)
    {
        const float amount = (float) juce::jlimit (0.0, 1.0, 1.0 - std::abs (v - ledValues[(size_t) i]) / spacing);
        auto led = juce::Rectangle<float> (r.getX() + 2.5f, r.getBottom() - 2.0f - ledH * (float) (i + 1),
                                           r.getWidth() - 5.0f, ledH).reduced (0.0f, 1.6f);
        drawLed (g, led, colours::ledRed, amount, display);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (colours::legend.withAlpha (0.4f));
        g.drawRoundedRectangle (r.reduced (0.5f), 1.5f, 0.8f);
    }
}

void LedColumn::mouseDoubleClick (const juce::MouseEvent&)
{
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    valueEditor = std::make_unique<juce::TextEditor>();
    auto& ed = *valueEditor;
    ed.setJustification (juce::Justification::centred);
    ed.setFont (juce::FontOptions (11.0f));
    ed.setColour (juce::TextEditor::backgroundColourId, colours::bezel);
    ed.setColour (juce::TextEditor::textColourId, colours::legend);
    ed.setColour (juce::TextEditor::outlineColourId, colours::legend.withAlpha (0.5f));
    ed.setText (getTextFromValue (getValue()), false);
    ed.selectAll();

    const auto b = getBounds();
    ed.setBounds (b.getCentreX() - 30, b.getCentreY() - 9, 60, 18);
    parent->addAndMakeVisible (ed);
    ed.grabKeyboardFocus();

    // Close asynchronously: the editor must not be deleted from inside its own callback.
    juce::Component::SafePointer<LedColumn> safe (this);
    ed.onReturnKey = [safe] { juce::MessageManager::callAsync ([safe] { if (safe) safe->closeEditor (true); }); };
    ed.onEscapeKey = [safe] { juce::MessageManager::callAsync ([safe] { if (safe) safe->closeEditor (false); }); };
    ed.onFocusLost = [safe] { juce::MessageManager::callAsync ([safe] { if (safe) safe->closeEditor (false); }); };
}

void LedColumn::closeEditor (bool commit)
{
    if (valueEditor == nullptr)
        return;

    if (commit)
        setValue (getValueFromText (valueEditor->getText()), juce::sendNotificationSync);

    if (auto* p = valueEditor->getParentComponent())
        p->removeChildComponent (valueEditor.get());
    valueEditor.reset();
}

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::legend);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (body.expanded (1.5f).translated (0.0f, 1.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8e8), body.getX(), body.getY(),
                                             juce::Colour (0xff8a8a8a), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);

    // Screwdriver slot.
    const float angle = startAngle + pos * (endAngle - startAngle);
    juce::Path slot;
    slot.addRoundedRectangle (-radius * 0.8f, -radius * 0.12f, radius * 1.6f, radius * 0.24f, radius * 0.08f);
    g.setColour (juce::Colour (0xff2b2b2b));
    g.fillPath (slot, juce::AffineTransform::rotation (angle + juce::MathConstants<float>::halfPi).translated (c));

    // Scale ticks.
    g.setColour (colours::legend.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const auto p1 = c.getPointOnCircumference (radius + 3.0f, a);
        const auto p2 = c.getPointOnCircumference (radius + (i % 5 == 0 ? 7.0f : 5.0f), a);
        g.drawLine ({ p1, p2 }, 0.8f);
    }
}

void PanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();

    // Slot.
    const auto slot = juce::Rectangle<float> (4.0f, r.getHeight() + 6.0f).withCentre (r.getCentre());
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (slot, 1.5f);

    // Cap.
    const auto cap = juce::Rectangle<float> (r.getWidth(), 10.0f).withCentre ({ r.getCentreX(), pos });
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.0f), 1.5f);
    g.setGradientFill (juce::ColourGradient (colours::buttonFace.brighter (0.1f), cap.getX(), cap.getY(),
                                             colours::buttonFace.darker (0.3f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawLine (cap.getX() + 1.0f, cap.getCentreY(), cap.getRight() - 1.0f, cap.getCentreY(), 0.8f);
}

juce::Label* PanelLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (10.0f));
    return l;
}

} // namespace fourbx::ui
