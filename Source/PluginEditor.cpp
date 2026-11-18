#include "PluginEditor.h"

namespace
{
    const juce::Identifier displayLevelId { "displayLevel" };
    const juce::Identifier gpuRenderingId { "gpuRendering" };

    // Names JUCE uses for the Windows rendering engines (see HWNDComponentPeer).
    constexpr const char* softwareEngineName = "Software Renderer";
    constexpr const char* gpuEngineName      = "Direct2D";

    float loadDisplayLevel (juce::AudioProcessorValueTreeState& apvts)
    {
        return juce::jlimit (0.0f, 1.0f, (float) (double) apvts.state.getProperty (displayLevelId, 1.0));
    }
}

FourBXAudioProcessorEditor::FourBXAudioProcessorEditor (FourBXAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      displayLevel (loadDisplayLevel (p.apvts)),
      gpuRendering ((bool) p.apvts.state.getProperty (gpuRenderingId, true)),
      front (p, displayLevel),
      rear (p, displayLevel)
{
    using namespace fourbx::ui;

    content.setSize (panelWidth, panelHeight);
    for (auto* panel : { static_cast<juce::Component*> (&front), static_cast<juce::Component*> (&rear) })
    {
        panel->setBounds (0, 0, panelWidth, panelHeight);
        content.addChildComponent (panel);
    }
    addAndMakeVisible (content);
    showRear (false);

    front.onShowRear  = [this] { showRear (true); };
    rear.onShowFront  = [this] { showRear (false); };
    front.onDisplayChanged = [this] (float level)
    {
        processor.apvts.state.setProperty (displayLevelId, level, nullptr);
        rear.repaint();
    };

    rear.setGpuSwitchState (gpuRendering);
    rear.onGpuChanged = [this] (bool gpuOn)
    {
        gpuRendering = gpuOn;
        processor.apvts.state.setProperty (gpuRenderingId, gpuOn, nullptr);
        updateRenderingEngine();
    };

    setResizable (true, true);
    setResizeLimits (panelWidth, panelHeight, panelWidth * 2, panelHeight * 2); // below 1x the legends get unreadable
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) panelWidth / (double) panelHeight);
    setSize (panelWidth * 5 / 4, panelHeight * 5 / 4);

    startTimerHz (30);
}

FourBXAudioProcessorEditor::~FourBXAudioProcessorEditor()
{
    stopTimer();
}

void FourBXAudioProcessorEditor::showRear (bool showRearPanel)
{
    front.setVisible (! showRearPanel);
    rear.setVisible (showRearPanel);
}

void FourBXAudioProcessorEditor::updateRenderingEngine()
{
    // The host creates the native window after the editor is constructed and may
    // recreate it, so this is called from the timer as well as from the switch.
    auto* peer = getPeer();
    if (peer == nullptr)
        return;

    const auto engines = peer->getAvailableRenderingEngines();
    const int softwareIndex = engines.indexOf (softwareEngineName);
    const int gpuIndex      = engines.indexOf (gpuEngineName);
    const bool switchable   = softwareIndex >= 0 && gpuIndex >= 0;

    rear.setGpuSwitchAvailable (switchable);

    if (! switchable)
        return;

    const int wanted = gpuRendering ? gpuIndex : softwareIndex;
    if (peer->getCurrentRenderingEngine() != wanted)
        peer->setCurrentRenderingEngine (wanted);
}

void FourBXAudioProcessorEditor::timerCallback()
{
    updateRenderingEngine();

    const auto& m = processor.getMeters();
    static constexpr float decayDbPerTick = 1.0f; // ~30 dB/s fall-back (static: usable in the lambda without capture on MSVC)

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

    if (front.isVisible())
        front.setMeterValues (shownExpDb, shownImpactDb);
}

void FourBXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void FourBXAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) fourbx::ui::panelWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
}
