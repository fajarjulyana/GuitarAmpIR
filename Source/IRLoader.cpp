#include "IRLoader.h"

IRLoader::IRLoader()
{
    formatManager.registerBasicFormats();   // WAV, AIFF, FLAC, OGG
}

void IRLoader::loadFromFile (const juce::File& file)
{
    if (!file.existsAsFile())
    {
        if (onLoadError)
            onLoadError ("File not found: " + file.getFullPathName());
        return;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));

    if (!reader)
    {
        if (onLoadError)
            onLoadError ("Unsupported format or corrupt file: " + file.getFileName());
        return;
    }

    // Sanity-check: refuse files longer than 10 s to avoid OOM
    const double maxSeconds = 10.0;
    auto         totalSamples = static_cast<int> (reader->lengthInSamples);
    int          maxSamples   = static_cast<int> (reader->sampleRate * maxSeconds);

    if (totalSamples > maxSamples)
    {
        if (onLoadError)
            onLoadError ("IR file is too long (max 10 s). Only the first 10 s will be used.");
        totalSamples = maxSamples;
    }

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), totalSamples);
    reader->read (&buffer, 0, totalSamples, 0, true, true);

    lastPath = file.getFullPathName();

    if (onIRLoaded)
        onIRLoaded (buffer, reader->sampleRate);
}

void IRLoader::browseAndLoad (juce::Component* parentComponent)
{
    juce::ignoreUnused (parentComponent); // Menghilangkan warning C4100

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    auto chooser = std::make_shared<juce::FileChooser> (
        "Load Impulse Response",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac"
    );

    chooser->launchAsync (chooserFlags, [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file.existsAsFile())
        {
            loadFromFile (file);
        }
    });
}
