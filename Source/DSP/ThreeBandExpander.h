#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "ExpanderConstants.h"

namespace fourbx
{

// Three-band, linear-in-dB upward/downward expander with transient
// ("impact") enhancement, modelled on the behaviour described in the original
// hardware's owner's manual. Stereo channels share one detector per band
// (linked), so the stereo image doesn't shift.
//
// Real-time safe: prepare() allocates; process() and setParameters() do not.
class ThreeBandExpander
{
public:
    struct Parameters
    {
        float expansionPercent = 25.0f;   // 0..50  -> ratio 1.0..1.5
        float transitionDb     = -20.0f;  // RMS dBFS pivot (unity-gain point)
        float hfTransitionDb   = 0.0f;    // offset added to the HF band's pivot
        float impactPercent    = 50.0f;   // 0..100
        float impactReleaseMs  = 200.0f;  // decay of impact gain (MF band)
        float outputDb         = 0.0f;    // post-expander volume
        bool  bypass           = false;   // removes expansion + impact only
        bool  softClip         = false;
    };

    // Gain readings for the UI. Written by the audio thread, read by the UI thread.
    struct Meters
    {
        std::array<std::atomic<float>, constants::numBands> expansionDb {};
        std::array<std::atomic<float>, constants::numBands> impactDb {};
    };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Call once per block before process(). Values are smoothed internally.
    void setParameters (const Parameters& p) noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;

    const Meters& getMeters() const noexcept { return meters; }

private:
    struct BandState
    {
        float meanSquare = 0.0f;
        float fastEnv = 0.0f;
        float slowEnvDb = -100.0f;
        float impactGainDb = 0.0f;

        float rmsCoeff = 0.0f;
        float fastAttackCoeff = 0.0f;
        float fastReleaseCoeff = 0.0f;
        float slowCoeff = 0.0f;
        float impactReleaseCoeff = 0.0f;
        float impactReleaseScale = 1.0f;
    };

    static float onePoleCoeff (double seconds, double sampleRate) noexcept;
    float computeBandGainDb (int band, float meanSquareIn, float absIn,
                             float ratioMinusOne, float transitionDb,
                             float impactAmount, float wet) noexcept;
    static float softClip (float x) noexcept;

    double sampleRate = 44100.0;
    int numChannels = 0;
    float lastImpactReleaseMs = -1.0f;

    juce::dsp::LinkwitzRileyFilter<float> lowMidSplit, midHighSplit, lowBandAllpass;
    std::array<BandState, constants::numBands> bands {};

    juce::SmoothedValue<float> ratioMinusOne, transitionDb, hfTransitionDb,
                               impactAmount, outputDb, wet;
    bool softClipEnabled = false;

    Meters meters;
};

} // namespace fourbx
