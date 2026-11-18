#include "MultiBandExpander.h"

#include <cmath>

namespace triexpand
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

float MultiBandExpander::onePoleCoeff (double seconds, double sr) noexcept
{
    // Coefficient for y += c * (x - y): reaches ~63% of a step after `seconds`.
    return (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
}

void MultiBandExpander::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate = newSampleRate;
    numChannels = newNumChannels;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlockSize),
                                        (juce::uint32) juce::jmax (1, numChannels) };

    for (int k = 0; k < numSplits; ++k)
    {
        splits[(size_t) k].setCutoffFrequency (crossoverHz[k]);
        splits[(size_t) k].prepare (spec);
    }

    for (int b = 0; b < numSplits; ++b)
        for (int k = b + 1; k < numSplits; ++k)
        {
            auto& ap = allpasses[(size_t) allpassIndex (b, k)];
            ap.setCutoffFrequency (crossoverHz[k]);
            ap.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
            ap.prepare (spec);
        }

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

    peakReleaseMul   = (float) std::exp (-1.0 / (headroomPeakReleaseSeconds * sampleRate));
    trimAttackCoeff  = onePoleCoeff (headroomTrimAttackSeconds, sampleRate);
    trimReleaseCoeff = onePoleCoeff (headroomTrimReleaseSeconds, sampleRate);

    for (auto* sv : { &ratioMinusOne, &transitionDb, &hfTransitionDb, &impactAmount, &outputDb, &wet,
                      &bassExtraRatio, &bassTransitionDb })
        sv->reset (sampleRate, parameterSmoothingSeconds);

    reset();
}

void MultiBandExpander::reset()
{
    for (auto& f : splits)    f.reset();
    for (auto& f : allpasses) f.reset();

    for (auto& s : bands)
    {
        s.meanSquare = 0.0f;
        s.levelDb = powerToDb (detectorFloor);
        s.fastEnv = 0.0f;
        s.slowEnvDb = amplitudeToDb (impactEnvFloor);
        s.impactGainDb = 0.0f;
    }

    inPeakEnv = outPeakEnv = 0.0f;
    headroomTrim = 1.0f;
    meters.outputPeak.store (0.0f, std::memory_order_relaxed);
    meters.headroomTrimDb.store (0.0f, std::memory_order_relaxed);

    for (auto& m : meters.expansionDb) m.store (0.0f, std::memory_order_relaxed);
    for (auto& m : meters.impactDb)    m.store (0.0f, std::memory_order_relaxed);
}

void MultiBandExpander::setParameters (const Parameters& p) noexcept
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
    setOrSnap (bassExtraRatio,   juce::jlimit (0.0f, 100.0f, p.bassRestorePercent) / 100.0f * maxBassExtraRatio, snap);
    setOrSnap (bassTransitionDb, p.bassTransitionDb, snap);
    softClipEnabled = p.softClip;
    autoHeadroomEnabled = hasAutoHeadroom && p.autoHeadroom;

    if (! juce::exactlyEqual (p.impactReleaseMs, lastImpactReleaseMs))
    {
        lastImpactReleaseMs = p.impactReleaseMs;
        for (auto& s : bands)
            s.impactReleaseCoeff = onePoleCoeff (0.001 * p.impactReleaseMs * s.impactReleaseScale, sampleRate);
    }
}

float MultiBandExpander::computeBandGainDb (int band, float meanSquareIn, float absIn,
                                            float rMinus1, float pivotDb,
                                            float impactAmt, float wetAmt,
                                            float* bassContributionDb,
                                            float baseRMinus1, float basePivotDb) noexcept
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
    if (bassContributionDb != nullptr)
        *bassContributionDb = expansionDb - juce::jlimit (maxDownwardExpansionDb, maxUpwardExpansionDb,
                                                          baseRMinus1 * (levelDb - basePivotDb));

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

float MultiBandExpander::softClip (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= clipperKnee)
        return x;
    const float range = 1.0f - clipperKnee;
    const float y = clipperKnee + range * std::tanh ((a - clipperKnee) / range);
    return std::copysign (y, x);
}

void MultiBandExpander::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int chans = juce::jmin (numChannels, buffer.getNumChannels());
    if (chans <= 0 || numSamples <= 0)
        return;

    auto* const* data = buffer.getArrayOfWritePointers();
    const float invChans = 1.0f / (float) chans;

    constexpr int maxChans = 2; // layouts are restricted to mono/stereo by the processor
    float bandSig[maxChans][numBands];
    float bandExpDb[numBands] {};
    float bassDb = 0.0f;
    float blockOutPeak = 0.0f;
    const float minTrim = dbToGain (headroomMaxTrimDb);

    for (int i = 0; i < numSamples; ++i)
    {
        const float rMinus1 = ratioMinusOne.getNextValue();
        const float pivot   = transitionDb.getNextValue();
        const float hfPivot = pivot + hfTransitionDb.getNextValue();
        const float impAmt  = impactAmount.getNextValue();
        const float wetAmt  = wet.getNextValue();
        const float outGain = dbToGain (outputDb.getNextValue());

        float bassRMinus1 = rMinus1, bassPivot = pivot;
        if constexpr (numBassBands > 0)
        {
            bassRMinus1 = rMinus1 + bassExtraRatio.getNextValue();
            bassPivot   = pivot + bassTransitionDb.getNextValue();
        }

        // Split into bands and gather linked (channel-averaged) detector inputs.
        float ms[numBands] {}, ab[numBands] {};
        float inPeak = 0.0f;
        bassDb = 0.0f; // reset per sample; the block-end value goes to the meter
        for (int ch = 0; ch < chans && ch < maxChans; ++ch)
        {
            auto* band = bandSig[ch];
            float rest = data[ch][i];
            inPeak = juce::jmax (inPeak, std::abs (rest));
            for (int k = 0; k < numSplits; ++k)
            {
                float lowPart, highPart;
                splits[(size_t) k].processSample (ch, rest, lowPart, highPart);
                // Phase-align this band with every split above it.
                for (int a = k + 1; a < numSplits; ++a)
                    lowPart = allpasses[(size_t) allpassIndex (k, a)].processSample (ch, lowPart);
                band[k] = lowPart;
                rest = highPart;
            }
            band[numBands - 1] = rest;

            for (int b = 0; b < numBands; ++b)
            {
                ms[b] += band[b] * band[b];
                ab[b] += std::abs (band[b]);
            }
        }

        float gain[numBands];
        for (int b = 0; b < numBands; ++b)
        {
            float gDb;
            float bandOffset = 0.0f;
            if constexpr (hasBandPivotOffsets)
                bandOffset = bandPivotOffsetDb[b];

            if (b < numBassBands)
            {
                float contribution = 0.0f;
                gDb = computeBandGainDb (b, ms[b] * invChans, ab[b] * invChans,
                                         bassRMinus1, bassPivot + bandOffset, impAmt, wetAmt,
                                         &contribution, rMinus1, pivot + bandOffset);
                if (std::abs (contribution) >= std::abs (bassDb))
                    bassDb = contribution;
            }
            else
            {
                float pivotForBand = (b == numBands - 1) ? hfPivot : pivot;
                if constexpr (hasBandPivotOffsets)
                    pivotForBand += bandOffset;
                gDb = computeBandGainDb (b, ms[b] * invChans, ab[b] * invChans,
                                         rMinus1, pivotForBand, impAmt, wetAmt);
            }
            gain[b] = dbToGain (gDb);
            bandExpDb[b] = gDb - wetAmt * bands[(size_t) b].impactGainDb;
        }

        float sums[maxChans];
        float outPeak = 0.0f;
        for (int ch = 0; ch < chans && ch < maxChans; ++ch)
        {
            float sum = 0.0f;
            for (int b = 0; b < numBands; ++b)
                sum += bandSig[ch][b] * gain[b];
            sums[ch] = sum;
            outPeak = juce::jmax (outPeak, std::abs (sum));
        }

        float trim = 1.0f;
        if constexpr (hasAutoHeadroom)
        {
            // Peak envelopes of the input and of the expander's output (before Volume).
            inPeakEnv  = juce::jmax (inPeak,  inPeakEnv  * peakReleaseMul);
            outPeakEnv = juce::jmax (outPeak, outPeakEnv * peakReleaseMul);
            float target = 1.0f;
            if (autoHeadroomEnabled && outPeakEnv > 1.0e-6f)
            {
                // Faded with the bypass crossfade: bypassed, the output is just the
                // phase-shifted input, so there is no added gain to take back.
                const float ratio = juce::jlimit (minTrim, 1.0f, inPeakEnv / outPeakEnv);
                target = 1.0f - wetAmt * (1.0f - ratio);
            }
            headroomTrim += (target < headroomTrim ? trimAttackCoeff : trimReleaseCoeff) * (target - headroomTrim);
            trim = headroomTrim;
        }

        for (int ch = 0; ch < chans && ch < maxChans; ++ch)
        {
            float y;
            if constexpr (hasAutoHeadroom)
                y = sums[ch] * trim * outGain;
            else
                y = sums[ch] * outGain;
            y = softClipEnabled ? softClip (y) : y;
            data[ch][i] = y;
            blockOutPeak = juce::jmax (blockOutPeak, std::abs (y));
        }
    }

    for (auto& f : splits)    f.snapToZero();
    for (auto& f : allpasses) f.snapToZero();

    const float wetNow = wet.getCurrentValue();
    for (int b = 0; b < numBands; ++b)
    {
        meters.expansionDb[(size_t) b].store (bandExpDb[b], std::memory_order_relaxed);
        meters.impactDb[(size_t) b].store (wetNow * bands[(size_t) b].impactGainDb, std::memory_order_relaxed);
    }
    meters.bassRestoreDb.store (wetNow * bassDb, std::memory_order_relaxed);
    meters.outputPeak.store (blockOutPeak, std::memory_order_relaxed);
    meters.headroomTrimDb.store (amplitudeToDb (juce::jmax (headroomTrim, 1.0e-6f)), std::memory_order_relaxed);
}

} // namespace triexpand
