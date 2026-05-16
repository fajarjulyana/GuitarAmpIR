#include "PluginProcessor.h"
#include "PluginEditor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Parameter layout
// ─────────────────────────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
GuitarAmpSimProcessor::createParameterLayout()
{
    using APVTS  = juce::AudioProcessorValueTreeState;
    using Param  = juce::AudioParameterFloat;
    using BParam = juce::AudioParameterBool;
    using Range  = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Param> (
        ParamID::InputGain, "Input Gain",
        Range (-12.0f, 36.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withLabel ("dB")));

    params.push_back (std::make_unique<Param> (
        ParamID::MasterVol, "Master Volume",
        Range (-60.0f, 0.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes{}.withLabel ("dB")));

    params.push_back (std::make_unique<Param> (
        ParamID::Bass,    "Bass",    Range (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<Param> (
        ParamID::Mid,     "Mid",     Range (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<Param> (
        ParamID::Treble,  "Treble",  Range (0.0f, 1.0f, 0.01f), 0.5f));
    params.push_back (std::make_unique<Param> (
        ParamID::Presence,"Presence",Range (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<BParam> (
        ParamID::CabEnabled, "Cabinet Sim", true));

    return { params.begin(), params.end() };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

GuitarAmpSimProcessor::GuitarAmpSimProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GuitarAmpSim", createParameterLayout())
{
    apvts.addParameterListener (ParamID::InputGain,  this);
    apvts.addParameterListener (ParamID::MasterVol,  this);
    apvts.addParameterListener (ParamID::Bass,       this);
    apvts.addParameterListener (ParamID::Mid,        this);
    apvts.addParameterListener (ParamID::Treble,     this);
    apvts.addParameterListener (ParamID::Presence,   this);
    apvts.addParameterListener (ParamID::CabEnabled, this);

    // Wire IR loader callback → convolution engine
    irLoader.onIRLoaded = [this] (const juce::AudioBuffer<float>& buf, double sr)
    {
        convEngine.loadIR (buf, sr);
        savedIRPath = irLoader.getLastLoadedPath();
    };

    irLoader.onLoadError = [] (const juce::String& msg)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon, "IR Load Error", msg);
    };
}

GuitarAmpSimProcessor::~GuitarAmpSimProcessor()
{
    apvts.removeParameterListener (ParamID::InputGain,  this);
    apvts.removeParameterListener (ParamID::MasterVol,  this);
    apvts.removeParameterListener (ParamID::Bass,       this);
    apvts.removeParameterListener (ParamID::Mid,        this);
    apvts.removeParameterListener (ParamID::Treble,     this);
    apvts.removeParameterListener (ParamID::Presence,   this);
    apvts.removeParameterListener (ParamID::CabEnabled, this);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Prepare / Release
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels      = 1;   // mono processing chain

    ampProcessor.prepare (spec);
    convEngine  .prepare (spec);
    ampProcessor.reset();
    convEngine  .reset();

    // Apply current parameter values
    ampProcessor.setInputGain  (*apvts.getRawParameterValue (ParamID::InputGain));
    ampProcessor.setMasterGain (*apvts.getRawParameterValue (ParamID::MasterVol));
    ampProcessor.setBass       (*apvts.getRawParameterValue (ParamID::Bass));
    ampProcessor.setMid        (*apvts.getRawParameterValue (ParamID::Mid));
    ampProcessor.setTreble     (*apvts.getRawParameterValue (ParamID::Treble));
    ampProcessor.setPresence   (*apvts.getRawParameterValue (ParamID::Presence));

    // Re-load saved IR (e.g. after plugin reload)
    if (savedIRPath.isNotEmpty())
        irLoader.loadFromFile (juce::File (savedIRPath));

    inputLevel .store (0.0f);
    outputLevel.store (0.0f);
}

void GuitarAmpSimProcessor::releaseResources()
{
    convEngine.reset();
    ampProcessor.reset();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Bus layout
// ─────────────────────────────────────────────────────────────────────────────

bool GuitarAmpSimProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Accept mono-in / mono-out or mono-in / stereo-out
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::mono())
        return false;
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() ||
           out == juce::AudioChannelSet::stereo();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Audio processing
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numOutChans = buffer.getNumChannels();

    // Use channel 0 as the mono working buffer
    float* mono = buffer.getWritePointer (0);

    // ── Input metering ────────────────────────────────────────────────────
    float inPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inPeak = std::max (inPeak, std::abs (mono[i]));
    inputLevel.store (inPeak);

    // ── Amp DSP (sample-by-sample) ────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
        mono[i] = ampProcessor.processSample (mono[i]);

    // ── Cabinet IR convolution ────────────────────────────────────────────
    const bool cabOn = *apvts.getRawParameterValue (ParamID::CabEnabled) > 0.5f;
    if (cabOn)
        convEngine.process (mono, numSamples);

    // ── Copy mono to stereo output if needed ─────────────────────────────
    if (numOutChans > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // ── Output metering ───────────────────────────────────────────────────
    float outPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        outPeak = std::max (outPeak, std::abs (mono[i]));
    outputLevel.store (outPeak);
}

// ─────────────────────────────────────────────────────────────────────────────
//  State persistence
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("irPath", savedIRPath, nullptr);
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GuitarAmpSimProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        savedIRPath = state.getProperty ("irPath", "").toString();
        apvts.replaceState (state);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Parameter change listener
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimProcessor::parameterChanged (const juce::String& paramID, float value)
{
    if      (paramID == ParamID::InputGain)  ampProcessor.setInputGain  (value);
    else if (paramID == ParamID::MasterVol)  ampProcessor.setMasterGain (value);
    else if (paramID == ParamID::Bass)       ampProcessor.setBass       (value);
    else if (paramID == ParamID::Mid)        ampProcessor.setMid        (value);
    else if (paramID == ParamID::Treble)     ampProcessor.setTreble     (value);
    else if (paramID == ParamID::Presence)   ampProcessor.setPresence   (value);
    // CabEnabled is read directly in processBlock
}

// ─────────────────────────────────────────────────────────────────────────────
//  IR management
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimProcessor::browseForIR (juce::Component* parent)
{
    irLoader.browseAndLoad (parent);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Editor
// ─────────────────────────────────────────────────────────────────────────────

juce::AudioProcessorEditor* GuitarAmpSimProcessor::createEditor()
{
    return new GuitarAmpSimEditor (*this);
}

// ─────────────────────────────────────────────────────────────────────────────
//  JUCE plugin entry point
// ─────────────────────────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarAmpSimProcessor();
}
