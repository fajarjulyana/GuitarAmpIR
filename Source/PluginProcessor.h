#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AmpProcessor.h"
#include "ConvolutionEngine.h"
#include "IRLoader.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Parameter IDs — string constants shared between processor and editor
// ─────────────────────────────────────────────────────────────────────────────
namespace ParamID
{
    inline constexpr const char* InputGain  = "inputGain";
    inline constexpr const char* MasterVol  = "masterVol";
    inline constexpr const char* Bass       = "bass";
    inline constexpr const char* Mid        = "mid";
    inline constexpr const char* Treble     = "treble";
    inline constexpr const char* Presence   = "presence";
    inline constexpr const char* CabEnabled = "cabEnabled";
}

// ─────────────────────────────────────────────────────────────────────────────
class GuitarAmpSimProcessor  : public juce::AudioProcessor,
                                public juce::AudioProcessorValueTreeState::Listener
{
public:
    GuitarAmpSimProcessor();
    ~GuitarAmpSimProcessor() override;

    // ── AudioProcessor overrides ─────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GuitarAmpIR"; }

    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Parameter tree ────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;

    // ── IR management ─────────────────────────────────────────────────────
    /** Trigger file browser (call from message thread only). */
    void browseForIR (juce::Component* parent);

    /** Returns path of the last loaded IR, or empty string. */
    juce::String getIRPath() const { return irLoader.getLastLoadedPath(); }

    bool hasIR() const noexcept { return convEngine.hasIR(); }

    // ── Metering (peak, updated per block) ────────────────────────────────
    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

private:
    // ── APVTS factory ─────────────────────────────────────────────────────
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── AudioProcessorValueTreeState::Listener ────────────────────────────
    void parameterChanged (const juce::String& paramID, float newValue) override;

    // ── DSP chain ─────────────────────────────────────────────────────────
    AmpProcessor      ampProcessor;
    ConvolutionEngine convEngine;
    IRLoader          irLoader;

    // IR path persisted in plugin state
    juce::String savedIRPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarAmpSimProcessor)
};
