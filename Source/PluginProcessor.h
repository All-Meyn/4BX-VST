#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/MultiBandExpander.h"

class TriExpandAudioProcessor final : public juce::AudioProcessor
{
public:
    TriExpandAudioProcessor();
    ~TriExpandAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; } // VST3 validator rejects empty names
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter IDs are part of the saved-state format: renaming one breaks
    // existing host sessions and presets.
    struct ParamIDs
    {
        static constexpr const char* expansion     = "expansion";
        static constexpr const char* transition    = "transition";
        static constexpr const char* hfTransition  = "hfTransition";
        static constexpr const char* impact        = "impact";
        static constexpr const char* impactRelease = "impactRelease";
        static constexpr const char* volume        = "volume";
        static constexpr const char* bypass        = "expanderBypass";
        static constexpr const char* softClip      = "softClip";
        // Hepta-Expand only (constants::numBassBands > 0):
        static constexpr const char* bassRestore    = "bassRestore";
        static constexpr const char* bassTransition = "bassTransition";
    };

    static constexpr bool hasBassRestoration = triexpand::constants::numBassBands > 0;

    juce::AudioProcessorValueTreeState apvts;

    const triexpand::MultiBandExpander::Meters& getMeters() const noexcept { return expander.getMeters(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushParametersToDsp() noexcept;

    // Cached parameter pointers; never looked up per block.
    std::atomic<float>* expansionParam     = nullptr;
    std::atomic<float>* transitionParam    = nullptr;
    std::atomic<float>* hfTransitionParam  = nullptr;
    std::atomic<float>* impactParam        = nullptr;
    std::atomic<float>* impactReleaseParam = nullptr;
    std::atomic<float>* volumeParam        = nullptr;
    std::atomic<float>* bypassParam        = nullptr;
    std::atomic<float>* softClipParam      = nullptr;
    std::atomic<float>* bassRestoreParam    = nullptr; // null in Tri-Expand
    std::atomic<float>* bassTransitionParam = nullptr; // null in Tri-Expand

    triexpand::MultiBandExpander expander;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriExpandAudioProcessor)
};
