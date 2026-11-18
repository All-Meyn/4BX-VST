#include "PluginEditor.h"

namespace
{
    const juce::Identifier displayLevelId { "displayLevel" };
    const juce::Identifier gpuRenderingId { "gpuRendering" };

    // Names JUCE uses for the Windows rendering engines (see HWNDComponentPeer).
    constexpr const char* softwareEngineName = "Software Renderer";
    constexpr const char* gpuEngineName      = "Direct2D";

    // Audacity 4 places one of its own windows over the plugin during Preview.
    // With Direct2D (GPU) drawing that shows as a blank white panel; with the
    // software renderer the panel shows through faded. So in Audacity, default
    // to software drawing. JUCE's PluginHostType doesn't know Audacity, so check
    // the host executable's name.
    bool isAudacityHost()
    {
        return juce::File::getSpecialLocation (juce::File::hostApplicationPath)
                   .getFileNameWithoutExtension().containsIgnoreCase ("audacity");
    }

    // An explicit choice made with the GPU DRAW switch is saved and wins;
    // otherwise use the per-host default.
    bool loadGpuRendering (juce::AudioProcessorValueTreeState& apvts)
    {
        if (apvts.state.hasProperty (gpuRenderingId))
            return (bool) apvts.state.getProperty (gpuRenderingId);

        return ! isAudacityHost();
    }

    float loadDisplayLevel (juce::AudioProcessorValueTreeState& apvts)
    {
        return juce::jlimit (0.0f, 1.0f, (float) (double) apvts.state.getProperty (displayLevelId, 1.0));
    }
}

TriExpandAudioProcessorEditor::TriExpandAudioProcessorEditor (TriExpandAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      displayLevel (loadDisplayLevel (p.apvts)),
      gpuRendering (loadGpuRendering (p.apvts)),
      front (p, displayLevel),
      rear (p, displayLevel)
{
    using namespace triexpand::ui;

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

    if constexpr (TriExpandAudioProcessor::hasAutoTransition)
        autoTransitionParam = p.apvts.getRawParameterValue (TriExpandAudioProcessor::ParamIDs::autoTransition);

    startTimerHz (30);
}

TriExpandAudioProcessorEditor::~TriExpandAudioProcessorEditor()
{
    stopTimer();
}

void TriExpandAudioProcessorEditor::showRear (bool showRearPanel)
{
    front.setVisible (! showRearPanel);
    rear.setVisible (showRearPanel);
}

void TriExpandAudioProcessorEditor::updateRenderingEngine()
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

void TriExpandAudioProcessorEditor::timerCallback()
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
    shownBassDb = follow (shownBassDb, m.bassRestoreDb.load (std::memory_order_relaxed));

    if (m.outputPeak.load (std::memory_order_relaxed) > 1.0f)
        overHoldTicks = 30;
    else if (overHoldTicks > 0)
        --overHoldTicks;

    if (front.isVisible())
    {
        front.setMeterValues (shownExpDb, shownImpactDb, shownBassDb);
        front.setOverLit (overHoldTicks > 0);
        if constexpr (TriExpandAudioProcessor::hasAutoTransition)
            if (autoTransitionParam != nullptr)
                front.setAutoTransitionLit (autoTransitionParam->load (std::memory_order_relaxed) >= 0.5f);
    }
    else
    {
        rear.setHeadroomTrimDb (m.headroomTrimDb.load (std::memory_order_relaxed));
    }
}

void TriExpandAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void TriExpandAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) triexpand::ui::panelWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
}
