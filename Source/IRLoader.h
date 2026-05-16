#pragma once
#include <juce_gui_basics/juce_gui_basics.h> // <--- Pastikan ini ada
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <functional>

/**
 * IRLoader
 * --------
 * Thin wrapper around JUCE's AudioFormatManager that reads a .WAV file
 * (or any format JUCE supports) into an AudioBuffer<float>.
 *
 * Usage:
 *   IRLoader loader;
 *   loader.onIRLoaded = [this](auto& buf, double sr) { engine.loadIR(buf, sr); };
 *   loader.loadFromFile(file);
 */
class IRLoader
{
public:
    IRLoader();

    /** Callback fired on the calling thread after a successful load.
     *  @param buffer  The decoded audio data (mono or stereo)
     *  @param sampleRate  The IR file's native sample rate */
    std::function<void(const juce::AudioBuffer<float>&, double sampleRate)> onIRLoaded;

    /** Callback fired when loading fails. */
    std::function<void(const juce::String& reason)> onLoadError;

    /** Synchronously load a WAV/AIFF/FLAC file into memory.
     *  Fires onIRLoaded or onLoadError on the calling thread. */
    void loadFromFile (const juce::File& file);

    /** Open a native file-chooser dialog and load the selected file.
     *  Must be called on the message thread. */
    void browseAndLoad (juce::Component* parentComponent);

    /** Returns the path of the last successfully loaded file. */
    juce::String getLastLoadedPath() const { return lastPath; }

private:
    juce::AudioFormatManager formatManager;
    juce::String             lastPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRLoader)
};
