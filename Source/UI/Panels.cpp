#include "Panels.h"

namespace fourbx::ui
{
using IDs = FourBXAudioProcessor::ParamIDs;

namespace
{
    // Gain-change rows: 6 yellow LEDs (downward expansion, -20 dB at the far left)
    // and 6 red LEDs (upward expansion, +12 dB at the far right), as on the hardware.
    constexpr float yellowThresholdsDb[6] = { -18.0f, -14.0f, -10.0f, -6.0f, -3.0f, -1.0f }; // left -> centre
    constexpr float redThresholdsDb[6]    = { 1.0f, 3.0f, 5.0f, 7.0f, 9.0f, 11.0f };         // centre -> right

    constexpr float rowX = 312.0f, rowW = 208.0f;   // LED row extent
    constexpr float rowYs[3] = { 34.0f, 63.0f, 92.0f }; // HF, MF, LF (top to bottom)
    constexpr float impactRowY = 142.0f;
    constexpr float ledW = 14.0f, ledH = 6.0f;

    juce::Rectangle<float> rowLed (int index, float y) // index 0..11 left to right
    {
        const float slot = (rowW - 6.0f) / 12.0f;
        const float x = rowX + (float) index * slot + (index >= 6 ? 6.0f : 0.0f);
        return { x + (slot - ledW) * 0.5f, y - ledH * 0.5f, ledW, ledH };
    }

    void drawTriangle (juce::Graphics& g, juce::Point<float> c, bool up)
    {
        juce::Path p;
        const float s = 4.0f;
        if (up) p.addTriangle (c.x - s, c.y + s * 0.8f, c.x + s, c.y + s * 0.8f, c.x, c.y - s * 0.8f);
        else    p.addTriangle (c.x - s, c.y - s * 0.8f, c.x + s, c.y - s * 0.8f, c.x, c.y + s * 0.8f);
        g.setColour (colours::legend);
        g.fillPath (p);
    }
}

//==============================================================================
FrontPanel::FrontPanel (FourBXAudioProcessor& p, float& displayLevel)
    : display (displayLevel),
      volume     ({ -30.0, -20.0, -10.0, 0.0, 10.0 }, displayLevel),
      expansion  ({ 10.0, 20.0, 30.0, 40.0, 50.0 }, displayLevel),
      transition ({ -40.0, -30.0, -20.0, -10.0, 0.0 }, displayLevel),
      impact     ({ 20.0, 40.0, 60.0, 80.0, 100.0 }, displayLevel)
{
    setUpStepped (volume,     p.apvts, IDs::volume,     1.0);
    setUpStepped (expansion,  p.apvts, IDs::expansion,  1.0);
    setUpStepped (transition, p.apvts, IDs::transition, 1.0);
    setUpStepped (impact,     p.apvts, IDs::impact,     2.0);

    rearButton.setTooltip ("Rear panel: Impact Release, HF Transition, Soft Clip");
    rearButton.onClick = [this] { if (onShowRear) onShowRear(); };
    addAndMakeVisible (rearButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.onClick = [this] { repaint(); };
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        p.apvts, IDs::bypass, bypassButton);

    displaySlider.setLookAndFeel (&lnf);
    displaySlider.setRange (0.0, 1.0);
    displaySlider.setValue (display, juce::dontSendNotification);
    displaySlider.setSliderSnapsToMousePosition (false);
    displaySlider.setTooltip ("Display brightness");
    displaySlider.onValueChange = [this]
    {
        display = (float) displaySlider.getValue();
        if (onDisplayChanged) onDisplayChanged (display);
        repaint();
    };
    addAndMakeVisible (displaySlider);
}

FrontPanel::~FrontPanel()
{
    displaySlider.setLookAndFeel (nullptr);
}

void FrontPanel::setUpStepped (Stepped& s, juce::AudioProcessorValueTreeState& apvts, const char* paramId, double step)
{
    s.column.setStepSize (step);
    for (auto* b : { &s.down, &s.up })
    {
        b->setRepeatSpeed (400, 70);
        addAndMakeVisible (*b);
    }
    s.down.onClick = [&s] { s.column.step (-1); };
    s.up.onClick   = [&s] { s.column.step (+1); };
    addAndMakeVisible (s.column);
    s.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, s.column);
}

void FrontPanel::setMeterValues (const std::array<float, 3>& expansionDb, float impactDb)
{
    meterExpDb = expansionDb;
    meterImpactDb = impactDb;
    repaint (juce::Rectangle<int> ((int) rowX - 4, 26, (int) rowW + 8, 74));
    repaint (juce::Rectangle<int> ((int) rowX - 4, (int) impactRowY - 8, (int) rowW + 8, 16));
    repaint (bypassButton.getBounds().withY (104).withHeight (14)); // bypass LED (host automation)
}

void FrontPanel::drawGainRow (juce::Graphics& g, float y, float expDb) const
{
    for (int i = 0; i < 6; ++i)
        drawLed (g, rowLed (i, y), colours::ledYellow, expDb <= yellowThresholdsDb[i] ? 1.0f : 0.0f, display);
    for (int i = 0; i < 6; ++i)
        drawLed (g, rowLed (6 + i, y), colours::ledRed, expDb >= redThresholdsDb[i] ? 1.0f : 0.0f, display);
}

void FrontPanel::drawImpactRow (juce::Graphics& g, float y, float impactDb) const
{
    for (int i = 0; i < 12; ++i)
        drawLed (g, rowLed (i, y), colours::ledRed, impactDb >= (float) i + 0.5f ? 1.0f : 0.0f, display);
}

void FrontPanel::drawArrows (juce::Graphics& g, const Stepped& s) const
{
    drawTriangle (g, { (float) s.down.getBounds().getCentreX(), 107.0f }, false);
    drawTriangle (g, { (float) s.up.getBounds().getCentreX(),   107.0f }, true);
}

void FrontPanel::drawColumnScale (juce::Graphics& g, const LedColumn& col,
                                  const std::array<const char*, 5>& labels, bool leftSide, float width) const
{
    const auto b = col.getBounds().toFloat();
    const float ledH = (b.getHeight() - 4.0f) / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float cy = b.getY() + 2.0f + ledH * ((float) i + 0.5f);
        auto area = leftSide ? juce::Rectangle<float> (b.getX() - width - 4.0f, cy - 5.0f, width, 10.0f)
                             : juce::Rectangle<float> (b.getRight() + 5.0f, cy - 5.0f, width, 10.0f);
        drawLegend (g, labels[(size_t) i], area, 8.0f,
                    leftSide ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }
}

void FrontPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Faceplate.
    g.setGradientFill (juce::ColourGradient (colours::faceplate, 0.0f, 0.0f,
                                             colours::faceplate2, 0.0f, bounds.getBottom(), false));
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawLine (0.0f, 1.0f, bounds.getRight(), 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (0.0f, bounds.getBottom() - 1.0f, bounds.getRight(), bounds.getBottom() - 1.0f, 1.5f);

    // Badge.
    g.setColour (colours::legend);
    g.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)).withExtraKerningFactor (-0.02f));
    g.drawText ("4BX", juce::Rectangle<float> (34.0f, 26.0f, 110.0f, 46.0f), juce::Justification::centredLeft);

    // Rear-panel button legend.
    drawLegend (g, "REAR", { 26.0f, 152.0f, 50.0f, 10.0f }, 8.0f);
    drawLegend (g, "PANEL", { 26.0f, 162.0f, 50.0f, 10.0f }, 8.0f);

    // Volume & expansion columns.
    drawColumnScale (g, volume.column, { "+10dB", "0dB", "-10dB", "-20dB", "-30dB" }, false, 34.0f);
    drawColumnScale (g, expansion.column, { "50%", "40%", "30%", "20%", "10%" }, true, 24.0f);
    drawColumnScale (g, expansion.column, { "1.5/1", "1.4/1", "1.3/1", "1.2/1", "1.1/1" }, false, 28.0f);
    drawArrows (g, volume);
    drawArrows (g, expansion);
    drawLegend (g, "VOLUME",    { 135.0f, 156.0f, 66.0f, 10.0f }, 8.5f);
    drawLegend (g, "EXPANSION", { 210.0f, 156.0f, 72.0f, 10.0f }, 8.5f);

    // Gain-change rows.
    const char* rowNames[3] = { "HF", "MF", "LF" };
    for (int r = 0; r < 3; ++r)
    {
        drawLegend (g, rowNames[r], { rowX - 30.0f, rowYs[r] - 5.0f, 22.0f, 10.0f }, 8.5f, juce::Justification::centredRight);
        drawLegend (g, rowNames[r], { rowX + rowW + 8.0f, rowYs[r] - 5.0f, 22.0f, 10.0f }, 8.5f, juce::Justification::centredLeft);
        drawGainRow (g, rowYs[r], meterExpDb[(size_t) (2 - r)]);
    }
    drawLegend (g, "-20dB", { rowX - 4.0f, 103.0f, 40.0f, 9.0f }, 7.5f, juce::Justification::centredLeft);
    drawLegend (g, "GAIN CHANGE", { rowX, 103.0f, rowW, 9.0f }, 8.5f);
    drawLegend (g, "+12dB", { rowX + rowW - 36.0f, 103.0f, 40.0f, 9.0f }, 7.5f, juce::Justification::centredRight);

    drawImpactRow (g, impactRowY, meterImpactDb);
    drawLegend (g, "IMPACT RESTORATION", { rowX, 156.0f, rowW, 10.0f }, 8.5f);

    // Transition level & impact restoration columns.
    {
        const auto b = transition.column.getBounds().toFloat();
        const float ledH = (b.getHeight() - 4.0f) / 5.0f;
        drawLegend (g, "+", { b.getRight() + 4.0f, b.getY() + 2.0f, 12.0f, ledH }, 10.0f);
        juce::Path left;
        left.addTriangle (b.getRight() + 7.0f, b.getCentreY(), b.getRight() + 13.0f, b.getCentreY() - 4.0f,
                          b.getRight() + 13.0f, b.getCentreY() + 4.0f);
        g.setColour (colours::legend);
        g.fillPath (left);
        drawLegend (g, "-", { b.getRight() + 4.0f, b.getBottom() - 2.0f - ledH, 12.0f, ledH }, 11.0f);
    }
    drawArrows (g, transition);
    drawArrows (g, impact);
    drawLegend (g, "TRANSITION", { 548.0f, 156.0f, 72.0f, 10.0f }, 8.5f);
    drawLegend (g, "LEVEL",      { 548.0f, 166.0f, 72.0f, 10.0f }, 8.5f);
    drawLegend (g, "IMPACT",      { 622.0f, 156.0f, 80.0f, 10.0f }, 8.5f);
    drawLegend (g, "RESTORATION", { 622.0f, 166.0f, 80.0f, 10.0f }, 8.5f);

    // Model legend, top right.
    drawLegend (g, "MULTI-BAND EXPANDER", { 715.0f, 30.0f, 170.0f, 10.0f }, 9.0f);
    drawLegend (g, "WITH",                { 715.0f, 41.0f, 170.0f, 10.0f }, 9.0f);
    drawLegend (g, "IMPACT RESTORATION",  { 715.0f, 52.0f, 170.0f, 10.0f }, 9.0f);

    // Dark receiver window (decorative, as on the hardware).
    g.setColour (juce::Colour (0xff0a0a0b));
    g.fillRect (juce::Rectangle<float> (940.0f, 24.0f, 40.0f, 24.0f));
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRect (juce::Rectangle<float> (940.0f, 24.0f, 40.0f, 24.0f), 0.8f);

    // Bypass: LED above, legend below.
    {
        const auto b = bypassButton.getBounds().toFloat();
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ b.getCentreX(), 111.0f }),
                 colours::ledRed, bypassButton.getToggleState() ? 1.0f : 0.0f, display);
        drawLegend (g, "BYPASS", { b.getCentreX() - 30.0f, 156.0f, 60.0f, 10.0f }, 8.5f);
    }
    drawLegend (g, "DISPLAY", { 935.0f, 156.0f, 50.0f, 10.0f }, 8.5f);
}

void FrontPanel::resized()
{
    rearButton.setBounds (34, 126, 30, 14);

    const auto layoutStepped = [] (Stepped& s, int columnX, int downX, int upX)
    {
        s.column.setBounds (columnX, 28, 14, 70);
        s.down.setBounds (downX, 116, 14, 30);
        s.up.setBounds (upX, 116, 14, 30);
    };
    layoutStepped (volume,     162, 145, 179);
    layoutStepped (expansion,  239, 222, 256);
    layoutStepped (transition, 577, 561, 595);
    layoutStepped (impact,     654, 638, 672);

    bypassButton.setBounds (900, 128, 30, 14);
    displaySlider.setBounds (952, 82, 16, 64);
}

//==============================================================================
RearPanel::RearPanel (FourBXAudioProcessor& p, const float& displayLevel)
    : display (displayLevel)
{
    for (auto* pot : { &releasePot, &hfPot })
    {
        pot->setLookAndFeel (&lnf);
        pot->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
        pot->setMouseDragSensitivity (200);
        addAndMakeVisible (*pot);
    }
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, IDs::impactRelease, releasePot);
    hfAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, IDs::hfTransition, hfPot);

    softClipButton.setClickingTogglesState (true);
    softClipButton.onClick = [this] { repaint(); };
    addAndMakeVisible (softClipButton);
    softClipAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, IDs::softClip, softClipButton);

    frontButton.onClick = [this] { if (onShowFront) onShowFront(); };
    addAndMakeVisible (frontButton);
}

RearPanel::~RearPanel()
{
    releasePot.setLookAndFeel (nullptr);
    hfPot.setLookAndFeel (nullptr);
}

void RearPanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (colours::rearPanel, 0.0f, 0.0f,
                                             colours::rearPanel.darker (0.4f), 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Faint vertical brushing.
    g.setColour (juce::Colours::white.withAlpha (0.015f));
    for (float x = 0.0f; x < bounds.getRight(); x += 3.0f)
        g.drawVerticalLine ((int) x, 0.0f, bounds.getBottom());

    drawLegend (g, "MODEL 4BX", { 34.0f, 22.0f, 120.0f, 12.0f }, 10.0f, juce::Justification::centredLeft);
    drawLegend (g, "REAR PANEL CONTROLS", { 34.0f, 36.0f, 200.0f, 10.0f }, 8.0f, juce::Justification::centredLeft,
                colours::legend.withAlpha (0.6f));

    // Decorative RCA jack pairs.
    const auto jack = [&g] (float cx, float cy)
    {
        g.setColour (juce::Colour (0xffc9b26b));
        g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ cx, cy }));
        g.setColour (juce::Colour (0xff111111));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ cx, cy }));
    };
    for (float x : { 120.0f, 150.0f })
        for (float y : { 95.0f, 130.0f })
            jack (x, y);
    drawLegend (g, "IN", { 105.0f, 150.0f, 60.0f, 10.0f }, 8.0f);
    for (float x : { 790.0f, 820.0f })
        for (float y : { 95.0f, 130.0f })
            jack (x, y);
    drawLegend (g, "OUT", { 775.0f, 150.0f, 60.0f, 10.0f }, 8.0f);

    drawLegend (g, "IMPACT", { releasePot.getX() - 10.0f, 52.0f, (float) releasePot.getWidth() + 20.0f, 10.0f }, 8.5f);
    drawLegend (g, "RELEASE RATE", { releasePot.getX() - 10.0f, 62.0f, (float) releasePot.getWidth() + 20.0f, 10.0f }, 8.5f);
    drawLegend (g, "HF", { hfPot.getX() - 10.0f, 52.0f, (float) hfPot.getWidth() + 20.0f, 10.0f }, 8.5f);
    drawLegend (g, "TRANSITION LEVEL", { hfPot.getX() - 20.0f, 62.0f, (float) hfPot.getWidth() + 40.0f, 10.0f }, 8.5f);

    drawLegend (g, "ms", { releasePot.getX() + 0.0f, (float) releasePot.getBottom() + 1.0f, (float) releasePot.getWidth(), 9.0f }, 7.5f,
                juce::Justification::centred, colours::legend.withAlpha (0.6f));
    drawLegend (g, "dB", { hfPot.getX() + 0.0f, (float) hfPot.getBottom() + 1.0f, (float) hfPot.getWidth(), 9.0f }, 7.5f,
                juce::Justification::centred, colours::legend.withAlpha (0.6f));

    const auto sb = softClipButton.getBounds().toFloat();
    drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ sb.getCentreX(), sb.getY() - 10.0f }),
             colours::ledRed, softClipButton.getToggleState() ? 1.0f : 0.0f, display);
    drawLegend (g, "SOFT CLIP", { sb.getCentreX() - 40.0f, sb.getBottom() + 8.0f, 80.0f, 10.0f }, 8.5f);

    const auto fb = frontButton.getBounds().toFloat();
    drawLegend (g, "FRONT", { fb.getCentreX() - 30.0f, fb.getBottom() + 8.0f, 60.0f, 10.0f }, 8.0f);
    drawLegend (g, "PANEL", { fb.getCentreX() - 30.0f, fb.getBottom() + 18.0f, 60.0f, 10.0f }, 8.0f);
}

void RearPanel::resized()
{
    releasePot.setBounds (330, 76, 80, 96);
    hfPot.setBounds (480, 76, 80, 96);
    softClipButton.setBounds (640, 112, 30, 14);
    frontButton.setBounds (920, 126, 30, 14);
}

} // namespace fourbx::ui
