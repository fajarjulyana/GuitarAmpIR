#include "AmpProcessor.h"
#include <cmath>

AmpProcessor::AmpProcessor() = default;

void AmpProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    // Smooth over 20 ms
    const float smoothTime = 0.02f;
    smoothInputGain .reset (sampleRate, smoothTime);
    smoothMasterGain.reset (sampleRate, smoothTime);
    smoothBass      .reset (sampleRate, smoothTime);
    smoothMid       .reset (sampleRate, smoothTime);
    smoothTreble    .reset (sampleRate, smoothTime);
    smoothPresence  .reset (sampleRate, smoothTime);

    smoothInputGain .setCurrentAndTargetValue (1.0f);
    smoothMasterGain.setCurrentAndTargetValue (1.0f);
    smoothBass      .setCurrentAndTargetValue (0.5f);
    smoothMid       .setCurrentAndTargetValue (0.5f);
    smoothTreble    .setCurrentAndTargetValue (0.5f);
    smoothPresence  .setCurrentAndTargetValue (0.5f);

    // Anti-aliasing LPF: 2nd-order Butterworth, fc = sampleRate / 4
    aaFilter.prepare (spec);
    *aaFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate, sampleRate * 0.25, 0.7071f);

    // DC blocker: HPF @ 10 Hz
    dcBlocker.prepare (spec);
    *dcBlocker.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, 10.0f, 0.7071f);

    // Tone stack filters
    bassShelf   .prepare (spec);
    midPeak     .prepare (spec);
    trebleShelf .prepare (spec);
    presencePeak.prepare (spec);

    updateToneFilters (0.5f, 0.5f, 0.5f, 0.5f);

    reset();
}

void AmpProcessor::reset()
{
    aaFilter    .reset();
    dcBlocker   .reset();
    bassShelf   .reset();
    midPeak     .reset();
    trebleShelf .reset();
    presencePeak.reset();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sample-by-sample processing
// ─────────────────────────────────────────────────────────────────────────────

float AmpProcessor::processSample (float x) noexcept
{
    // 1. Input gain
    float inGain  = smoothInputGain .getNextValue();
    float outGain = smoothMasterGain.getNextValue();

    float bass     = smoothBass    .getNextValue();
    float mid      = smoothMid     .getNextValue();
    float treble   = smoothTreble  .getNextValue();
    float presence = smoothPresence.getNextValue();

    // Update tone filters if any parameter changed
    if (bass     != prevBass     || mid     != prevMid   ||
        treble   != prevTreble   || presence != prevPresence)
    {
        updateToneFilters (bass, mid, treble, presence);
        prevBass     = bass;
        prevMid      = mid;
        prevTreble   = treble;
        prevPresence = presence;
    }

    // 2. Makeup gain before clipping
    x *= inGain;

    // 3. Anti-aliasing LPF (avoids foldback from the clipper)
    x = aaFilter.processSample (x);

    // 4. Triode nonlinearity
    x = triodeClip (x);

    // 5. DC blocker
    x = dcBlocker.processSample (x);

    // 6. Passive tone stack
    x = bassShelf   .processSample (x);
    x = midPeak     .processSample (x);
    x = trebleShelf .processSample (x);
    x = presencePeak.processSample (x);

    // 7. Master volume
    x *= outGain;

    return x;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parameter setters
// ─────────────────────────────────────────────────────────────────────────────

void AmpProcessor::setInputGain (float dB) noexcept
{
    smoothInputGain.setTargetValue (juce::Decibels::decibelsToGain (dB));
}

void AmpProcessor::setMasterGain (float dB) noexcept
{
    smoothMasterGain.setTargetValue (juce::Decibels::decibelsToGain (dB));
}

void AmpProcessor::setBass (float norm) noexcept
{
    smoothBass.setTargetValue (juce::jlimit (0.0f, 1.0f, norm));
}

void AmpProcessor::setMid (float norm) noexcept
{
    smoothMid.setTargetValue (juce::jlimit (0.0f, 1.0f, norm));
}

void AmpProcessor::setTreble (float norm) noexcept
{
    smoothTreble.setTargetValue (juce::jlimit (0.0f, 1.0f, norm));
}

void AmpProcessor::setPresence (float norm) noexcept
{
    smoothPresence.setTargetValue (juce::jlimit (0.0f, 1.0f, norm));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tone stack coefficient update
// ─────────────────────────────────────────────────────────────────────────────

void AmpProcessor::updateToneFilters (float bass, float mid, float treble, float presence)
{
    // Bass shelf: centre at 200 Hz, ±15 dB range
    float bassDb   = (bass   - 0.5f) * 30.0f;
    float midDb    = (mid    - 0.5f) * 20.0f;
    float trebleDb = (treble - 0.5f) * 20.0f;
    float presDb   = (presence - 0.5f) * 16.0f;

    *bassShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        sampleRate, 200.0f, 0.7071f, juce::Decibels::decibelsToGain (bassDb));

    *midPeak.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 800.0f, 1.2f, juce::Decibels::decibelsToGain (midDb));

    *trebleShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
        sampleRate, 3200.0f, 0.7071f, juce::Decibels::decibelsToGain (trebleDb));

    *presencePeak.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
        sampleRate, 5000.0f, 1.5f, juce::Decibels::decibelsToGain (presDb));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Triode soft-clipper
// ─────────────────────────────────────────────────────────────────────────────

float AmpProcessor::triodeClip (float x) noexcept
{
    // Asymmetric waveshaper inspired by Pirkle (2019) and Yeh et al. (2010).
    //
    //  Positive half (grid conduction onset ~0.7 V):
    //    f(x) = 1 - exp(-x)   for x >= 0       → smooth saturation
    //
    //  Negative half (cathode cutoff ~-1.2 V):
    //    f(x) = -1 + exp(x/1.2) · tanh correction
    //
    // We normalise by the hard ceiling to keep unity gain at small signals.

    constexpr float positiveKnee = 0.7f;
    constexpr float negativeKnee = 1.2f;

    if (x >= 0.0f)
    {
        // Soft clip with gain compensation so near-zero gain ≈ 1
        return (1.0f - std::exp (-x * positiveKnee)) / positiveKnee;
    }
    else
    {
        // Harder clip on the negative rail (triode cuts off sharper)
        float y = -(1.0f - std::exp (x / negativeKnee));
        return y;
    }
}
