# GuitarAmpIR

A cross-platform guitar amplifier simulator plugin with a built-in FFT convolution IR (Impulse Response) cabinet loader, inspired by the LePou plugin series.

![GuitarAmpIR User Interface](capture/Cuplikan%20layar%20dari%202026-05-17%2003-33-21.png)

**Formats:** VST3 · LV2 · Standalone 
**Platforms:** Linux · macOS · Windows 
**Framework:** JUCE 7 (fetched automatically via CMake)

---

## Signal Chain


```

Input
→ Input Gain (makeup gain, -12 .. +36 dB)
→ Anti-aliasing LPF  (2nd-order Butterworth @ Nyquist/4)
→ Triode nonlinearity (asymmetric soft-clipper — positive knee 0.7, negative 1.2)
→ DC blocker (HPF @ 10 Hz)
→ Passive tone stack (Bass shelf 200 Hz · Mid peak 800 Hz · Treble shelf 3.2 kHz · Presence peak 5 kHz)
→ Master Volume (-60 .. 0 dB)
→ [optional] Convolution cabinet (FFT overlap-add, uniform partitioning)
Output (mono → stereo)

```

---

## Building

### Prerequisites

| Platform | Requirement |
|----------|-------------|
| All      | CMake ≥ 3.22, Git, C++17 compiler |
| Linux    | `sudo apt install build-essential libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev lv2-dev` |
| macOS    | Xcode CLT (`xcode-select --install`), `brew install lv2` |
| Windows  | Visual Studio 2022 (MSVC) or MinGW-w64, download LV2 headers and pass `-DLV2_PATH=<path>` |

### Quick start

```bash
# 1. Clone
git clone [https://github.com/fajarjulyana/GuitarAmpIR.git](https://github.com/fajarjulyana/GuitarAmpIR.git)
cd GuitarAmpIR

# 2. Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --config Release --parallel

# 4. Install plugins to user plugin directories
cmake --install build

```

Plugins land in:

| Format | Linux | macOS | Windows |
| --- | --- | --- | --- |
| VST3 | `~/.vst3/` | `~/Library/Audio/Plug-Ins/VST3/` | `%PROGRAMFILES%\Common Files\VST3\` |
| LV2 | `~/.lv2/` | `~/Library/Audio/Plug-Ins/LV2/` | `%APPDATA%\LV2\` |

### Standalone app

The Standalone target is built automatically. Find it at:

```
build/GuitarAmpSim_artefacts/Standalone/GuitarAmpIR

```

---

## Project Structure

```
guitar-amp-sim/
├── CMakeLists.txt              # CMake build, JUCE FetchContent, VST3+LV2 targets
├── cmake/
│   └── LV2Support.cmake        # LV2 header discovery + install targets
├── Source/
│   ├── ConvolutionEngine.h/.cpp  # FFT overlap-add convolution (the IR engine)
│   ├── IRLoader.h/.cpp           # WAV/AIFF/FLAC file reader via JUCE formats
│   ├── AmpProcessor.h/.cpp       # Triode preamp + passive tone stack DSP
│   ├── PluginProcessor.h/.cpp    # JUCE AudioProcessor — param tree, state, routing
│   └── PluginEditor.h/.cpp       # Minimalist 2D GUI — knobs, meters, IR button
└── Resources/                  # Drop default IR .wav files here if desired

```

---

## ConvolutionEngine — How It Works

The engine uses **uniformly-partitioned overlap-add** convolution:

1. The IR is split into equal-length blocks (length = `blockSize`, chosen as the smallest power of two ≥ host block size).
2. Each partition is stored as a pre-computed FFT spectrum (`irPartitions`).
3. A frequency-domain delay line (`fdlSpectra`) holds the FFT of recent input blocks.
4. On each audio block:
* Forward-FFT the incoming audio block.
* Multiply against all IR partition spectra and sum → output spectrum.
* Inverse-FFT the output spectrum.
* Add the previous OLA tail; save the new tail.


5. IR updates are double-buffered via `SpinLock`-guarded swap — the audio thread never blocks on load.

**Latency:** exactly one block (zero look-ahead).

**Complexity:** $O(N \cdot P \cdot \log N)$ per block, where $P$ = number of IR partitions.

---

## Tone Stack Parameters

| Control | Filter type | Centre freq | Range |
| --- | --- | --- | --- |
| Bass | Low shelf | 200 Hz | ±15 dB |
| Mid | Peak (Q 1.2) | 800 Hz | ±10 dB |
| Treble | High shelf | 3.2 kHz | ±10 dB |
| Presence | Peak (Q 1.5) | 5.0 kHz | ±8 dB |

All biquad coefficients are recomputed only when a parameter actually changes, and transitions use 20 ms linear smoothing to avoid clicks.

---

## Loading an IR

1. Enable the **CAB** toggle in the plugin GUI.
2. Click **LOAD IR** and select any `.wav`, `.aif`, `.aiff`, or `.flac` file.
3. Files are automatically mixed to mono and resampled to the host sample rate.
4. Files longer than 10 seconds are truncated; IRs longer than 4 seconds are further capped inside the convolution engine.
5. The IR path is saved in the plugin state and reloaded on next session.

---

## Extending the Plugin

### Adding an oversampling stage

Wrap the `triodeClip` call with `juce::dsp::Oversampling<float>`:

```cpp
// In AmpProcessor::prepare()
oversampler.initProcessing(spec.maximumBlockSize);

// In AmpProcessor::processSample() — switch to block processing
auto osBlock = oversampler.processSamplesUp(inputBlock);
// ... apply triodeClip per sample ...
oversampler.processSamplesDown(outputBlock);

```

### Replacing the waveshaper

Swap `triodeClip()` in `AmpProcessor.cpp` for any `float → float` function. A simple alternative using `std::tanh`:

```cpp
static float triodeClip(float x) noexcept {
    return std::tanh(x * 1.4f) / 1.4f;
}

```

### Adding more amp channels

Instantiate a second `AmpProcessor` with its own gain/tone parameters and route via a `channelSelect` parameter.

---

## Authors & Developers

* **Fajar Julyana** — Lead Developer / DSP Engineer
* **QWare, inc.** — Bandung, Indonesia

---

## License

* **Main License:** This project is licensed under the [MIT License](LICENSE) — feel free to use it in open-source and commercial applications.
* **Third-Party Framework:** This project is built using the [JUCE 7 Framework](https://juce.com/), which is subject to its own licensing terms (GPLv3 / Commercial).

```
