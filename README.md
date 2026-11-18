# 4BX-VST

Audio plugin (VST3 + Standalone) built with [JUCE](https://juce.com) 8 and CMake.
JUCE is downloaded automatically at configure time; it's licensed to this
project under the GPLv3 (see `LICENSE`).

## Build

Requirements: CMake 3.22+, a C++17 compiler, Ninja (optional).
On Linux, JUCE also needs: `libasound2-dev libjack-jackd2-dev libfreetype-dev
libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev
libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev libgtk-3-dev`.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Output: `build/FourBX_artefacts/Release/VST3/4BX.vst3` and
`build/FourBX_artefacts/Release/Standalone/4BX`.

## Layout

- `Source/PluginProcessor.*` — audio processing and parameters
- `Source/PluginEditor.*` — plugin UI
