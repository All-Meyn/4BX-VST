#include "Panels.h"

namespace triexpand::ui
{
using IDs = TriExpandAudioProcessor::ParamIDs;

namespace
{
    // Gain-change rows: 6 yellow LEDs (downward expansion, -20 dB at the far left)
    // and 6 red LEDs (upward expansion, +12 dB at the far right), as on the hardware.
    constexpr float yellowThresholdsDb[6] = { -18.0f, -14.0f, -10.0f, -6.0f, -3.0f, -1.0f }; // left -> centre
    constexpr float redThresholdsDb[6]    = { 1.0f, 3.0f, 5.0f, 7.0f, 9.0f, 11.0f };         // centre -> right

    constexpr float rowX = 312.0f, rowW = 208.0f;   // LED row extent
    // Row r (top to bottom) shows band numBands-1-r (highest band at the top).
    constexpr float rowY (int r) { return firstMeterRowY + meterRowSpacing * (float) r; }
    constexpr float oy = sideOffset; // vertical offset for everything outside the meter block

    // Transition / Impact / Bass Restoration: 10-LED filling bars, 20 px wide (the
    // LEDs then match the gain-change rows), centred where the old 14 px columns were.
    constexpr int barColumnLeds = 10;
    constexpr int barColumnWidth = 20;

   #if EXPANDER_NUM_BANDS == 3
    constexpr const char* badgeWord = "TRI";
    constexpr float badgeSize = 40.0f;
   #elif EXPANDER_NUM_BANDS == 4
    constexpr const char* badgeWord = "TETRA";
    constexpr float badgeSize = 30.0f;
   #else
    constexpr const char* badgeWord = "HEPTA";
    constexpr float badgeSize = 30.0f;
   #endif
    constexpr float rowLedW = 14.0f, rowLedH = 6.0f;

    juce::Rectangle<float> rowLed (int index, float y) // index 0..11 left to right
    {
        const float slot = (rowW - 6.0f) / 12.0f;
        const float x = rowX + (float) index * slot + (index >= 6 ? 6.0f : 0.0f);
        return { x + (slot - rowLedW) * 0.5f, y - rowLedH * 0.5f, rowLedW, rowLedH };
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
FrontPanel::FrontPanel (TriExpandAudioProcessor& p, float& displayLevel)
    : display (displayLevel),
      volume     ({ -30.0, -20.0, -10.0, 0.0, 10.0 }, displayLevel),
      expansion  ({ 10.0, 20.0, 30.0, 40.0, 50.0 }, displayLevel),
      transition (barColumnLeds, displayLevel),
      impact     (barColumnLeds, displayLevel),
      bass       (barColumnLeds, displayLevel)
{
    setUpStepped (volume,     p.apvts, IDs::volume,     1.0);
    setUpStepped (expansion,  p.apvts, IDs::expansion,  1.0);
    setUpStepped (transition, p.apvts, IDs::transition, 1.0);
    setUpStepped (impact,     p.apvts, IDs::impact,     2.0);
    if constexpr (hasBassRow)
        setUpStepped (bass,   p.apvts, IDs::bassRestore, 2.0);

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

void FrontPanel::setMeterValues (const std::array<float, constants::numBands>& expansionDb, float impactDb, float bassDb)
{
    meterExpDb = expansionDb;
    meterImpactDb = impactDb;
    meterBassDb = bassDb;
    if constexpr (hasBassRow)
        repaint (juce::Rectangle<int> ((int) rowX - 4, (int) bassRowY - 8, (int) rowW + 8, 16));
    repaint (juce::Rectangle<int> ((int) rowX - 4, (int) firstMeterRowY - 8, (int) rowW + 8, (int) (lastMeterRowY - firstMeterRowY) + 16));
    repaint (juce::Rectangle<int> ((int) rowX - 4, (int) impactRowY - 8, (int) rowW + 8, 16));
    repaint (bypassButton.getBounds().withY (104 + (int) oy).withHeight (14)); // bypass LED (host automation)
}

void FrontPanel::setOverLit (bool lit)
{
    if (lit != overLit)
    {
        overLit = lit;
        repaint (juce::Rectangle<int> (160, 8 + (int) oy, 50, 18));
    }
}

void FrontPanel::setAutoTransitionLit (bool lit)
{
    if (lit != autoTransitionLit)
    {
        autoTransitionLit = lit;
        repaint (juce::Rectangle<int> (570, 8 + (int) oy, 50, 18));
    }
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
    drawTriangle (g, { (float) s.down.getBounds().getCentreX(), 107.0f + oy }, false);
    drawTriangle (g, { (float) s.up.getBounds().getCentreX(),   107.0f + oy }, true);
}

void FrontPanel::drawBarScale (juce::Graphics& g, const LedColumn& col,
                               std::initializer_list<const char*> labels, float x) const
{
    const auto b = col.getBounds().toFloat();
    const float span = b.getHeight() - 4.0f;
    const int n = (int) labels.size();
    int i = 0;
    for (auto* text : labels)
    {
        const float y = b.getBottom() - 2.0f - span * (float) i / (float) (n - 1);
        drawLegend (g, text, { x, y - 4.5f, 30.0f, 9.0f }, 7.0f, juce::Justification::centredLeft);
        ++i;
    }
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
    g.setFont (juce::Font (juce::FontOptions (badgeSize, juce::Font::bold)).withExtraKerningFactor (-0.02f));
    g.drawText (badgeWord, juce::Rectangle<float> (34.0f, 22.0f + oy, 106.0f, 42.0f), juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)).withExtraKerningFactor (0.32f));
    g.drawText ("EXPAND", juce::Rectangle<float> (36.0f, 64.0f + oy, 106.0f, 16.0f), juce::Justification::centredLeft);

    // Rear-panel button legend.
    drawLegend (g, "REAR", { 26.0f, 152.0f + oy, 50.0f, 10.0f }, 8.0f);
    drawLegend (g, "PANEL", { 26.0f, 162.0f + oy, 50.0f, 10.0f }, 8.0f);

    // Volume & expansion columns.
    drawColumnScale (g, volume.column, { "+10dB", "0dB", "-10dB", "-20dB", "-30dB" }, false, 34.0f);
    drawColumnScale (g, expansion.column, { "50%", "40%", "30%", "20%", "10%" }, true, 24.0f);
    drawColumnScale (g, expansion.column, { "1.5/1", "1.4/1", "1.3/1", "1.2/1", "1.1/1" }, false, 28.0f);
    drawArrows (g, volume);
    drawArrows (g, expansion);
    drawLegend (g, "VOLUME",    { 135.0f, 156.0f + oy, 66.0f, 10.0f }, 8.5f);
    if constexpr (hasHeadroomUi)
    {
        // OVER: the plugin's output went above 0 dBFS (held ~1 s by the editor).
        drawLed (g, juce::Rectangle<float> (10.0f, 5.0f).withCentre ({ 169.0f, 18.0f + oy }), colours::ledRed, overLit ? 1.0f : 0.0f, display);
        drawLegend (g, "OVER", { 178.0f, 13.0f + oy, 30.0f, 10.0f }, 7.5f, juce::Justification::centredLeft);
    }
    drawLegend (g, "EXPANSION", { 210.0f, 156.0f + oy, 72.0f, 10.0f }, 8.5f);

    // Gain-change rows.
    for (int r = 0; r < constants::numBands; ++r)
    {
        const int band = constants::numBands - 1 - r;
        const char* name = constants::bandNames[band];
        const float y = rowY (r);
        // Names on both sides when there's room (3 and 4 bands); right side only for 7,
        // where the rows are too close and the left would collide with the Expansion scale.
        if (constants::numBands <= 4)
            drawLegend (g, name, constants::numBands == 3 ? juce::Rectangle<float> (rowX - 30.0f, y - 5.0f, 22.0f, 10.0f)
                                                          : juce::Rectangle<float> (rowX - 34.0f, y - 5.0f, 26.0f, 10.0f),
                        8.5f, juce::Justification::centredRight);
        drawLegend (g, name, { rowX + rowW + 8.0f, y - 5.0f, constants::numBands <= 4 ? 26.0f : 34.0f, 10.0f }, 8.5f,
                    juce::Justification::centredLeft);
        drawGainRow (g, y, meterExpDb[(size_t) band]);
    }
    const float gainLabelY = lastMeterRowY + 11.0f;
    drawLegend (g, "-20dB", { rowX - 4.0f, gainLabelY, 40.0f, 9.0f }, 7.5f, juce::Justification::centredLeft);
    drawLegend (g, "GAIN CHANGE", { rowX, gainLabelY, rowW, 9.0f }, 8.5f);
    drawLegend (g, "+12dB", { rowX + rowW - 36.0f, gainLabelY, 40.0f, 9.0f }, 7.5f, juce::Justification::centredRight);

    drawImpactRow (g, impactRowY, meterImpactDb);
    drawLegend (g, "IMPACT RESTORATION", { rowX, impactRowY + 14.0f, rowW, 10.0f }, 8.5f);

    if constexpr (hasBassRow)
    {
        // Centre-zero like the gain rows: yellow = Bass Restoration pulling the bass down,
        // red = pushing it up, relative to the main expansion.
        drawGainRow (g, bassRowY, meterBassDb);
        drawLegend (g, "BASS RESTORATION", { rowX, bassRowY + 14.0f, rowW, 10.0f }, 8.5f);
    }

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
    drawBarScale (g, transition.column, { "-40dB", "-30", "-20", "-10", "0dB" }, (float) transition.column.getRight() + 17.0f);
    drawBarScale (g, impact.column, { "0%", "20%", "40%", "60%", "80%", "100%" }, (float) impact.column.getRight() + 3.0f);
    if constexpr (hasBassRow)
        drawBarScale (g, bass.column, { "0%", "20%", "40%", "60%", "80%", "100%" }, (float) bass.column.getRight() + 3.0f);
    if constexpr (hasAutoTransitionUi)
    {
        // AUTO: Transition is an offset from each band's long-term level.
        drawLed (g, juce::Rectangle<float> (10.0f, 5.0f).withCentre ({ 584.0f, 18.0f + oy }), colours::ledRed,
                 autoTransitionLit ? 1.0f : 0.0f, display);
        drawLegend (g, "AUTO", { 593.0f, 13.0f + oy, 30.0f, 10.0f }, 7.5f, juce::Justification::centredLeft);
    }
    if constexpr (hasBassRow)
    {
        drawArrows (g, bass);
        drawLegend (g, "BASS",        { 698.0f, 156.0f + oy, 80.0f, 10.0f }, 8.5f);
        drawLegend (g, "RESTORATION", { 698.0f, 166.0f + oy, 80.0f, 10.0f }, 8.5f);
    }
    drawLegend (g, "TRANSITION", { 548.0f, 156.0f + oy, 72.0f, 10.0f }, 8.5f);
    drawLegend (g, "LEVEL",      { 548.0f, 166.0f + oy, 72.0f, 10.0f }, 8.5f);
    drawLegend (g, "IMPACT",      { 622.0f, 156.0f + oy, 80.0f, 10.0f }, 8.5f);
    drawLegend (g, "RESTORATION", { 622.0f, 166.0f + oy, 80.0f, 10.0f }, 8.5f);

    // Model legend, top right.
    // Moved right when the Bass Restoration column takes its place.
    const float legendX = hasBassRow ? 785.0f : 715.0f;
    drawLegend (g, "MULTI-BAND EXPANDER", { legendX, 30.0f + oy, 170.0f, 10.0f }, 9.0f);
    drawLegend (g, "WITH",                { legendX, 41.0f + oy, 170.0f, 10.0f }, 9.0f);
    drawLegend (g, hasBassRow ? "IMPACT & BASS RESTORATION" : "IMPACT RESTORATION",
                { legendX - 10.0f, 52.0f + oy, 190.0f, 10.0f }, 9.0f);

    // Dark receiver window (decorative, as on the hardware).
    g.setColour (juce::Colour (0xff0a0a0b));
    g.fillRect (juce::Rectangle<float> (940.0f, 24.0f + oy, 40.0f, 24.0f));
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRect (juce::Rectangle<float> (940.0f, 24.0f + oy, 40.0f, 24.0f), 0.8f);

    // Bypass: LED above, legend below.
    {
        const auto b = bypassButton.getBounds().toFloat();
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ b.getCentreX(), 111.0f + oy }),
                 colours::ledRed, bypassButton.getToggleState() ? 1.0f : 0.0f, display);
        drawLegend (g, "BYPASS", { b.getCentreX() - 30.0f, 156.0f + oy, 60.0f, 10.0f }, 8.5f);
    }
    drawLegend (g, "DISPLAY", { 935.0f, 156.0f + oy, 50.0f, 10.0f }, 8.5f);
}

void FrontPanel::resized()
{
    const int dy = (int) oy;
    rearButton.setBounds (34, 126 + dy, 30, 14);

    const auto layoutStepped = [dy] (Stepped& s, int columnX, int downX, int upX, int width = 14)
    {
        s.column.setBounds (columnX, 28 + dy, width, 70);
        s.down.setBounds (downX, 116 + dy, 14, 30);
        s.up.setBounds (upX, 116 + dy, 14, 30);
    };
    layoutStepped (volume,     162, 145, 179);
    layoutStepped (expansion,  239, 222, 256);
    constexpr int barShift = (barColumnWidth - 14) / 2; // keep the bars centred where the old columns were
    layoutStepped (transition, 577 - barShift, 561, 595, barColumnWidth);
    layoutStepped (impact,     654 - barShift, 638, 672, barColumnWidth);
    if constexpr (hasBassRow)
        layoutStepped (bass,   731 - barShift, 715, 749, barColumnWidth);

    bypassButton.setBounds (900, 128 + dy, 30, 14);
    displaySlider.setBounds (952, 82 + dy, 16, 64);
}

//==============================================================================
RearPanel::RearPanel (TriExpandAudioProcessor& p, const float& displayLevel)
    : display (displayLevel)
{
    for (auto* pot : { &releasePot, &hfPot, &bassPot })
    {
        pot->setLookAndFeel (&lnf);
        pot->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
        pot->setMouseDragSensitivity (200);
        if (pot != &bassPot || hasBassRow)
            addAndMakeVisible (*pot);
    }
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, IDs::impactRelease, releasePot);
    hfAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, IDs::hfTransition, hfPot);
    if constexpr (hasBassRow)
        bassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, IDs::bassTransition, bassPot);

    softClipButton.setClickingTogglesState (true);
    softClipButton.onClick = [this] { repaint(); };
    addAndMakeVisible (softClipButton);
    softClipAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, IDs::softClip, softClipButton);

    frontButton.onClick = [this] { if (onShowFront) onShowFront(); };
    addAndMakeVisible (frontButton);

    if constexpr (hasHeadroomUi)
    {
        autoHeadroomButton.setClickingTogglesState (true);
        autoHeadroomButton.setTooltip ("Auto Headroom: keeps the output's peaks near the input's");
        autoHeadroomButton.onClick = [this] { repaint(); };
        addAndMakeVisible (autoHeadroomButton);
        autoHeadroomAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, IDs::autoHeadroom, autoHeadroomButton);
    }

    if constexpr (hasAutoTransitionUi)
    {
        autoTransitionButton.setClickingTogglesState (true);
        autoTransitionButton.setTooltip ("Auto Transition: each band's pivot follows its own long-term level");
        autoTransitionButton.onClick = [this] { repaint(); };
        addAndMakeVisible (autoTransitionButton);
        autoTransitionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, IDs::autoTransition, autoTransitionButton);
    }

    gpuButton.setClickingTogglesState (true);
    gpuButton.setToggleState (true, juce::dontSendNotification);
    gpuButton.setTooltip ("GPU drawing (Direct2D). Turn off if the window goes blank in your host.");
    gpuButton.onClick = [this]
    {
        repaint();
        if (onGpuChanged) onGpuChanged (gpuButton.getToggleState());
    };
    addChildComponent (gpuButton); // hidden until the editor knows both renderers exist
}

void RearPanel::setHeadroomTrimDb (float db)
{
    if (std::abs (db - headroomTrimDb) >= 0.05f)
    {
        headroomTrimDb = db;
        const auto b = autoHeadroomButton.getBounds();
        repaint (b.getX() - 40, b.getBottom() + 4, b.getWidth() + 80, 30);
    }
}

void RearPanel::setGpuSwitchState (bool gpuOn)
{
    gpuButton.setToggleState (gpuOn, juce::dontSendNotification);
    repaint();
}

void RearPanel::setGpuSwitchAvailable (bool available)
{
    if (gpuButton.isVisible() != available)
    {
        gpuButton.setVisible (available);
        repaint();
    }
}

RearPanel::~RearPanel()
{
    releasePot.setLookAndFeel (nullptr);
    hfPot.setLookAndFeel (nullptr);
    bassPot.setLookAndFeel (nullptr);
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

    drawLegend (g, "MODEL " + juce::String (JucePlugin_Name).toUpperCase(), { 34.0f, 22.0f + oy, 200.0f, 12.0f }, 10.0f,
                juce::Justification::centredLeft);
    drawLegend (g, "REAR PANEL CONTROLS", { 34.0f, 36.0f + oy, 200.0f, 10.0f }, 8.0f, juce::Justification::centredLeft,
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
            jack (x, y + oy);
    drawLegend (g, "IN", { 105.0f, 150.0f + oy, 60.0f, 10.0f }, 8.0f);
    for (float x : { 790.0f, 820.0f })
        for (float y : { 95.0f, 130.0f })
            jack (x, y + oy);
    drawLegend (g, "OUT", { 775.0f, 150.0f + oy, 60.0f, 10.0f }, 8.0f);

    drawLegend (g, "IMPACT", { releasePot.getX() - 10.0f, 52.0f + oy, (float) releasePot.getWidth() + 20.0f, 10.0f }, 8.5f);
    drawLegend (g, "RELEASE RATE", { releasePot.getX() - 10.0f, 62.0f + oy, (float) releasePot.getWidth() + 20.0f, 10.0f }, 8.5f);
    drawLegend (g, "HF", { hfPot.getX() - 10.0f, 52.0f + oy, (float) hfPot.getWidth() + 20.0f, 10.0f }, 8.5f);
    drawLegend (g, "TRANSITION LEVEL", { hfPot.getX() - 20.0f, 62.0f + oy, (float) hfPot.getWidth() + 40.0f, 10.0f }, 8.5f);

    drawLegend (g, "ms", { releasePot.getX() + 0.0f, (float) releasePot.getBottom() + 1.0f, (float) releasePot.getWidth(), 9.0f }, 7.5f,
                juce::Justification::centred, colours::legend.withAlpha (0.6f));
    drawLegend (g, "dB", { hfPot.getX() + 0.0f, (float) hfPot.getBottom() + 1.0f, (float) hfPot.getWidth(), 9.0f }, 7.5f,
                juce::Justification::centred, colours::legend.withAlpha (0.6f));

    if constexpr (hasBassRow)
    {
        drawLegend (g, "BASS", { bassPot.getX() - 10.0f, 52.0f + oy, (float) bassPot.getWidth() + 20.0f, 10.0f }, 8.5f);
        drawLegend (g, "TRANSITION LEVEL", { bassPot.getX() - 20.0f, 62.0f + oy, (float) bassPot.getWidth() + 40.0f, 10.0f }, 8.5f);
        drawLegend (g, "dB", { bassPot.getX() + 0.0f, (float) bassPot.getBottom() + 1.0f, (float) bassPot.getWidth(), 9.0f }, 7.5f,
                    juce::Justification::centred, colours::legend.withAlpha (0.6f));
    }

    const auto sb = softClipButton.getBounds().toFloat();
    drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ sb.getCentreX(), sb.getY() - 10.0f }),
             colours::ledRed, softClipButton.getToggleState() ? 1.0f : 0.0f, display);
    drawLegend (g, "SOFT CLIP", { sb.getCentreX() - 40.0f, sb.getBottom() + 8.0f, 80.0f, 10.0f }, 8.5f);

    if constexpr (hasHeadroomUi)
    {
        const auto ab = autoHeadroomButton.getBounds().toFloat();
        const bool on = autoHeadroomButton.getToggleState();
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ ab.getCentreX(), ab.getY() - 10.0f }),
                 colours::ledRed, on ? 1.0f : 0.0f, display);
        drawLegend (g, "AUTO HEADROOM", { ab.getCentreX() - 50.0f, ab.getBottom() + 8.0f, 100.0f, 10.0f }, 8.5f);
        drawLegend (g, on ? juce::String (headroomTrimDb, 1) + " dB" : juce::String ("OFF"),
                    { ab.getCentreX() - 40.0f, ab.getBottom() + 20.0f, 80.0f, 9.0f }, 7.5f,
                    juce::Justification::centred, colours::legend.withAlpha (0.6f));
    }

    if constexpr (hasAutoTransitionUi)
    {
        const auto tb = autoTransitionButton.getBounds().toFloat();
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ tb.getCentreX(), tb.getY() - 10.0f }),
                 colours::ledRed, autoTransitionButton.getToggleState() ? 1.0f : 0.0f, display);
        drawLegend (g, "AUTO TRANSITION", { tb.getCentreX() - 55.0f, tb.getBottom() + 8.0f, 110.0f, 10.0f }, 8.5f);
    }

    if (gpuButton.isVisible())
    {
        const auto gb = gpuButton.getBounds().toFloat();
        drawLed (g, juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ gb.getCentreX(), gb.getY() - 10.0f }),
                 colours::ledRed, gpuButton.getToggleState() ? 1.0f : 0.0f, display);
        drawLegend (g, "GPU DRAW", { gb.getCentreX() - 34.0f, gb.getBottom() + 8.0f, 68.0f, 10.0f }, 8.5f);
    }

    const auto fb = frontButton.getBounds().toFloat();
    drawLegend (g, "FRONT", { fb.getCentreX() - 30.0f, fb.getBottom() + 8.0f, 60.0f, 10.0f }, 8.0f);
    drawLegend (g, "PANEL", { fb.getCentreX() - 30.0f, fb.getBottom() + 18.0f, 60.0f, 10.0f }, 8.0f);
}

void RearPanel::resized()
{
    const int dy = (int) oy;
    releasePot.setBounds (330, 76 + dy, 80, 96);
    hfPot.setBounds (480, 76 + dy, 80, 96);
    bassPot.setBounds (200, 76 + dy, 80, 96);
    softClipButton.setBounds (620, 112 + dy, 30, 14);
    gpuButton.setBounds (700, 112 + dy, 30, 14);
    autoHeadroomButton.setBounds (620, 162 + dy, 30, 14);
    autoTransitionButton.setBounds (720, 162 + dy, 30, 14);
    frontButton.setBounds (920, 126 + dy, 30, 14);
}

} // namespace triexpand::ui
