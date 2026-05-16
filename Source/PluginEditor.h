#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ─────────────────────────────────────────────────────────────────────────────
//  LevelMeter — simple vertical peak meter
// ─────────────────────────────────────────────────────────────────────────────
class LevelMeter : public juce::Component,
                   public juce::Timer
{
public:
    LevelMeter();
    void setLevel (float linearLevel);
    void paint (juce::Graphics& g) override;
    void timerCallback() override;

private:
    float displayLevel = 0.0f;
    float holdLevel    = 0.0f;
    int   holdCounter  = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  KnobLookAndFeel — flat, dark, LePou-inspired style
// ─────────────────────────────────────────────────────────────────────────────
class DarkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DarkLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool highlighted, bool down) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  GuitarAmpSimEditor
// ─────────────────────────────────────────────────────────────────────────────
class GuitarAmpSimEditor : public juce::AudioProcessorEditor,
                            public juce::Timer
{
public:
    explicit GuitarAmpSimEditor (GuitarAmpSimProcessor& p);
    ~GuitarAmpSimEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    GuitarAmpSimProcessor& processor;
    DarkLookAndFeel        darkLaf;

    // ── Controls ───────────────────────────────────────────────────────────
    juce::Slider inputGainKnob;
    juce::Slider masterVolKnob;
    juce::Slider bassKnob;
    juce::Slider midKnob;
    juce::Slider trebleKnob;
    juce::Slider presenceKnob;

    juce::ToggleButton cabToggle { "CAB" };
    juce::TextButton   loadIRBtn { "LOAD IR" };
    juce::Label        irPathLabel;

    // ── Labels ─────────────────────────────────────────────────────────────
    juce::Label labelInputGain  { {}, "INPUT" };
    juce::Label labelMaster     { {}, "MASTER" };
    juce::Label labelBass       { {}, "BASS" };
    juce::Label labelMid        { {}, "MID" };
    juce::Label labelTreble     { {}, "TREBLE" };
    juce::Label labelPresence   { {}, "PRESENCE" };

    // ── Meters ─────────────────────────────────────────────────────────────
    LevelMeter inputMeter;
    LevelMeter outputMeter;

    // ── APVTS attachments ──────────────────────────────────────────────────
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> attInputGain;
    std::unique_ptr<SliderAttachment> attMasterVol;
    std::unique_ptr<SliderAttachment> attBass;
    std::unique_ptr<SliderAttachment> attMid;
    std::unique_ptr<SliderAttachment> attTreble;
    std::unique_ptr<SliderAttachment> attPresence;
    std::unique_ptr<ButtonAttachment> attCabEnabled;

    // ── Layout helpers ─────────────────────────────────────────────────────
    void setupKnob  (juce::Slider& s, juce::Label& l, const juce::String& name);
    void setupLabel (juce::Label& l, const juce::String& text);
    static constexpr int kWidth  = 620;
    static constexpr int kHeight = 240;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarAmpSimEditor)
};
