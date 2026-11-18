#include "PluginEditor.h"

namespace
{
    constexpr float meterMinDb = -20.0f; // yellow, downward expansion
    constexpr float meterMaxDb = 12.0f;  // red, upward expansion / impact
    const juce::Colour downColour  { 0xffe8c228 };
    const juce::Colour upColour    { 0xffe0402a };
    const juce::Colour unlitColour { 0xff2a2a2a };
}

FourBXAudioProcessorEditor::FourBXAudioProcessorEditor (FourBXAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    using IDs = FourBXAudioProcessor::ParamIDs;
    const std::array<std::pair<const char*, const char*>, 6> knobDefs {{
        { IDs::volume,        "Volume" },
        { IDs::expansion,     "Expansion" },
        { IDs::transition,    "Transition" },
        { IDs::hfTransition,  "HF Trim" },
        { IDs::impact,        "Impact" },
        { IDs::impactRelease, "Impact Release" },
    }};

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        addAndMakeVisible (k.slider);
        k.label.setText (knobDefs[i].second, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (k.label);
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, knobDefs[i].first, k.slider);
    }

    addAndMakeVisible (bypassButton);
    addAndMakeVisible (softClipButton);
    bypassAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, IDs::bypass, bypassButton);
    softClipAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, IDs::softClip, softClipButton);

    setSize (640, 380);
    startTimerHz (30);
}

FourBXAudioProcessorEditor::~FourBXAudioProcessorEditor()
{
    stopTimer();
}

void FourBXAudioProcessorEditor::timerCallback()
{
    const auto& m = processor.getMeters();
    constexpr float decayDbPerTick = 1.0f; // ~30 dB/s fall-back

    const auto follow = [] (float shown, float target)
    {
        // Jump out to larger magnitudes immediately, fall back toward 0 dB slowly.
        if (std::abs (target) >= std::abs (shown) || (target > 0.0f) != (shown > 0.0f))
            return target;
        return shown > 0.0f ? juce::jmax (target, shown - decayDbPerTick)
                            : juce::jmin (target, shown + decayDbPerTick);
    };

    float impactMax = 0.0f;
    for (size_t b = 0; b < shownExpDb.size(); ++b)
    {
        shownExpDb[b] = follow (shownExpDb[b], m.expansionDb[b].load (std::memory_order_relaxed));
        impactMax = juce::jmax (impactMax, m.impactDb[b].load (std::memory_order_relaxed));
    }
    shownImpactDb = follow (shownImpactDb, impactMax);

    repaint (meterArea);
}

void FourBXAudioProcessorEditor::drawGainMeter (juce::Graphics& g, juce::Rectangle<int> area,
                                                const char* label, float expDb) const
{
    g.setColour (juce::Colours::lightgrey);
    g.drawText (label, area.removeFromLeft (30), juce::Justification::centredLeft);

    constexpr int numLeds = 16; // 10 yellow (-20..0 in 2 dB steps), 6 red (0..+12 in 2 dB steps)
    const float ledW = (float) area.getWidth() / (float) numLeds;
    for (int i = 0; i < numLeds; ++i)
    {
        const bool isUp = i >= 10;
        bool lit;
        if (isUp)
            lit = expDb >= 2.0f * (float) (i - 10) + 1.0f;      // +1, +3, ... +11
        else
            lit = expDb <= -2.0f * (float) (10 - i) + 1.0f;     // -19, -17, ... -1
        auto led = juce::Rectangle<float> ((float) area.getX() + ledW * (float) i, (float) area.getY(),
                                           ledW, (float) area.getHeight()).reduced (2.0f);
        g.setColour (lit ? (isUp ? upColour : downColour) : unlitColour);
        g.fillRoundedRectangle (led, 2.0f);
    }
}

void FourBXAudioProcessorEditor::drawImpactMeter (juce::Graphics& g, juce::Rectangle<int> area, float impactDb) const
{
    g.setColour (juce::Colours::lightgrey);
    g.drawText ("IMP", area.removeFromLeft (30), juce::Justification::centredLeft);

    constexpr int numLeds = 12; // 0..+12 dB, 1 dB per LED
    const float ledW = (float) area.getWidth() / (float) numLeds;
    for (int i = 0; i < numLeds; ++i)
    {
        auto led = juce::Rectangle<float> ((float) area.getX() + ledW * (float) i, (float) area.getY(),
                                           ledW, (float) area.getHeight()).reduced (2.0f);
        g.setColour (impactDb >= (float) i + 0.5f ? upColour : unlitColour);
        g.fillRoundedRectangle (led, 2.0f);
    }
}

void FourBXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161616));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("4BX", getLocalBounds().removeFromTop (36).reduced (16, 0), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colours::grey);
    g.drawText ("3-band expander  |  gain change " + juce::String ((int) meterMinDb) + " .. +" + juce::String ((int) meterMaxDb) + " dB",
                getLocalBounds().removeFromTop (36).reduced (16, 0), juce::Justification::centredRight);

    auto rows = meterArea;
    const int rowH = rows.getHeight() / 4;
    drawGainMeter (g, rows.removeFromTop (rowH), "HF", shownExpDb[2]);
    drawGainMeter (g, rows.removeFromTop (rowH), "MF", shownExpDb[1]);
    drawGainMeter (g, rows.removeFromTop (rowH), "LF", shownExpDb[0]);
    drawImpactMeter (g, rows, shownImpactDb);
}

void FourBXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (28);

    meterArea = area.removeFromTop (110);
    area.removeFromTop (12);

    auto buttons = area.removeFromBottom (28);
    bypassButton.setBounds (buttons.removeFromLeft (100));
    softClipButton.setBounds (buttons.removeFromLeft (100));

    const int knobW = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (knobW);
        k.label.setBounds (col.removeFromTop (18));
        k.slider.setBounds (col.reduced (4));
    }
}
