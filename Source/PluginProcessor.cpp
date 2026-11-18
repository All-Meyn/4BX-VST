#include "PluginProcessor.h"
#include "PluginEditor.h"

TriExpandAudioProcessor::TriExpandAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    expansionParam     = apvts.getRawParameterValue (ParamIDs::expansion);
    transitionParam    = apvts.getRawParameterValue (ParamIDs::transition);
    hfTransitionParam  = apvts.getRawParameterValue (ParamIDs::hfTransition);
    impactParam        = apvts.getRawParameterValue (ParamIDs::impact);
    impactReleaseParam = apvts.getRawParameterValue (ParamIDs::impactRelease);
    volumeParam        = apvts.getRawParameterValue (ParamIDs::volume);
    bypassParam        = apvts.getRawParameterValue (ParamIDs::bypass);
    softClipParam      = apvts.getRawParameterValue (ParamIDs::softClip);

    jassert (expansionParam && transitionParam && hfTransitionParam && impactParam
             && impactReleaseParam && volumeParam && bypassParam && softClipParam);
}

juce::AudioProcessorValueTreeState::ParameterLayout TriExpandAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto dB  = AudioParameterFloatAttributes().withLabel ("dB");
    const auto pct = AudioParameterFloatAttributes().withLabel ("%");
    const auto ms  = AudioParameterFloatAttributes().withLabel ("ms");

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::expansion, 1 }, "Expansion",
        NormalisableRange<float> (0.0f, triexpand::constants::maxExpansionPercent, 0.1f), 25.0f, pct));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::transition, 1 }, "Transition Level",
        NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -20.0f, dB));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::hfTransition, 1 }, "HF Transition Trim",
        NormalisableRange<float> (-10.0f, 10.0f, 0.1f), 0.0f, dB));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::impact, 1 }, "Impact Restoration",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct));

    NormalisableRange<float> releaseRange (25.0f, 1500.0f, 1.0f);
    releaseRange.setSkewForCentre (200.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::impactRelease, 1 }, "Impact Release",
        releaseRange, 200.0f, ms));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::volume, 1 }, "Output Volume",
        NormalisableRange<float> (-40.0f, 10.0f, 0.1f), 0.0f, dB));

    // On/off switches are two-option choices rather than AudioParameterBool:
    // AudioParameterBool keeps an unsnapped value (e.g. 0.66) that changes after a
    // state save/restore, which pluginval flags. A choice snaps to Off/On at once.
    // Same IDs, and the state still stores 0 or 1, so existing sessions load unchanged.
    const StringArray offOn { "Off", "On" };
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::bypass, 1 }, "Expander Bypass", offOn, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::softClip, 1 }, "Soft Clip", offOn, 0));

    return layout;
}

void TriExpandAudioProcessor::pushParametersToDsp() noexcept
{
    triexpand::ThreeBandExpander::Parameters p;
    p.expansionPercent = expansionParam->load();
    p.transitionDb     = transitionParam->load();
    p.hfTransitionDb   = hfTransitionParam->load();
    p.impactPercent    = impactParam->load();
    p.impactReleaseMs  = impactReleaseParam->load();
    p.outputDb         = volumeParam->load();
    p.bypass           = bypassParam->load() >= 0.5f;
    p.softClip         = softClipParam->load() >= 0.5f;
    expander.setParameters (p);
}

void TriExpandAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    expander.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    pushParametersToDsp(); // first call after prepare snaps to current values
}

void TriExpandAudioProcessor::releaseResources() {}

bool TriExpandAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void TriExpandAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    pushParametersToDsp();
    expander.process (buffer);
}

juce::AudioProcessorEditor* TriExpandAudioProcessor::createEditor()
{
    return new TriExpandAudioProcessorEditor (*this);
}

void TriExpandAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TriExpandAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TriExpandAudioProcessor();
}
