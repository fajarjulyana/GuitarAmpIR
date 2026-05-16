#include "PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Colour palette
// ─────────────────────────────────────────────────────────────────────────────
namespace Palette
{
    const juce::Colour background  { 0xFF1A1A1A };
    const juce::Colour panel       { 0xFF252525 };
    const juce::Colour accent      { 0xFFD4500A };   // burnt orange (LePou-ish)
    const juce::Colour knobBody    { 0xFF333333 };
    const juce::Colour knobPointer { 0xFFE8E8E8 };
    const juce::Colour labelText   { 0xFFAAAAAA };
    const juce::Colour meterGreen  { 0xFF44BB44 };
    const juce::Colour meterYellow { 0xFFCCCC33 };
    const juce::Colour meterRed    { 0xFFCC2222 };
    const juce::Colour buttonBg    { 0xFF2E2E2E };
    const juce::Colour buttonText  { 0xFFCCCCCC };
}

// ─────────────────────────────────────────────────────────────────────────────
//  LevelMeter
// ─────────────────────────────────────────────────────────────────────────────

LevelMeter::LevelMeter()
{
    startTimerHz (30);
}

void LevelMeter::setLevel (float v)
{
    displayLevel = v;
    if (v > holdLevel)
    {
        holdLevel   = v;
        holdCounter = 45;   // hold for ~1.5 s at 30 Hz
    }
}

void LevelMeter::timerCallback()
{
    if (holdCounter > 0)
        --holdCounter;
    else
        holdLevel = std::max (0.0f, holdLevel - 0.01f);

    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.fillAll (juce::Colour (0xFF111111));

    float dbValue  = juce::Decibels::gainToDecibels (displayLevel, -60.0f);
    float dbNorm   = juce::jlimit (0.0f, 1.0f, (dbValue + 60.0f) / 60.0f);
    float fillH    = bounds.getHeight() * dbNorm;

    // Gradient fill
    juce::ColourGradient grad (Palette::meterRed,    bounds.getX(), bounds.getY(),
                               Palette::meterGreen,  bounds.getX(), bounds.getBottom(),
                               false);
    grad.addColour (0.33, Palette::meterYellow);
    g.setGradientFill (grad);
    g.fillRect (bounds.withTop (bounds.getBottom() - fillH));

    // Hold line
    float holdNorm = juce::jlimit (0.0f, 1.0f,
        (juce::Decibels::gainToDecibels (holdLevel, -60.0f) + 60.0f) / 60.0f);
    float holdY = bounds.getBottom() - bounds.getHeight() * holdNorm;
    g.setColour (Palette::knobPointer);
    g.drawHorizontalLine (static_cast<int> (holdY), bounds.getX(), bounds.getRight());
}

// ─────────────────────────────────────────────────────────────────────────────
//  DarkLookAndFeel
// ─────────────────────────────────────────────────────────────────────────────

DarkLookAndFeel::DarkLookAndFeel()
{
    setColour (juce::Slider::thumbColourId,            Palette::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, Palette::accent);
    setColour (juce::Slider::rotarySliderFillColourId, Palette::knobBody);
    setColour (juce::ToggleButton::tickColourId,        Palette::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::labelText);
    setColour (juce::TextButton::buttonColourId,        Palette::buttonBg);
    setColour (juce::TextButton::textColourOffId,       Palette::buttonText);
    setColour (juce::Label::textColourId,               Palette::labelText);
}

void DarkLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPos,
                                        float startAngle, float endAngle,
                                        juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                          static_cast<float> (y),
                                          static_cast<float> (width),
                                          static_cast<float> (height)).reduced (6.0f);
    float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    float cx = bounds.getCentreX();
    float cy = bounds.getCentreY();

    // Shadow
    g.setColour (juce::Colour (0x55000000));
    g.fillEllipse (cx - radius + 2, cy - radius + 3, radius * 2, radius * 2);

    // Body
    g.setColour (Palette::knobBody);
    g.fillEllipse (cx - radius, cy - radius, radius * 2, radius * 2);

    // Rim
    g.setColour (Palette::accent.withAlpha (0.6f));
    g.drawEllipse (cx - radius, cy - radius, radius * 2, radius * 2, 1.5f);

    // Arc track
    juce::Path arc;
    arc.addArc (cx - radius + 4, cy - radius + 4,
                (radius - 4) * 2, (radius - 4) * 2,
                startAngle, endAngle, true);
    g.setColour (Palette::panel);
    g.strokePath (arc, juce::PathStrokeType (2.0f));

    // Value arc
    juce::Path valueArc;
    valueArc.addArc (cx - radius + 4, cy - radius + 4,
                     (radius - 4) * 2, (radius - 4) * 2,
                     startAngle, startAngle + (endAngle - startAngle) * sliderPos, true);
    g.setColour (Palette::accent);
    g.strokePath (valueArc, juce::PathStrokeType (2.0f));

    // Pointer line
    float angle    = startAngle + sliderPos * (endAngle - startAngle) - juce::MathConstants<float>::halfPi;
    float innerR   = radius * 0.35f;
    float outerR   = radius * 0.82f;
    juce::Point<float> p1 { cx + innerR * std::cos (angle), cy + innerR * std::sin (angle) };
    juce::Point<float> p2 { cx + outerR * std::cos (angle), cy + outerR * std::sin (angle) };
    g.setColour (Palette::knobPointer);
    g.drawLine ({ p1, p2 }, 2.0f);
}

void DarkLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& btn,
                                        bool highlighted, bool /*down*/)
{
    bool on = btn.getToggleState();
    auto b  = btn.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (on ? Palette::accent : Palette::buttonBg);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (on ? juce::Colours::white : Palette::labelText);
    if (highlighted) g.setColour (btn.findColour (juce::ToggleButton::textColourId).brighter (0.2f));
    g.setFont (juce::Font (11.0f, juce::Font::bold));
    g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font DarkLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (10.5f, juce::Font::plain);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GuitarAmpSimEditor
// ─────────────────────────────────────────────────────────────────────────────

GuitarAmpSimEditor::GuitarAmpSimEditor (GuitarAmpSimProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&darkLaf);
    setSize (kWidth, kHeight);
    setResizable (false, false);

    // ── Knobs ──────────────────────────────────────────────────────────────
    setupKnob (inputGainKnob, labelInputGain, "INPUT");
    setupKnob (masterVolKnob, labelMaster,    "MASTER");
    setupKnob (bassKnob,      labelBass,      "BASS");
    setupKnob (midKnob,       labelMid,       "MID");
    setupKnob (trebleKnob,    labelTreble,    "TREBLE");
    setupKnob (presenceKnob,  labelPresence,  "PRESENCE");

    // ── APVTS attachments ──────────────────────────────────────────────────
    attInputGain = std::make_unique<SliderAttachment> (p.apvts, ParamID::InputGain,  inputGainKnob);
    attMasterVol = std::make_unique<SliderAttachment> (p.apvts, ParamID::MasterVol,  masterVolKnob);
    attBass      = std::make_unique<SliderAttachment> (p.apvts, ParamID::Bass,       bassKnob);
    attMid       = std::make_unique<SliderAttachment> (p.apvts, ParamID::Mid,        midKnob);
    attTreble    = std::make_unique<SliderAttachment> (p.apvts, ParamID::Treble,     trebleKnob);
    attPresence  = std::make_unique<SliderAttachment> (p.apvts, ParamID::Presence,   presenceKnob);
    attCabEnabled= std::make_unique<ButtonAttachment> (p.apvts, ParamID::CabEnabled, cabToggle);

    // ── Cabinet toggle ─────────────────────────────────────────────────────
    addAndMakeVisible (cabToggle);

    // ── IR load button ─────────────────────────────────────────────────────
    loadIRBtn.setColour (juce::TextButton::buttonColourId,   Palette::buttonBg);
    loadIRBtn.setColour (juce::TextButton::textColourOffId,  Palette::buttonText);
    loadIRBtn.onClick = [this]
    {
        processor.browseForIR (this);
        irPathLabel.setText (juce::File (processor.getIRPath()).getFileName(),
                             juce::dontSendNotification);
    };
    addAndMakeVisible (loadIRBtn);

    // ── IR path label ──────────────────────────────────────────────────────
    irPathLabel.setColour (juce::Label::textColourId, Palette::labelText);
    irPathLabel.setFont   (juce::Font (10.0f));
    irPathLabel.setJustificationType (juce::Justification::centredLeft);
    irPathLabel.setText ("No IR loaded", juce::dontSendNotification);
    addAndMakeVisible (irPathLabel);

    // ── Meters ─────────────────────────────────────────────────────────────
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    startTimerHz (30);
}

GuitarAmpSimEditor::~GuitarAmpSimEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

void GuitarAmpSimEditor::timerCallback()
{
    inputMeter .setLevel (processor.inputLevel .load());
    outputMeter.setLevel (processor.outputLevel.load());

    // Update IR label if it changed externally
    auto path = processor.getIRPath();
    if (path.isNotEmpty())
    {
        auto filename = juce::File (path).getFileName();
        if (irPathLabel.getText() != filename)
            irPathLabel.setText (filename, juce::dontSendNotification);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Left: meters  (12 px wide each)
    auto meterArea = area.removeFromLeft (14);
    inputMeter .setBounds (meterArea.removeFromLeft  (6).reduced (0, 4));
    outputMeter.setBounds (meterArea.reduced (0, 4));

    area.removeFromLeft (6);   // gap

    // Bottom strip: CAB toggle + LOAD IR + path label
    auto bottomStrip = area.removeFromBottom (32);
    cabToggle  .setBounds (bottomStrip.removeFromLeft (46).reduced (2));
    loadIRBtn  .setBounds (bottomStrip.removeFromLeft (72).reduced (2));
    bottomStrip.removeFromLeft (6);
    irPathLabel.setBounds (bottomStrip);

    // Knobs — evenly spaced across remaining area
    const int numKnobs = 6;
    int knobW = area.getWidth() / numKnobs;

    auto placeKnob = [&] (juce::Slider& k, juce::Label& lbl)
    {
        auto col = area.removeFromLeft (knobW);
        lbl.setBounds (col.removeFromBottom (16));
        k  .setBounds (col.reduced (4));
    };

    placeKnob (inputGainKnob, labelInputGain);
    placeKnob (bassKnob,      labelBass);
    placeKnob (midKnob,       labelMid);
    placeKnob (trebleKnob,    labelTreble);
    placeKnob (presenceKnob,  labelPresence);
    placeKnob (masterVolKnob, labelMaster);
}

void GuitarAmpSimEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Title bar
    auto titleArea = juce::Rectangle<int> (0, 0, getWidth(), 22);
    g.setColour (Palette::panel);
    g.fillRect (titleArea);

    g.setColour (Palette::accent);
    g.fillRect (0, 0, 4, 22);   // left accent stripe

    g.setColour (Palette::knobPointer);
    g.setFont   (juce::Font (12.0f, juce::Font::bold));
    g.drawText  ("GuitarAmpIR", titleArea.withLeft (10),
                 juce::Justification::centredLeft);

    g.setColour (Palette::labelText);
    g.setFont   (juce::Font (10.0f));
    g.drawText  ("convolution cabinet · triode preamp",
                 titleArea.withRight (getWidth() - 8),
                 juce::Justification::centredRight);

    // Separator
    g.setColour (Palette::panel);
    g.fillRect  (0, 22, getWidth(), 2);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Knob setup helper
// ─────────────────────────────────────────────────────────────────────────────

void GuitarAmpSimEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                     const juce::String& name)
{
    s.setSliderStyle        (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle       (juce::Slider::NoTextBox, false, 0, 0);
    s.setDoubleClickReturnValue (true, 0.5);
    addAndMakeVisible (s);

    l.setColour (juce::Label::textColourId,  Palette::labelText);
    l.setFont   (juce::Font (10.0f, juce::Font::plain));
    l.setJustificationType (juce::Justification::centred);
    l.setText   (name, juce::dontSendNotification);
    addAndMakeVisible (l);
}
