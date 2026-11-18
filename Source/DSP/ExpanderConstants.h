#pragma once

// Tunable constants for the 3-band expander.
//
// Values marked [manual] come from the original hardware owner's manual.
// Values marked [chosen] are NOT documented there; they are our own design
// choices and are the first things to adjust by ear.

namespace fourbx::constants
{
    inline constexpr int numBands = 3;

    // Crossover frequencies in Hz. [manual: LF < ~150 Hz, MF ~150 Hz-6 kHz, HF > ~6 kHz]
    inline constexpr float crossoverLowMidHz  = 150.0f;
    inline constexpr float crossoverMidHighHz = 6000.0f;

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
    inline constexpr float rmsTimeSeconds[numBands] = { 0.050f, 0.010f, 0.005f };
    // 2) Ballistics on that level in dB: release much slower than attack, so the
    //    gain follows the programme's loudness rather than individual beats
    //    (a single fast symmetric detector pumped on drums).
    inline constexpr float levelAttackSeconds[numBands]  = { 0.030f, 0.015f, 0.008f };
    inline constexpr float levelReleaseSeconds[numBands] = { 0.400f, 0.250f, 0.150f };

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
    inline constexpr float impactFastReleaseSeconds[numBands] = { 0.080f, 0.020f, 0.010f };
    inline constexpr float impactSlowTimeSeconds[numBands]    = { 0.150f, 0.080f, 0.040f };
    // Rises smaller than this (dB) are ignored, so residual ripple doesn't pump. [chosen]
    inline constexpr float impactDeadbandDb = 2.0f;
    // Per-band multiplier on the user's Impact Release time. [chosen]
    inline constexpr float impactReleaseBandScale[numBands] = { 1.5f, 1.0f, 0.6f };

    // Parameter smoothing time, seconds. [chosen]
    inline constexpr double parameterSmoothingSeconds = 0.05;

    // Soft clipper: linear below this level, then a smooth knee up to a ceiling of 1.0 (0 dBFS). [chosen]
    inline constexpr float clipperKnee = 0.891f; // -1 dBFS

    // Floor for level detection to avoid log(0). [chosen]
    inline constexpr float detectorFloor = 1.0e-10f; // -100 dB (power)
}
