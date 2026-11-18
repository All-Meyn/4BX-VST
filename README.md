# Tri-Expand & Hepta-Expand

Multi-band dynamic-range expanders with transient "impact" enhancement, modelled on
the behaviour described in the owner's manual of a classic 1980s hi-fi expander.
VST3 + Standalone, built with [JUCE](https://juce.com) 9 and CMake.

| Plugin | Bands | Crossovers |
|---|---|---|
| **Tri-Expand** | 3: LF / MF / HF | 150 Hz, 6 kHz |
| **Hepta-Expand** | 7: SUB / LOW / LMID / MID / HMID / PRES / AIR | 60, 150, 400 Hz, 1, 2.5, 6 kHz |

Both share all code and controls; the band count is fixed per plugin at build time
(`EXPANDER_NUM_BANDS`). They are separate plugins to hosts (different plugin codes).

Narrow bands carry less of the programme's energy than wide ones, so Hepta-Expand
lowers each band's pivot by that band's share of the Tri-Expand band containing it
(pink-spectrum reference; SUB −2.6, LOW −3.4, LMID −5.8, MID/HMID −6.1, PRES −6.3,
AIR 0 dB). Each band then behaves like the corresponding Tri-Expand band, keeping
the same tonal balance; without it the mids and presence were pulled ~3 dB further
down. Offsets live in `ExpanderConstants.h` (`bandPivotOffsetDb`).

## Controls

| Control | Range | What it does |
|---|---|---|
| Expansion | 0–50 % | Ratio 1:1 to 1:1.5 in every band. Gain = (ratio − 1) × (band RMS level − Transition). |
| Transition | −40 to 0 dBFS | The unity-gain pivot: louder material is expanded up, quieter material down. |
| HF Trim | ±10 dB | Offsets the pivot of the top band (above 6 kHz) only. Raise it to tame hiss/cymbals. |
| Impact | 0–100 % | Upward-only boost on sudden attacks; the % sets the maximum boost per band (50 % = +6 dB, 100 % = +12 dB). |
| Impact Release | 25–1500 ms | How fast the impact boost decays (scaled per band: slower on low bands, faster on high). |
| Volume | −40 to +10 dB | Output level. |
| Bypass | on/off | Removes expansion and impact; Volume still applies (as on the hardware). |
| Soft Clip | on/off | Smooth limiting above −1 dBFS so peaks never exceed 0 dBFS. Off by default. |
| Bass Restoration *(Hepta-Expand)* | 0–100 % | Extra expansion in the bass bands (SUB + LOW, below 150 Hz), on top of Expansion: 100 % adds +0.5 to the ratio (50 % Expansion + 100 % Bass = 1:2 in the bass). Bass above its pivot gets louder, bass below it quieter. 0 % = off. |
| Bass Transition *(Hepta-Expand, rear)* | ±10 dB | Offsets the pivot of the bass bands only. Lower it to push more of the bass upward. |
| Auto Headroom *(Hepta-Expand, rear)* | on/off, **on** | Turns the output down by however much the expander raises the peaks (input vs output peak envelopes; down only, up to −12 dB, ~50 ms down / ~3 s recovery), so loud masters aren't pushed into clipping after the plugin. Rear panel shows the current trim. |
| OVER LED *(Hepta-Expand)* | — | Lights for ~1 s when the plugin's output exceeds 0 dBFS: lower Volume or keep Auto Headroom on. |

### Interface

The window is styled as a hardware faceplate and scales with a fixed aspect ratio.
Volume, Expansion, Transition and Impact use ▼/▲ buttons (hold to repeat) with an
LED column as the readout; you can also drag or scroll a column, or double-click it
to type a value. **REAR PANEL** flips to the back, which holds Impact Release,
HF Transition, Soft Clip and (on Windows) **GPU DRAW**. **DISPLAY** sets LED
brightness (saved with the session).

**GPU DRAW** (Windows) chooses Direct2D or software drawing. It defaults to off in
Audacity: Audacity 4 covers the plugin window with one of its own during Preview,
which shows as a white panel with GPU drawing and as a faded panel without. Your
choice with the switch is saved with the session.

Crossovers are Linkwitz-Riley 24 dB/oct, phase-aligned so the bands sum flat.
Stereo channels share one detector per band. Hepta-Expand's faceplate is taller,
with one gain-change row per band, plus a Bass Restoration meter row showing the
extra gain it adds to the bass (yellow = down, red = up). Meters: yellow = downward expansion
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

Output:
- `build/TriExpand_artefacts/Release/VST3/Tri-Expand.vst3` and `.../Standalone/Tri-Expand`
- `build/HeptaExpand_artefacts/Release/VST3/Hepta-Expand.vst3` and `.../Standalone/Hepta-Expand`

Build just one with `--target TriExpand_VST3` or `--target HeptaExpand_VST3`.

## Tests

Offline test programs check the DSP for each band count (flat crossover sum,
per-band expansion law, gain limits, HF trim, impact behaviour, bypass, soft clip, mono):

```bash
cmake --build build --target TriExpandTests HeptaExpandTests
./build/TriExpandTests_artefacts/Release/TriExpandTests
./build/HeptaExpandTests_artefacts/Release/HeptaExpandTests
```

Turn it off with `-DTRIEXPAND_BUILD_TESTS=OFF`.

## Layout

- `Source/DSP/MultiBandExpander.*` — the DSP (no plugin dependencies)
- `Source/DSP/ExpanderConstants.h` — band count, crossover points, per-band time constants, limits
- `Source/PluginProcessor.*` — parameters, state, host glue
- `Source/PluginEditor.*` — window scaling, panel flip, meter updates
- `Source/UI/` — faceplate widgets (LEDs, buttons, pots) and front/rear panels
- `tests/DspTests.cpp` — offline DSP tests
