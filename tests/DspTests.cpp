// Offline checks for fourbx::ThreeBandExpander. Exits non-zero on failure.
#include "DSP/ThreeBandExpander.h"

#include <cmath>
#include <cstdio>
#include <functional>

using fourbx::ThreeBandExpander;

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 512;
int failures = 0;

void check (bool ok, const char* what, double value)
{
    std::printf ("%s  %-58s (%.3f)\n", ok ? "PASS" : "FAIL", what, value);
    if (! ok) ++failures;
}

double rmsDb (const juce::AudioBuffer<float>& b, int start, int len)
{
    double sum = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = start; i < start + len; ++i)
            sum += (double) b.getSample (ch, i) * b.getSample (ch, i);
    return 10.0 * std::log10 (sum / (double) (len * b.getNumChannels()) + 1e-30);
}

// Runs `seconds` of the generator through a freshly prepared expander, returns the output.
juce::AudioBuffer<float> run (const ThreeBandExpander::Parameters& p, double seconds,
                              const std::function<float (int)>& gen, int channels = 2,
                              ThreeBandExpander* keep = nullptr)
{
    ThreeBandExpander local;
    auto& ex = keep ? *keep : local;
    ex.prepare (sr, block, channels);
    ex.setParameters (p);

    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> out (channels, n);
    for (int i = 0; i < n; ++i)
        for (int ch = 0; ch < channels; ++ch)
            out.setSample (ch, i, gen (i));

    for (int start = 0; start < n; start += block)
    {
        const int len = juce::jmin (block, n - start);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), channels, start, len);
        ex.setParameters (p);
        ex.process (view);
    }
    return out;
}

std::function<float (int)> sine (double hz, double rmsDbfs)
{
    const double amp = std::sqrt (2.0) * std::pow (10.0, rmsDbfs / 20.0);
    return [=] (int i) { return (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / sr)); };
}

ThreeBandExpander::Parameters neutral()
{
    ThreeBandExpander::Parameters p;
    p.expansionPercent = 0.0f;
    p.impactPercent = 0.0f;
    p.transitionDb = -20.0f;
    p.hfTransitionDb = 0.0f;
    p.outputDb = 0.0f;
    return p;
}

// Steady-state gain in dB for a sine at the given level (measured over the last 0.5 s of 2 s).
double steadyGainDb (const ThreeBandExpander::Parameters& p, double hz, double levelDb)
{
    auto out = run (p, 2.0, sine (hz, levelDb));
    return rmsDb (out, (int) (1.5 * sr), (int) (0.5 * sr)) - levelDb;
}
} // namespace

int main()
{
    // 1. Neutral settings: crossovers recombine flat.
    for (double hz : { 40.0, 150.0, 1000.0, 6000.0, 15000.0 })
    {
        char name[96];
        std::snprintf (name, sizeof name, "flat response at %.0f Hz with 0%% expansion, 0%% impact", hz);
        const double g = steadyGainDb (neutral(), hz, -20.0);
        check (std::abs (g) < 0.1, name, g);
    }

    // 2. Linear-in-dB law: gain = (R-1) * (level - transition). R = 1.5.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        const double up = steadyGainDb (p, 1000.0, -10.0);
        check (std::abs (up - 5.0) < 0.5, "1 kHz @ -10 dBFS, pivot -20, 50%: expect +5 dB", up);
        const double down = steadyGainDb (p, 1000.0, -30.0);
        check (std::abs (down + 5.0) < 0.5, "1 kHz @ -30 dBFS, pivot -20, 50%: expect -5 dB", down);
        const double unity = steadyGainDb (p, 1000.0, -20.0);
        check (std::abs (unity) < 0.5, "1 kHz @ pivot level: expect 0 dB", unity);
        p.expansionPercent = 20.0f;
        const double r12 = steadyGainDb (p, 1000.0, -10.0);
        check (std::abs (r12 - 2.0) < 0.3, "1 kHz @ -10 dBFS, 20% (1.2:1): expect +2 dB", r12);
    }

    // 3. HF transition trim moves only the HF pivot.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.hfTransitionDb = 10.0f; // HF pivot -10
        const double hf = steadyGainDb (p, 12000.0, -20.0);
        check (std::abs (hf + 5.0) < 0.5, "12 kHz @ -20, HF trim +10: expect -5 dB", hf);
        const double mf = steadyGainDb (p, 1000.0, -20.0);
        check (std::abs (mf) < 0.5, "1 kHz unaffected by HF trim: expect 0 dB", mf);
    }

    // 4. Gain limits.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.transitionDb = -40.0f;
        const double up = steadyGainDb (p, 1000.0, -3.0); // would be +18.5 unclamped
        check (up <= 12.1 && up > 11.0, "upward expansion clamps at +12 dB", up);
        p.transitionDb = 0.0f;
        const double down = steadyGainDb (p, 1000.0, -90.0); // would be -45 unclamped
        check (down >= -40.1 && down < -39.0, "downward expansion clamps at -40 dB", down);
    }

    // 5. Silence in -> silence out, no NaN/Inf.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.impactPercent = 100.0f;
        auto out = run (p, 0.5, [] (int) { return 0.0f; });
        bool finite = true;
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                finite &= std::isfinite (out.getSample (ch, i));
                peak = juce::jmax (peak, std::abs (out.getSample (ch, i)));
            }
        check (finite && juce::exactlyEqual (peak, 0.0f), "silence stays silent and finite", peak);
    }

    // 6. Impact: boosts a sudden attack, never cuts, returns to ~0 on steady signal.
    {
        auto p = neutral();
        p.impactPercent = 100.0f;
        p.impactReleaseMs = 200.0f;
        ThreeBandExpander ex;
        const int onset = (int) (0.5 * sr);
        auto quietThenLoud = [&] (int i) { return (float) (i < onset ? 0.01 : 0.3) * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sr); };
        auto out = run (p, 0.5 + 0.02, quietThenLoud, 2, &ex); // stop 20 ms after the attack
        const float impactAtAttack = ex.getMeters().impactDb[1].load();
        check (impactAtAttack > 3.0f, "impact boosts MF band on a sudden attack (dB)", impactAtAttack);

        for (auto [hz, band] : { std::pair { 30.0, 0 }, std::pair { 40.0, 0 }, std::pair { 1000.0, 1 }, std::pair { 10000.0, 2 } })
        {
            // Track the worst impact reading over the last second of a 3 s steady tone.
            ThreeBandExpander steadyEx;
            steadyEx.prepare (sr, block, 2);
            steadyEx.setParameters (p);
            auto gen = sine (hz, -20.0);
            juce::AudioBuffer<float> buf (2, block);
            float worst = 0.0f;
            for (int start = 0; start < (int) (3.0 * sr); start += block)
            {
                for (int i = 0; i < block; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                        buf.setSample (ch, i, gen (start + i));
                steadyEx.setParameters (p);
                steadyEx.process (buf);
                if (start > (int) (2.0 * sr))
                    worst = juce::jmax (worst, steadyEx.getMeters().impactDb[(size_t) band].load());
            }
            char name[96];
            std::snprintf (name, sizeof name, "impact < 1 dB on a steady %.0f Hz tone (worst, dB)", hz);
            check (worst < 1.0f, name, worst);
        }

        // Never attenuates: with 0% expansion, output can't be quieter than input.
        auto p2 = neutral();
        p2.impactPercent = 100.0f;
        const double g = steadyGainDb (p2, 1000.0, -20.0);
        check (g > -0.1 && g < 1.0, "impact never cuts level, ~0 dB on steady tone", g);
    }

    // 7. Longer impact release holds the boost longer.
    {
        auto measure = [] (float releaseMs)
        {
            auto p = neutral();
            p.impactPercent = 100.0f;
            p.impactReleaseMs = releaseMs;
            ThreeBandExpander ex;
            const int onset = (int) (0.5 * sr);
            run (p, 0.5 + 0.15, [&] (int i) { return (float) (i < onset ? 0.01 : 0.3) * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / sr); }, 2, &ex);
            return ex.getMeters().impactDb[1].load();
        };
        const float fast = measure (25.0f), slow = measure (1500.0f);
        check (slow > fast + 1.0f, "150 ms after attack: 1500 ms release > 25 ms release", slow - fast);
    }

    // 8. Expander bypass removes expansion and impact but keeps output volume.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.impactPercent = 100.0f;
        p.bypass = true;
        p.outputDb = -6.0f;
        const double g = steadyGainDb (p, 1000.0, -5.0);
        check (std::abs (g + 6.0) < 0.1, "bypass: only output volume applies (-6 dB)", g);
    }

    // 9. Soft clipper keeps output at or below 0 dBFS.
    {
        auto p = neutral();
        p.outputDb = 10.0f;
        p.softClip = true;
        auto out = run (p, 0.5, sine (1000.0, -3.0));
        const float peak = out.getMagnitude (0, out.getNumSamples());
        check (peak <= 1.0f, "soft clip: peak <= 1.0 with +10 dB volume", peak);
        p.softClip = false;
        auto raw = run (p, 0.5, sine (1000.0, -3.0));
        check (raw.getMagnitude (0, raw.getNumSamples()) > 1.0f, "no soft clip: peak exceeds 1.0", raw.getMagnitude (0, raw.getNumSamples()));
    }

    // 10. Mono works.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        auto out = run (p, 2.0, sine (1000.0, -10.0), 1);
        const double g = rmsDb (out, (int) (1.5 * sr), (int) (0.5 * sr)) + 10.0;
        check (std::abs (g - 5.0) < 0.5, "mono: 1 kHz @ -10, 50%: expect +5 dB", g);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
