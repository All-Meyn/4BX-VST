// Offline checks for triexpand::MultiBandExpander. Exits non-zero on failure.
// Built once per band count (EXPANDER_NUM_BANDS = 3 or 7).
#include "DSP/MultiBandExpander.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using triexpand::MultiBandExpander;

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 512;
int failures = 0;

using namespace triexpand::constants;

// Index of the band that contains `hz`.
int bandOf (double hz)
{
    int b = 0;
    while (b < numBands - 1 && hz >= crossoverHz[b])
        ++b;
    return b;
}

// Test tones well inside a band. With 7 narrow bands a sine at band centre still
// loses ~2.6 dB to the two neighbouring crossovers, so gain-law checks get a wider
// tolerance there (the law itself is the same code, checked exactly at 3 bands).
const double midHz  = numBands == 3 ? 1000.0 : 632.0;   // 3: MF; 7: centre of 400 Hz - 1 kHz
const int    midBand = bandOf (midHz);
const int    topBand = numBands - 1;
const double lawTol  = 0.5;
const double law20Tol = 0.3;

// Analytic magnitude (dB) of band `b`'s crossover filters at `hz` (LR4: |LP| = 1/(1+r^4),
// |HP| = r^4/(1+r^4) with r = f/fc). Allpasses don't change magnitude.
double bandResponseDb (double hz, int b)
{
    double mag = 1.0;
    for (int k = 0; k < numBands - 1; ++k)
    {
        const double r4 = std::pow (hz / crossoverHz[k], 4.0);
        if (k < b)       mag *= r4 / (1.0 + r4); // high-passed by crossovers below the band
        else if (k == b) mag *= 1.0 / (1.0 + r4); // low-passed by the band's own upper crossover
    }
    return 20.0 * std::log10 (mag);
}

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
juce::AudioBuffer<float> run (const MultiBandExpander::Parameters& p, double seconds,
                              const std::function<float (int)>& gen, int channels = 2,
                              MultiBandExpander* keep = nullptr)
{
    MultiBandExpander local;
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

MultiBandExpander::Parameters neutral()
{
    MultiBandExpander::Parameters p;
    p.expansionPercent = 0.0f;
    p.impactPercent = 0.0f;
    p.transitionDb = -20.0f;
    p.hfTransitionDb = 0.0f;
    p.outputDb = 0.0f;
    return p;
}

// Steady-state gain in dB for a sine at the given level (measured over the last 0.5 s of 2 s).
double steadyGainDb (const MultiBandExpander::Parameters& p, double hz, double levelDb)
{
    auto out = run (p, 2.0, sine (hz, levelDb));
    return rmsDb (out, (int) (1.5 * sr), (int) (0.5 * sr)) - levelDb;
}
// Steady-state expansion gain the expander reports for one band (Impact 0), and the
// gain the law predicts from that band's own level: (R-1) * (bandLevel - pivot).
std::pair<double, double> bandLawDb (MultiBandExpander::Parameters p, double hz, double levelDb, int band)
{
    p.impactPercent = 0.0f;
    MultiBandExpander ex;
    run (p, 2.0, sine (hz, levelDb), 2, &ex);
    const double measured = ex.getMeters().expansionDb[(size_t) band].load();
    const double pivot = p.transitionDb + (band == numBands - 1 ? p.hfTransitionDb : 0.0f)
                         + (band < numBassBands ? p.bassTransitionDb : 0.0f) + bandPivotOffsetDb[band];
    const double expected = juce::jlimit ((double) maxDownwardExpansionDb, (double) maxUpwardExpansionDb,
                                          p.expansionPercent / 100.0 * (levelDb + bandResponseDb (hz, band) - pivot));
    return { measured, expected };
}
} // namespace

int main()
{
    std::printf ("Band count: %d, mid test tone %.0f Hz (band %d)\n\n", numBands, midHz, midBand);
    // 1. Neutral settings: crossovers recombine flat.
    std::vector<double> flatFreqs { 40.0, midHz, 15000.0 };
    for (int k = 0; k < numBands - 1; ++k)
        flatFreqs.push_back (crossoverHz[k]);
    for (double hz : flatFreqs)
    {
        char name[96];
        std::snprintf (name, sizeof name, "flat response at %.0f Hz with 0%% expansion, 0%% impact", hz);
        const double g = steadyGainDb (neutral(), hz, -20.0);
        check (std::abs (g) < 0.1, name, g);
    }

    // 2. Linear-in-dB law per band: gain = (R-1) * (band level - transition), R = 1.5.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        for (double level : { -10.0, -20.0, -30.0 })
        {
            const auto [measured, expected] = bandLawDb (p, midHz, level, midBand);
            char name[96];
            std::snprintf (name, sizeof name, "band law: mid tone @ %.0f dBFS, 50%%: expect %+.2f dB", level, expected);
            check (std::abs (measured - expected) < 0.3, name, measured);
        }
        p.expansionPercent = 20.0f;
        const auto [m20, e20] = bandLawDb (p, midHz, -10.0, midBand);
        check (std::abs (m20 - e20) < 0.2, "band law: mid tone @ -10 dBFS, 20% (1.2:1)", m20);
        p.expansionPercent = 50.0f;
        const auto [m40, e40] = bandLawDb (p, 40.0, -10.0, bandOf (40.0));
        check (std::abs (m40 - e40) < 0.3, "band law: 40 Hz @ -10 dBFS, 50% (bass detector, no peak bias)", m40);
    }

    // 2b. Whole-signal gain on a steady tone. With 3 wide bands this matches the law;
    //     with 7 narrow bands the tone also leaks into neighbouring bands, which sit far
    //     below the pivot and get expanded down, so the tone ends up quieter (reported only).
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        const double up = steadyGainDb (p, midHz, -10.0);
        const double down = steadyGainDb (p, midHz, -30.0);
        const double unity = steadyGainDb (p, midHz, -20.0);
        p.expansionPercent = 20.0f;
        const double r12 = steadyGainDb (p, midHz, -10.0);
        if (numBands == 3)
        {
            check (std::abs (up - 5.0) < lawTol, "mid tone @ -10 dBFS, pivot -20, 50%: expect +5 dB", up);
            check (std::abs (down + 5.0) < lawTol, "mid tone @ -30 dBFS, pivot -20, 50%: expect -5 dB", down);
            check (std::abs (unity) < lawTol, "mid tone @ pivot level: expect 0 dB", unity);
            check (std::abs (r12 - 2.0) < law20Tol, "mid tone @ -10 dBFS, 20% (1.2:1): expect +2 dB", r12);
        }
        else
        {
            std::printf ("INFO  whole-tone gain at %.0f Hz, 50%%: -10 dBFS %+.2f, -20 dBFS %+.2f, -30 dBFS %+.2f; 20%% @ -10: %+.2f dB\n",
                         midHz, up, unity, down, r12);
            // Expansion must still go the right way and stay ordered.
            check (up > unity && unity > down, "whole tone: louder input -> more gain (ordered)", up - down);
        }
    }

    // 3. HF transition trim moves only the HF pivot.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.hfTransitionDb = 10.0f; // HF pivot -10
        const double hf = steadyGainDb (p, 12000.0, -20.0);
        check (std::abs (hf + 5.0) < 0.5, "12 kHz @ -20, HF trim +10: expect -5 dB", hf);
        const auto [mf, mfExpected] = bandLawDb (p, midHz, -20.0, midBand);
        check (std::abs (mf - mfExpected) < 0.3, "mid band unaffected by HF trim (band law)", mf);
    }

    // 4. Gain limits.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.transitionDb = -40.0f;
        const double up = steadyGainDb (p, midHz, -3.0); // would be +18.5 unclamped
        check (up <= 12.1 && up > 11.0, "upward expansion clamps at +12 dB", up);
        p.transitionDb = 0.0f;
        const double down = steadyGainDb (p, midHz, -90.0); // would be -45 unclamped
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
        MultiBandExpander ex;
        const int onset = (int) (0.5 * sr);
        auto quietThenLoud = [&] (int i) { return (float) ((i < onset ? 0.01 : 0.3) * std::sin (2.0 * juce::MathConstants<double>::pi * midHz * i / sr)); };
        auto out = run (p, 0.5 + 0.02, quietThenLoud, 2, &ex); // stop 20 ms after the attack
        const float impactAtAttack = ex.getMeters().impactDb[(size_t) midBand].load();
        check (impactAtAttack > 3.0f, "impact boosts the mid tone's band on a sudden attack (dB)", impactAtAttack);

        for (auto [hz, band] : { std::pair { 30.0, 0 }, std::pair { 40.0, 0 }, std::pair { midHz, midBand }, std::pair { 10000.0, topBand } })
        {
            // Track the worst impact reading over the last second of a 3 s steady tone.
            MultiBandExpander steadyEx;
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
        const double g = steadyGainDb (p2, midHz, -20.0);
        check (g > -0.1 && g < 1.0, "impact never cuts level, ~0 dB on steady tone", g);
    }

    // 7. Longer impact release holds the boost longer.
    {
        auto measure = [] (float releaseMs)
        {
            auto p = neutral();
            p.impactPercent = 100.0f;
            p.impactReleaseMs = releaseMs;
            MultiBandExpander ex;
            const int onset = (int) (0.5 * sr);
            run (p, 0.5 + 0.15, [&] (int i) { return (float) ((i < onset ? 0.01 : 0.3) * std::sin (2.0 * juce::MathConstants<double>::pi * midHz * i / sr)); }, 2, &ex);
            return ex.getMeters().impactDb[(size_t) midBand].load();
        };
        const float fast = measure (25.0f), slow = measure (1500.0f);
        check (slow > fast + 1.0f, "150 ms after attack: 1500 ms release > 25 ms release", slow - fast);
    }

    // 7b. Impact amount sets the ceiling: 50% -> at most +6 dB (was +12 dB, which pumped).
    {
        auto p = neutral();
        p.impactPercent = 50.0f;
        MultiBandExpander ex;
        const int onset = (int) (0.5 * sr);
        run (p, 0.5 + 0.05, [&] (int i) { return (float) ((i < onset ? 0.001 : 0.5) * std::sin (2.0 * juce::MathConstants<double>::pi * midHz * i / sr)); }, 2, &ex);
        const float peak = ex.getMeters().impactDb[(size_t) midBand].load();
        check (peak <= 6.01f && peak > 3.0f, "impact 50%: big attack boosts, but at most +6 dB", peak);
    }

    // 7d. Bass Restoration (Hepta-Expand only): extra expansion in the bass bands.
    if constexpr (numBassBands > 0)
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.bassRestorePercent = 100.0f;           // bass ratio 1:2 (0.5 + 0.5)
        const int sub = bandOf (40.0);

        // Bass band follows the bass ratio: (R_bass - 1) * (band level - pivot).
        MultiBandExpander ex;
        run (p, 2.0, sine (40.0, -10.0), 2, &ex);
        const double measured = ex.getMeters().expansionDb[(size_t) sub].load();
        const double bandLevel = -10.0 + bandResponseDb (40.0, sub);
        const double bassPivot = p.transitionDb + bandPivotOffsetDb[sub];
        const double expected = 1.0 * (bandLevel - bassPivot);
        char name[96];
        std::snprintf (name, sizeof name, "bass 100%%: 40 Hz band at 1:2, expect %+.2f dB", expected);
        check (std::abs (measured - expected) < 0.3, name, measured);

        // Meter shows the extra gain relative to the main expansion: 0.5 * (level - pivot).
        const double bassMeter = ex.getMeters().bassRestoreDb.load();
        check (std::abs (bassMeter - 0.5 * (bandLevel - bassPivot)) < 0.3, "bass meter = extra gain from Bass Restoration", bassMeter);

        // Bands above the bass are untouched.
        const auto [mid, midExpected] = bandLawDb (p, midHz, -10.0, midBand);
        check (std::abs (mid - midExpected) < 0.3, "bass 100%: mid band unaffected", mid);

        // Bass Transition moves only the bass pivot: +10 dB -> 40 Hz band 10 dB further below its pivot.
        p.bassTransitionDb = 10.0f;
        MultiBandExpander ex2;
        run (p, 2.0, sine (40.0, -10.0), 2, &ex2);
        const double shifted = ex2.getMeters().expansionDb[(size_t) sub].load();
        check (std::abs (shifted - (expected - 10.0)) < 0.3, "bass transition +10 dB: bass gain 10 dB lower at 1:2", shifted);

        // 0 % Bass Restoration: bass band follows the normal law and the meter reads 0.
        p.bassRestorePercent = 0.0f;
        p.bassTransitionDb = 0.0f;
        MultiBandExpander ex3;
        run (p, 2.0, sine (40.0, -10.0), 2, &ex3);
        check (std::abs (ex3.getMeters().expansionDb[(size_t) sub].load() - 0.5 * (bandLevel - bassPivot)) < 0.3
                   && std::abs (ex3.getMeters().bassRestoreDb.load()) < 1.0e-6f,
               "bass 0%: normal law in the bass, meter 0", ex3.getMeters().bassRestoreDb.load());
    }

    // 7e. Auto Headroom (Hepta-Expand only): output peaks follow input peaks; down only.
    if constexpr (hasAutoHeadroom)
    {
        auto peakDb = [] (const juce::AudioBuffer<float>& b, int start)
        {
            float pk = 0.0f;
            for (int ch = 0; ch < b.getNumChannels(); ++ch)
                pk = juce::jmax (pk, b.getMagnitude (ch, start, b.getNumSamples() - start));
            return 20.0 * std::log10 (pk + 1e-12);
        };

        // Steady upward expansion (+~5 dB) is cancelled at the output.
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.autoHeadroom = true;
        // Auto Headroom settles over seconds (1 s down), so measure after 8 s.
        double steady;
        {
            auto out = run (p, 9.0, sine (midHz, -10.0));
            steady = rmsDb (out, (int) (8.0 * sr), (int) (1.0 * sr)) + 10.0;
        }
        check (std::abs (steady) < 0.7, "auto headroom: steady +gain cancelled to within its 0.5 dB deadband", steady);

        // Down only: downward expansion is left alone (no make-up gain).
        const double quiet = steadyGainDb (p, midHz, -30.0);
        p.autoHeadroom = false;
        const double quietRef = steadyGainDb (p, midHz, -30.0);
        check (std::abs (quiet - quietRef) < 0.1, "auto headroom: never boosts (downward expansion unchanged)", quiet - quietRef);

        // Drum-like hits with Impact 100 %: output peaks stay near input peaks.
        const auto hits = [] (int i)
        {
            const double t = i / sr, bt = std::fmod (t, 0.5);
            return (float) (0.5 * std::exp (-bt * 25) * std::sin (2.0 * juce::MathConstants<double>::pi * 180.0 * bt)
                            + 0.05 * std::sin (2.0 * juce::MathConstants<double>::pi * 632.0 * t));
        };
        auto ph = neutral();
        ph.impactPercent = 100.0f;
        ph.expansionPercent = 30.0f;
        ph.transitionDb = -30.0f;
        const int settle = (int) (4.0 * sr);
        double inPk = 0.0;
        { auto in = run (neutral(), 6.0, hits); ph.bypass = false; (void) in; }
        {
            juce::AudioBuffer<float> src (2, (int) (6.0 * sr));
            for (int i = 0; i < src.getNumSamples(); ++i) for (int ch = 0; ch < 2; ++ch) src.setSample (ch, i, hits (i));
            inPk = peakDb (src, settle);
        }
        ph.autoHeadroom = false;
        const double offPk = peakDb (run (ph, 6.0, hits), settle) - inPk;
        ph.autoHeadroom = true;
        MultiBandExpander hx;
        const double onPk = peakDb (run (ph, 6.0, hits, 2, &hx), settle) - inPk;
        std::printf ("INFO  Impact 100%% hits: output peak vs input %+.2f dB without, %+.2f dB with Auto Headroom (trim %.2f dB)\n",
                     offPk, onPk, hx.getMeters().headroomTrimDb.load());
        check (offPk > 2.0 && onPk < 1.0, "auto headroom: Impact peaks held within 1 dB of input", onPk);

        // Bypassed: transparent (only Volume applies).
        auto pb = neutral();
        pb.autoHeadroom = true;
        pb.bypass = true;
        pb.expansionPercent = 50.0f;
        pb.outputDb = -3.0f;
        const double byp = steadyGainDb (pb, midHz, -5.0);
        check (std::abs (byp + 3.0) < 0.1, "auto headroom + bypass: only Volume applies (-3 dB)", byp);
    }

    // 7f. Auto Transition (Hepta-Expand only): pivots follow each band's long-term level.
    if constexpr (hasAutoTransition)
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.autoTransition = true;
        const auto bandGain = [&] (const MultiBandExpander::Parameters& prm, const std::function<float (int)>& gen, double seconds)
        {
            MultiBandExpander ex;
            run (prm, seconds, gen, 2, &ex);
            return (double) ex.getMeters().expansionDb[(size_t) midBand].load();
        };

        // Steady tones at very different levels both sit at their own average -> ~0 dB.
        const double loud = bandGain (p, sine (midHz, -10.0), 3.0);
        const double quiet = bandGain (p, sine (midHz, -35.0), 3.0);
        check (std::abs (loud) < 0.5 && std::abs (quiet) < 0.5, "auto transition: steady tone ~0 dB at -10 and -35 dBFS", loud - quiet);

        // Transition 10 dB below centre -> pivot 10 dB under the average -> +5 dB at 50 %.
        auto po = p;
        po.transitionDb = autoTransitionCentreDb - 10.0f;
        const double offs = bandGain (po, sine (midHz, -35.0), 3.0);
        check (std::abs (offs - 5.0) < 0.5, "auto transition: Transition -10 dB offset -> +5 dB", offs);

        // Dynamics still expanded: 4 s at -30 dBFS, then a 10 dB jump; 0.6 s later ~+4..5 dB.
        const auto step = [] (int i)
        {
            const double lvl = i < (int) (4.0 * sr) ? -30.0 : -20.0;
            return (float) (std::sqrt (2.0) * std::pow (10.0, lvl / 20.0) * std::sin (2.0 * juce::MathConstants<double>::pi * midHz * i / sr));
        };
        const double jump = bandGain (p, step, 4.6);
        check (jump > 3.5 && jump < 5.5, "auto transition: +10 dB jump expanded up (~+5 dB at 50%)", jump);

        // Silence doesn't drag the average down: tone, 3 s silence, same tone -> ~0 dB again.
        const auto gap = [] (int i)
        {
            const bool on = i < (int) (3.0 * sr) || i >= (int) (6.0 * sr);
            return on ? (float) (std::sqrt (2.0) * std::pow (10.0, -25.0 / 20.0) * std::sin (2.0 * juce::MathConstants<double>::pi * midHz * i / sr)) : 0.0f;
        };
        const double afterGap = bandGain (p, gap, 7.5);
        check (std::abs (afterGap) < 1.0, "auto transition: average survives silence (~0 dB after a gap)", afterGap);
    }

    // 8. Expander bypass removes expansion and impact but keeps output volume.
    {
        auto p = neutral();
        p.expansionPercent = 50.0f;
        p.impactPercent = 100.0f;
        p.bypass = true;
        p.outputDb = -6.0f;
        const double g = steadyGainDb (p, midHz, -5.0);
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
        MultiBandExpander monoEx;
        auto out = run (p, 2.0, sine (midHz, -10.0), 1, &monoEx);
        const double monoBand = monoEx.getMeters().expansionDb[(size_t) midBand].load();
        const double monoExpected = 0.5 * (-10.0 + bandResponseDb (midHz, midBand) + 20.0 - bandPivotOffsetDb[midBand]);
        check (std::abs (monoBand - monoExpected) < 0.3, "mono: band law, mid tone @ -10, 50%", monoBand);
        const double g = rmsDb (out, (int) (1.5 * sr), (int) (0.5 * sr)) + 10.0;
        if (numBands == 3)
            check (std::abs (g - 5.0) < lawTol, "mono: mid tone @ -10, 50%: expect +5 dB", g);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
