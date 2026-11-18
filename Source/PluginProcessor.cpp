#include "PluginProcessor.h"
#include "PluginEditor.h"

FourBXAudioProcessor::FourBXAudioProcessor()
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

juce::AudioProcessorValueTreeState::ParameterLayout FourBXAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto dB  = AudioParameterFloatAttributes().withLabel ("dB");
    const auto pct = AudioParameterFloatAttributes().withLabel ("%");
    const auto ms  = AudioParameterFloatAttributes().withLabel ("ms");

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::expansion, 1 }, "Expansion",
        NormalisableRange<float> (0.0f, fourbx::constants::maxExpansionPercent, 0.1f), 25.0f, pct));

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

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Expander Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::softClip, 1 }, "Soft Clip", false));

    return layout;
}

void FourBXAudioProcessor::pushParametersToDsp() noexcept
{
    fourbx::ThreeBandExpander::Parameters p;
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

void FourBXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    expander.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    pushParametersToDsp(); // first call after prepare snaps to current values
}

void FourBXAudioProcessor::releaseResources() {}

bool FourBXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void FourBXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    pushParametersToDsp();
    expander.process (buffer);
}

juce::AudioProcessorEditor* FourBXAudioProcessor::createEditor()
{
    return new FourBXAudioProcessorEditor (*this);
}

void FourBXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FourBXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FourBXAudioProcessor();
}
