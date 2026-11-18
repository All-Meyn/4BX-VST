# 4BX-VST

A 3-band dynamic-range expander with transient "impact" enhancement, modelled on
the behaviour described in the owner's manual of a classic 1980s hi-fi expander.
VST3 + Standalone, built with [JUCE](https://juce.com) 9 and CMake.

## Controls

| Control | Range | What it does |
|---|---|---|
| Expansion | 0–50 % | Ratio 1:1 to 1:1.5 in every band. Gain = (ratio − 1) × (band RMS level − Transition). |
| Transition | −40 to 0 dBFS | The unity-gain pivot: louder material is expanded up, quieter material down. |
| HF Trim | ±10 dB | Offsets the pivot of the high band (above ~6 kHz) only. Raise it to tame hiss/cymbals. |
| Impact | 0–100 % | Upward-only boost (up to +12 dB per band) on sudden attacks. |
| Impact Release | 25–1500 ms | How fast the impact boost decays (scaled ×1.5 on LF, ×0.6 on HF). |
| Volume | −40 to +10 dB | Output level. |
| Bypass | on/off | Removes expansion and impact; Volume still applies (as on the hardware). |
| Soft Clip | on/off | Smooth limiting above −1 dBFS so peaks never exceed 0 dBFS. Off by default. |

### Interface

The window is styled as a hardware faceplate and scales with a fixed aspect ratio.
Volume, Expansion, Transition and Impact use ▼/▲ buttons (hold to repeat) with an
LED column as the readout; you can also drag or scroll a column, or double-click it
to type a value. **REAR PANEL** flips to the back, which holds Impact Release,
HF Transition and Soft Clip. **DISPLAY** sets LED brightness (saved with the session).

Bands: LF < 150 Hz, MF 150 Hz–6 kHz, HF > 6 kHz (Linkwitz-Riley 24 dB/oct; sums flat).
Stereo channels share one detector per band. Meters: yellow = downward expansion
to −20 dB, red = upward expansion to +12 dB; IMP = impact boost (loudest band).

All tunable constants live in `Source/DSP/ExpanderConstants.h`, each marked as
taken from the manual or chosen by us.

## Licensing

JUCE (9.0.5) is downloaded automatically at configure time. Its modules are
dual-licensed under the **AGPLv3** or a commercial JUCE licence. This project's
own code is GPLv3 (see `LICENSE`); GPLv3 permits combining it with AGPLv3 code,
and a distributed binary built with the free JUCE licence must meet the AGPLv3
terms (including making the complete source available). To ship closed-source,
you need a commercial JUCE licence. This is a summary, not legal advice.

## Build

Requirements: CMake 3.22+ (4.2+ for the Visual Studio 2026 generator),
a C++17 compiler, Ninja (optional).
On Linux, JUCE also needs: `libasound2-dev libjack-jackd2-dev libfreetype-dev
libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
libxinerama-dev libxrandr-dev libxrender-dev libxi-dev libgl1-mesa-dev libgtk-3-dev`.

After changing the JUCE version, delete the `build` folder (or run a fresh
configure) so CMake fetches the new release.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/FourBX_artefacts/Release/VST3/4BX.vst3` and
`build/FourBX_artefacts/Release/Standalone/4BX`.

## Tests

An offline test program checks the DSP (flat crossover sum, expansion law,
gain limits, HF trim, impact behaviour, bypass, soft clip, mono):

```bash
cmake --build build --target FourBXTests
./build/FourBXTests_artefacts/Release/FourBXTests
```

Turn it off with `-DFOURBX_BUILD_TESTS=OFF`.

## Layout

- `Source/DSP/ThreeBandExpander.*` — the DSP (no plugin dependencies)
- `Source/DSP/ExpanderConstants.h` — crossover points, time constants, limits
- `Source/PluginProcessor.*` — parameters, state, host glue
- `Source/PluginEditor.*` — window scaling, panel flip, meter updates
- `Source/UI/` — faceplate widgets (LEDs, buttons, pots) and front/rear panels
- `tests/DspTests.cpp` — offline DSP tests
