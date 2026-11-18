#pragma once

// Tunable constants for the multi-band expander.
//
// The band count is chosen per plugin at build time with EXPANDER_NUM_BANDS:
// 3 = Tri-Expand, 4 = Quad-Expand, 7 = Hepta-Expand. Per-band tables below have one entry per band,
// lowest band first.
//
// Values marked [manual] come from the original hardware owner's manual.
// Values marked [chosen] are NOT documented there; they are our own design
// choices and are the first things to adjust by ear.

#ifndef EXPANDER_NUM_BANDS
 #define EXPANDER_NUM_BANDS 3
#endif

namespace triexpand::constants
{
    inline constexpr int numBands = EXPANDER_NUM_BANDS;
    static_assert (numBands == 3 || numBands == 4 || numBands == 7, "EXPANDER_NUM_BANDS must be 3, 4 or 7");

#if EXPANDER_NUM_BANDS == 3
    // ---- Tri-Expand: LF / MF / HF ----
    // Crossovers. [manual: LF < ~150 Hz, MF ~150 Hz-6 kHz, HF > ~6 kHz]
    inline constexpr float crossoverHz[numBands - 1] = { 150.0f, 6000.0f };
    inline constexpr const char* bandNames[numBands] = { "LF", "MF", "HF" };

    inline constexpr float rmsTimeSeconds[numBands]      = { 0.050f, 0.010f, 0.005f };
    inline constexpr float levelAttackSeconds[numBands]  = { 0.030f, 0.015f, 0.008f };
    inline constexpr float levelReleaseSeconds[numBands] = { 0.400f, 0.250f, 0.150f };
    inline constexpr float impactFastReleaseSeconds[numBands] = { 0.080f, 0.020f, 0.010f };
    inline constexpr float impactSlowTimeSeconds[numBands]    = { 0.150f, 0.080f, 0.040f };
    inline constexpr float impactReleaseBandScale[numBands]   = { 1.5f, 1.0f, 0.6f };

    // No Bass Restoration in Tri-Expand.
    inline constexpr int numBassBands = 0;

    // Tri-Expand is the reference for band-width compensation (no offsets).
    inline constexpr bool hasBandPivotOffsets = false;
    inline constexpr bool hasAutoHeadroom = false; // Tri-Expand 1.0 behaviour kept
    inline constexpr bool hasAutoTransition = false;
    inline constexpr float bandPivotOffsetDb[numBands] = { 0.0f, 0.0f, 0.0f };
#elif EXPANDER_NUM_BANDS == 4
    // ---- Quad-Expand: LF / LMF / HMF / HF ----
    // Crossovers. [chosen] Tri-Expand's 150 Hz and 6 kHz, with its mid band split at 1 kHz.
    inline constexpr float crossoverHz[numBands - 1] = { 150.0f, 1000.0f, 6000.0f };
    inline constexpr const char* bandNames[numBands] = { "LF", "LMF", "HMF", "HF" };

    // Per-band timing [chosen]: LF and HF as Tri-Expand; the two mid bands in between.
    inline constexpr float rmsTimeSeconds[numBands]      = { 0.050f, 0.015f, 0.008f, 0.005f };
    inline constexpr float levelAttackSeconds[numBands]  = { 0.030f, 0.020f, 0.012f, 0.008f };
    inline constexpr float levelReleaseSeconds[numBands] = { 0.400f, 0.300f, 0.200f, 0.150f };
    inline constexpr float impactFastReleaseSeconds[numBands] = { 0.080f, 0.030f, 0.015f, 0.010f };
    inline constexpr float impactSlowTimeSeconds[numBands]    = { 0.150f, 0.100f, 0.060f, 0.040f };
    inline constexpr float impactReleaseBandScale[numBands]   = { 1.5f, 1.15f, 0.85f, 0.6f };

    // Bass Restoration acts on the LF band (below 150 Hz). [chosen]
    inline constexpr int numBassBands = 1;

    // Band-width compensation (see Hepta-Expand below): LF and HF are Tri-Expand's own
    // bands; the split mid bands get their share of Tri-Expand's MF band. [chosen]
    inline constexpr bool hasBandPivotOffsets = true;
    inline constexpr float bandPivotOffsetDb[numBands] = { 0.0f, -2.89f, -3.14f, 0.0f };
    inline constexpr bool hasAutoHeadroom = true;
    inline constexpr bool hasAutoTransition = true;
#else
    // ---- Hepta-Expand: 7 bands ----
    // Crossovers. [chosen] Keeps Tri-Expand's 150 Hz and 6 kHz points, ~1.3 octaves apart.
    inline constexpr float crossoverHz[numBands - 1] = { 60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f };
    inline constexpr const char* bandNames[numBands] = { "SUB", "LOW", "LMID", "MID", "HMID", "PRES", "AIR" };

    // Per-band timing [chosen]: same scheme as Tri-Expand, graded from slow (low bands,
    // long periods) to fast (high bands). Lowest bands need longer RMS windows and
    // impact fast-release so per-cycle ripple doesn't read as level change / transients.
    inline constexpr float rmsTimeSeconds[numBands]      = { 0.080f, 0.050f, 0.025f, 0.012f, 0.008f, 0.005f, 0.005f };
    inline constexpr float levelAttackSeconds[numBands]  = { 0.040f, 0.030f, 0.020f, 0.015f, 0.012f, 0.010f, 0.008f };
    inline constexpr float levelReleaseSeconds[numBands] = { 0.450f, 0.400f, 0.300f, 0.250f, 0.200f, 0.170f, 0.150f };
    inline constexpr float impactFastReleaseSeconds[numBands] = { 0.100f, 0.080f, 0.040f, 0.020f, 0.015f, 0.012f, 0.010f };
    inline constexpr float impactSlowTimeSeconds[numBands]    = { 0.200f, 0.150f, 0.100f, 0.080f, 0.060f, 0.050f, 0.040f };
    inline constexpr float impactReleaseBandScale[numBands]   = { 1.6f, 1.5f, 1.25f, 1.0f, 0.85f, 0.7f, 0.6f };

    // Bass Restoration acts on the bands below 150 Hz: SUB and LOW. [chosen]
    inline constexpr int numBassBands = 2;

    // Band-width compensation. A narrow band carries less of the programme's energy
    // than a wide one, so against the same Transition level it reads as "quiet" and is
    // expanded down: without this, Hepta-Expand pulled 60 Hz-6 kHz ~3 dB further down
    // than Tri-Expand on the same material, dulling the mids and presence. Each band's
    // pivot is lowered by its share of the Tri-Expand band that contains it, for a pink
    // spectrum over 20 Hz-20 kHz: 10*log10(ln(hi/lo) / ln(HI/LO)). Each band then
    // behaves like the corresponding Tri-Expand band. [chosen]
    inline constexpr bool hasBandPivotOffsets = true;
    inline constexpr bool hasAutoHeadroom = true;
    inline constexpr bool hasAutoTransition = true;
    inline constexpr float bandPivotOffsetDb[numBands] = { -2.63f, -3.42f, -5.75f, -6.05f, -6.05f, -6.25f, 0.0f };
#endif
    // Bass Restoration: extra expansion ratio added to the bass bands at 100 %
    // (on top of the global Expansion; 50 % Expansion + 100 % Bass = 1:2 in the bass). [chosen]
    inline constexpr float maxBassExtraRatio = 0.5f;
    static_assert (numBassBands < numBands, "bass bands must leave at least one other band");
    // The HF Transition trim applies to the top band.

    // Expansion ratio range. [manual: 1:1 to 1:1.5, "0 to 50%", each band]
    inline constexpr float maxExpansionPercent = 50.0f;

    // Expansion gain limits in dB. [chosen] The hardware meters span -20..+12 dB,
    // but the manual says downward expansion goes further than the meters show.
    inline constexpr float maxUpwardExpansionDb   = 12.0f;
    inline constexpr float maxDownwardExpansionDb = -40.0f;

    // Level detection per band (LF, MF, HF), two stages. [chosen]
    // Manual only says attack/release are "program-dependent, optimized for each band".
    // 1) True RMS: symmetric averaging of x^2, long enough to cover the band's
    //    lowest period, so a steady tone reads its real RMS level.
    //    (rmsTimeSeconds, per band: see the band tables at the top.)
    // 2) Ballistics on that level in dB: release much slower than attack, so the
    //    gain follows the programme's loudness rather than individual beats
    //    (a single fast symmetric detector pumped on drums).
    //    (levelAttackSeconds / levelReleaseSeconds, per band: see the band tables.)

    // Impact restoration. [manual: upward only, up to +12 dB each band]
    inline constexpr float maxImpactDb = 12.0f;
    // dB of boost per dB that the fast envelope exceeds the slow one (after the deadband).
    // The Impact control scales the MAXIMUM boost (100% = maxImpactDb), not this. [chosen]
    inline constexpr float impactSensitivity = 0.5f;
    // Transient detection, per band. [chosen]
    // A fast peak follower on |x| is converted to dB; the "slow" envelope is that
    // dB value smoothed further. Steady signals give fast ~= slow (no boost);
    // sudden rises give fast > slow. Fast release must be long relative to the
    // band's lowest period, or per-cycle ripple reads as a transient.
    inline constexpr float impactFastAttackSeconds  = 0.0005f;
    // (impactFastReleaseSeconds / impactSlowTimeSeconds, per band: see the band tables.)
    // Rises smaller than this (dB) are ignored, so residual ripple doesn't pump. [chosen]
    inline constexpr float impactDeadbandDb = 2.0f;
    // Per-band multiplier on the user's Impact Release time. [chosen]
    // (impactReleaseBandScale, per band: see the band tables.)

    // Auto Headroom (Hepta-Expand). Tracks the input's and the expander output's peak
    // envelopes and turns the output down by however much the expander raises the
    // peaks, so active and bypassed peaks stay about level. Down only. [chosen]
    // Deliberately slow: it sets an overall output level rather than reacting to
    // individual hits. A 50 ms / 3 s version pumped ~1.3 dB with the beat on hits of
    // varying strength. The first hits after a quiet passage can briefly go over until
    // it settles (Soft Clip catches those; the OVER LED shows them).
    inline constexpr float headroomPeakReleaseSeconds = 3.0f;  // peak envelopes: instant attack, this release
    inline constexpr float headroomTrimAttackSeconds  = 1.0f;  // trim moves down this fast
    inline constexpr float headroomTrimReleaseSeconds = 8.0f;  // ...and recovers this slowly
    inline constexpr float headroomMaxTrimDb          = -12.0f;
    // The trim holds still while the target is within this of it, so hit-to-hit
    // variation in peak level doesn't make it wobble. [chosen]
    inline constexpr float headroomDeadbandDb         = 0.5f;

    // Auto Transition (Hepta-Expand). Each band's pivot follows that band's own
    // long-term level, so the expander works around the music's average at any
    // playback level; Transition becomes an offset from it (its centre value,
    // autoTransitionCentreDb, = the average). HF / Bass Transition still add on top;
    // band-width compensation isn't needed (each band uses its own average). [chosen]
    inline constexpr float autoTransitionSeconds  = 8.0f;   // long-term average time constant (dB domain)
    inline constexpr float autoTransitionGateDb   = -70.0f; // band levels below this don't update the average
    inline constexpr float autoTransitionCentreDb = -20.0f; // Transition value meaning "at the average"
    // For this long after sound first appears, the average uses autoTransitionInitTau so
    // it locks onto the real level instead of crawling up from where the (still rising)
    // level detector first crossed the gate.
    inline constexpr float autoTransitionInitSeconds = 1.0f;
    inline constexpr float autoTransitionInitTau     = 0.25f;

    // Parameter smoothing time, seconds. [chosen]
    inline constexpr double parameterSmoothingSeconds = 0.05;

    // Soft clipper: linear below this level, then a smooth knee up to a ceiling of 1.0 (0 dBFS). [chosen]
    inline constexpr float clipperKnee = 0.891f; // -1 dBFS

    // Floor for level detection to avoid log(0). [chosen]
    inline constexpr float detectorFloor = 1.0e-10f; // -100 dB (power)
}
