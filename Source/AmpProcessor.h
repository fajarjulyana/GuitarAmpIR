#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>

/**
 * AmpProcessor
 * ------------
 * Models the preamp and tone stack of a classic cathode-biased triode stage
 * (think Fender/Marshall-inspired topology).
 *
 * Signal chain (mono):
 *
 *   Input
 *     → Input gain (makeup gain before clipping)
 *     → Anti-aliasing LPF (2nd-order Butterworth @ Nyquist/4)
 *     → Triode nonlinearity  (asymmetric soft-clipping)
 *     → DC blocking filter   (1st-order HPF @ 10 Hz)
 *     → Passive tone stack   (Bass / Mid / Treble shelves + peak)
 *     → Master volume
 *   Output
 *
 * The IR cabinet sim lives in ConvolutionEngine and is applied after this.
 */
class AmpProcessor
{
public:
    AmpProcessor();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Process a single mono sample through the amp model. */
    float processSample (float x) noexcept;

    // ── Parameter setters (thread-safe via atomic smoothing) ──────────────
    void setInputGain  (float dB)    noexcept;  // -12 .. +36 dB
    void setMasterGain (float dB)    noexcept;  // -60 .. 0 dB
    void setBass       (float norm)  noexcept;  //  0..1
    void setMid        (float norm)  noexcept;  //  0..1
    void setTreble     (float norm)  noexcept;  //  0..1
    void setPresence   (float norm)  noexcept;  //  0..1

private:
    double sampleRate = 44100.0;

    // ── Smooth parameters ─────────────────────────────────────────────────
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothInputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothMasterGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothBass;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothMid;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothTreble;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothPresence;

    // ── DSP primitives ────────────────────────────────────────────────────

    // Anti-aliasing LPF (2nd-order Butterworth, fc = sampleRate / 4)
    juce::dsp::IIR::Filter<float> aaFilter;

    // DC blocker (bilinear HPF @ 10 Hz)
    juce::dsp::IIR::Filter<float> dcBlocker;

    // Tone-stack filters — all 2nd-order biquads updated each block
    juce::dsp::IIR::Filter<float> bassShelf;
    juce::dsp::IIR::Filter<float> midPeak;
    juce::dsp::IIR::Filter<float> trebleShelf;
    juce::dsp::IIR::Filter<float> presencePeak;

    // Track previous tone values to avoid unnecessary coefficient updates
    float prevBass     = -1.0f;
    float prevMid      = -1.0f;
    float prevTreble   = -1.0f;
    float prevPresence = -1.0f;

    // ── Internal methods ──────────────────────────────────────────────────
    void updateToneFilters (float bass, float mid, float treble, float presence);

    /** Asymmetric soft-clip modelling a triode grid/cathode:
     *    positive half: soft knee at ~0.7 (grid conduction)
     *    negative half: harder knee at ~1.2 (cutoff)  */
    static float triodeClip (float x) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpProcessor)
};
