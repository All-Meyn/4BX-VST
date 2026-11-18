#include "ThreeBandExpander.h"

#include <cmath>

namespace fourbx
{
using namespace constants;

namespace
{
    // 10*log10(x) for power, 20*log10(x) for amplitude, and dB->gain helpers.
    inline float powerToDb (float p) noexcept     { return 10.0f * std::log10 (p); }
    inline float amplitudeToDb (float a) noexcept { return 20.0f * std::log10 (a); }
    inline float dbToGain (float db) noexcept     { return std::exp (db * 0.11512925464970229f); } // ln(10)/20

    constexpr float impactEnvFloor = 1.0e-5f; // -100 dBFS
}

float ThreeBandExpander::onePoleCoeff (double seconds, double sr) noexcept
{
    // Coefficient for y += c * (x - y): reaches ~63% of a step after `seconds`.
    return (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
}

void ThreeBandExpander::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate = newSampleRate;
    numChannels = newNumChannels;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize),
                                        (juce::uint32) juce::jmax (1, numChannels) };

    lowMidSplit.setCutoffFrequency (crossoverLowMidHz);
    midHighSplit.setCutoffFrequency (crossoverMidHighHz);
    lowBandAllpass.setCutoffFrequency (crossoverMidHighHz);
    lowBandAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    lowMidSplit.prepare (spec);
    midHighSplit.prepare (spec);
    lowBandAllpass.prepare (spec);

    for (int b = 0; b < numBands; ++b)
    {
        auto& s = bands[(size_t) b];
        s.rmsCoeff          = onePoleCoeff (rmsTimeSeconds[b], sampleRate);
        s.levelAttackCoeff  = onePoleCoeff (levelAttackSeconds[b], sampleRate);
        s.levelReleaseCoeff = onePoleCoeff (levelReleaseSeconds[b], sampleRate);
        s.fastAttackCoeff  = onePoleCoeff (impactFastAttackSeconds, sampleRate);
        s.fastReleaseCoeff = onePoleCoeff (impactFastReleaseSeconds[b], sampleRate);
        s.slowCoeff        = onePoleCoeff (impactSlowTimeSeconds[b], sampleRate);
        s.impactReleaseScale = impactReleaseBandScale[b];
    }
    lastImpactReleaseMs = -1.0f; // force recompute on next setParameters()

    for (auto* sv : { &ratioMinusOne, &transitionDb, &hfTransitionDb, &impactAmount, &outputDb, &wet })
        sv->reset (sampleRate, parameterSmoothingSeconds);

    reset();
}

void ThreeBandExpander::reset()
{
    lowMidSplit.reset();
    midHighSplit.reset();
    lowBandAllpass.reset();

    for (auto& s : bands)
    {
        s.meanSquare = 0.0f;
        s.levelDb = powerToDb (detectorFloor);
        s.fastEnv = 0.0f;
        s.slowEnvDb = amplitudeToDb (impactEnvFloor);
        s.impactGainDb = 0.0f;
    }

    for (auto& m : meters.expansionDb) m.store (0.0f, std::memory_order_relaxed);
    for (auto& m : meters.impactDb)    m.store (0.0f, std::memory_order_relaxed);
}

void ThreeBandExpander::setParameters (const Parameters& p) noexcept
{
    const auto setOrSnap = [] (juce::SmoothedValue<float>& sv, float target, bool snap)
    {
        if (snap) sv.setCurrentAndTargetValue (target);
        else      sv.setTargetValue (target);
    };

    // On the very first call after prepare(), jump straight to the values instead of ramping.
    const bool snap = lastImpactReleaseMs < 0.0f;

    setOrSnap (ratioMinusOne,  juce::jlimit (0.0f, maxExpansionPercent, p.expansionPercent) / 100.0f, snap);
    setOrSnap (transitionDb,   p.transitionDb, snap);
    setOrSnap (hfTransitionDb, p.hfTransitionDb, snap);
    setOrSnap (impactAmount,   juce::jlimit (0.0f, 100.0f, p.impactPercent) / 100.0f, snap);
    setOrSnap (outputDb,       p.outputDb, snap);
    setOrSnap (wet,            p.bypass ? 0.0f : 1.0f, snap);
    softClipEnabled = p.softClip;

    if (! juce::exactlyEqual (p.impactReleaseMs, lastImpactReleaseMs))
    {
        lastImpactReleaseMs = p.impactReleaseMs;
        for (auto& s : bands)
            s.impactReleaseCoeff = onePoleCoeff (0.001 * p.impactReleaseMs * s.impactReleaseScale, sampleRate);
    }
}

float ThreeBandExpander::computeBandGainDb (int band, float meanSquareIn, float absIn,
                                            float rMinus1, float pivotDb,
                                            float impactAmt, float wetAmt) noexcept
{
    auto& s = bands[(size_t) band];

    // --- Linear-in-dB expansion from a true-RMS detector -------------------
    s.meanSquare += s.rmsCoeff * (meanSquareIn - s.meanSquare);
    const float rmsDb = powerToDb (s.meanSquare + detectorFloor);
    const float levelC = rmsDb > s.levelDb ? s.levelAttackCoeff : s.levelReleaseCoeff;
    s.levelDb += levelC * (rmsDb - s.levelDb);
    const float levelDb = s.levelDb;
    const float expansionDb = juce::jlimit (maxDownwardExpansionDb, maxUpwardExpansionDb,
                                            rMinus1 * (levelDb - pivotDb));

    // --- Impact: fast envelope rising above slow envelope -> upward boost ---
    const float fastC = absIn > s.fastEnv ? s.fastAttackCoeff : s.fastReleaseCoeff;
    s.fastEnv += fastC * (absIn - s.fastEnv);
    const float fastDb = amplitudeToDb (s.fastEnv + impactEnvFloor);
    s.slowEnvDb += s.slowCoeff * (fastDb - s.slowEnvDb);

    const float riseDb = fastDb - s.slowEnvDb - impactDeadbandDb;
    // Impact amount sets the ceiling: 50% -> at most +6 dB, 100% -> +12 dB.
    const float targetImpactDb = juce::jlimit (0.0f, impactAmt * maxImpactDb, impactSensitivity * riseDb);

    // Instant attack, adjustable exponential release.
    if (targetImpactDb > s.impactGainDb)
        s.impactGainDb = targetImpactDb;
    else
        s.impactGainDb += s.impactReleaseCoeff * (targetImpactDb - s.impactGainDb);

    return wetAmt * (expansionDb + s.impactGainDb);
}

float ThreeBandExpander::softClip (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= clipperKnee)
        return x;
    const float range = 1.0f - clipperKnee;
    const float y = clipperKnee + range * std::tanh ((a - clipperKnee) / range);
    return std::copysign (y, x);
}

void ThreeBandExpander::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int chans = juce::jmin (numChannels, buffer.getNumChannels());
    if (chans <= 0 || numSamples <= 0)
        return;

    auto* const* data = buffer.getArrayOfWritePointers();
    const float invChans = 1.0f / (float) chans;

    constexpr int maxChans = 2; // layouts are restricted to mono/stereo by the processor
    float low[maxChans], mid[maxChans], high[maxChans];
    float bandExpDb[numBands] {};

    for (int i = 0; i < numSamples; ++i)
    {
        const float rMinus1 = ratioMinusOne.getNextValue();
        const float pivot   = transitionDb.getNextValue();
        const float hfPivot = pivot + hfTransitionDb.getNextValue();
        const float impAmt  = impactAmount.getNextValue();
        const float wetAmt  = wet.getNextValue();
        const float outGain = dbToGain (outputDb.getNextValue());

        // Split into three bands and gather linked (channel-averaged) detector inputs.
        float ms[numBands] {}, ab[numBands] {};
        for (int ch = 0; ch < chans && ch < maxChans; ++ch)
        {
            float lowMid, highPart;
            lowMidSplit.processSample (ch, data[ch][i], lowMid, highPart);
            low[ch] = lowBandAllpass.processSample (ch, lowMid); // phase-align with the MF/HF split
            midHighSplit.processSample (ch, highPart, mid[ch], high[ch]);

            ms[0] += low[ch]  * low[ch];   ab[0] += std::abs (low[ch]);
            ms[1] += mid[ch]  * mid[ch];   ab[1] += std::abs (mid[ch]);
            ms[2] += high[ch] * high[ch];  ab[2] += std::abs (high[ch]);
        }

        float gain[numBands];
        for (int b = 0; b < numBands; ++b)
        {
            const float pivotForBand = (b == 2) ? hfPivot : pivot;
            const float gDb = computeBandGainDb (b, ms[b] * invChans, ab[b] * invChans,
                                                rMinus1, pivotForBand, impAmt, wetAmt);
            gain[b] = dbToGain (gDb);
            bandExpDb[b] = gDb - wetAmt * bands[(size_t) b].impactGainDb;
        }

        for (int ch = 0; ch < chans && ch < maxChans; ++ch)
        {
            float y = (low[ch] * gain[0] + mid[ch] * gain[1] + high[ch] * gain[2]) * outGain;
            data[ch][i] = softClipEnabled ? softClip (y) : y;
        }
    }

    lowMidSplit.snapToZero();
    midHighSplit.snapToZero();
    lowBandAllpass.snapToZero();

    const float wetNow = wet.getCurrentValue();
    for (int b = 0; b < numBands; ++b)
    {
        meters.expansionDb[(size_t) b].store (bandExpDb[b], std::memory_order_relaxed);
        meters.impactDb[(size_t) b].store (wetNow * bands[(size_t) b].impactGainDb, std::memory_order_relaxed);
    }
}

} // namespace fourbx
