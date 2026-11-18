#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "ExpanderConstants.h"

namespace triexpand
{

// Multi-band (3 or 7 bands, see ExpanderConstants.h), linear-in-dB upward/downward expander with transient
// ("impact") enhancement, modelled on the behaviour described in the original
// hardware's owner's manual. Stereo channels share one detector per band
// (linked), so the stereo image doesn't shift.
//
// Real-time safe: prepare() allocates; process() and setParameters() do not.
class MultiBandExpander
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
        float bassRestorePercent = 0.0f;  // 0..100: extra expansion in the bass bands (Hepta-Expand only)
        float bassTransitionDb   = 0.0f;  // offset added to the bass bands' pivot (Hepta-Expand only)
        bool  autoHeadroom       = false; // Hepta-Expand only: match output peaks to input peaks
        bool  autoTransition     = false; // Hepta-Expand only: pivots follow each band's long-term level
    };

    // Gain readings for the UI. Written by the audio thread, read by the UI thread.
    struct Meters
    {
        std::array<std::atomic<float>, constants::numBands> expansionDb {};
        std::array<std::atomic<float>, constants::numBands> impactDb {};
        // Extra gain Bass Restoration adds to the bass (largest of the bass bands, dB, +/-).
        std::atomic<float> bassRestoreDb { 0.0f };
        // Largest absolute output sample in the last block (after Volume / Soft Clip).
        std::atomic<float> outputPeak { 0.0f };
        // Current Auto Headroom trim in dB (0 or negative).
        std::atomic<float> headroomTrimDb { 0.0f };
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
        float levelDb = -100.0f;
        float longTermDb = -100.0f;   // Auto Transition: band's long-term level
        bool  longTermValid = false;  // set by the first non-silent level
        int   longTermInitSamples = 0; // fast-adapt samples left after sound first appears
        float fastEnv = 0.0f;
        float slowEnvDb = -100.0f;
        float impactGainDb = 0.0f;

        float rmsCoeff = 0.0f;
        float levelAttackCoeff = 0.0f;
        float levelReleaseCoeff = 0.0f;
        float fastAttackCoeff = 0.0f;
        float fastReleaseCoeff = 0.0f;
        float slowCoeff = 0.0f;
        float impactReleaseCoeff = 0.0f;
        float impactReleaseScale = 1.0f;
    };

    static float onePoleCoeff (double seconds, double sampleRate) noexcept;
    // `bassContributionDb`, when given, receives how much the band's expansion differs
    // from what `baseRatioMinusOne` / `basePivotDb` (the non-bass settings) would give.
    float computeBandGainDb (int band, float meanSquareIn, float absIn,
                             float ratioMinusOne, float transitionDb,
                             float impactAmount, float wet,
                             float* bassContributionDb = nullptr,
                             float baseRatioMinusOne = 0.0f, float basePivotDb = 0.0f) noexcept;
    static float softClip (float x) noexcept;

    double sampleRate = 44100.0;
    int numChannels = 0;
    float lastImpactReleaseMs = -1.0f;

    // Crossover tree: split k separates band k from everything above it. Band k is
    // then passed through allpasses at every crossover above k, so all bands share
    // the same phase and sum back flat.
    static constexpr int numSplits    = constants::numBands - 1;
    static constexpr int numAllpasses = (constants::numBands - 1) * (constants::numBands - 2) / 2;
    static constexpr int allpassIndex (int band, int crossover) noexcept
    {
        // Allpasses for band b cover crossovers b+1 .. numSplits-1, stored band by band.
        int offset = 0;
        for (int b = 0; b < band; ++b)
            offset += numSplits - 1 - b;
        return offset + (crossover - band - 1);
    }

    std::array<juce::dsp::LinkwitzRileyFilter<float>, numSplits> splits;
    std::array<juce::dsp::LinkwitzRileyFilter<float>, numAllpasses> allpasses;
    std::array<BandState, constants::numBands> bands {};

    juce::SmoothedValue<float> ratioMinusOne, transitionDb, hfTransitionDb,
                               impactAmount, outputDb, wet,
                               bassExtraRatio, bassTransitionDb, // used only when numBassBands > 0
                               autoTransitionMix;                // used only when hasAutoTransition
    float autoMixNow = 0.0f;       // current Auto Transition crossfade (0 = fixed pivots, 1 = auto)
    float longTermCoeff = 0.0f, longTermInitCoeff = 0.0f;
    int   longTermInitLength = 0;
    bool softClipEnabled = false;

    // Auto Headroom state (used only when constants::hasAutoHeadroom).
    bool  autoHeadroomEnabled = false;
    float inPeakEnv = 0.0f, outPeakEnv = 0.0f, headroomTrim = 1.0f;
    float peakReleaseMul = 0.0f, trimAttackCoeff = 0.0f, trimReleaseCoeff = 0.0f;

    Meters meters;
};

} // namespace triexpand
