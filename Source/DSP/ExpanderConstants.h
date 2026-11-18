#pragma once

// Tunable constants for the multi-band expander.
//
// The band count is chosen per plugin at build time with EXPANDER_NUM_BANDS:
// 3 = Tri-Expand, 7 = Hepta-Expand. Per-band tables below have one entry per band,
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
    static_assert (numBands == 3 || numBands == 7, "EXPANDER_NUM_BANDS must be 3 or 7");

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
#endif
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

    // Parameter smoothing time, seconds. [chosen]
    inline constexpr double parameterSmoothingSeconds = 0.05;

    // Soft clipper: linear below this level, then a smooth knee up to a ceiling of 1.0 (0 dBFS). [chosen]
    inline constexpr float clipperKnee = 0.891f; // -1 dBFS

    // Floor for level detection to avoid log(0). [chosen]
    inline constexpr float detectorFloor = 1.0e-10f; // -100 dB (power)
}
