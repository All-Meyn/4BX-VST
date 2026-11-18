#include "PluginProcessor.h"
#include "PluginEditor.h"

FourBXAudioProcessor::FourBXAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    gainDbParam = apvts.getRawParameterValue (gainParamId);
    jassert (gainDbParam != nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout FourBXAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainParamId, 1 },
        "Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

void FourBXAudioProcessor::prepareToPlay (double sampleRate, int)
{
    gainSmoothed.reset (sampleRate, 0.02); // 20 ms ramp to avoid zipper noise
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainDbParam->load()));
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

    const auto numIn  = getTotalNumInputChannels();
    const auto numOut = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainDbParam->load()));

    if (! gainSmoothed.isSmoothing())
    {
        buffer.applyGain (gainSmoothed.getTargetValue());
        return;
    }

    auto* const* channels = buffer.getArrayOfWritePointers();
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = gainSmoothed.getNextValue();
        for (int ch = 0; ch < numIn; ++ch)
            channels[ch][i] *= g;
    }
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
