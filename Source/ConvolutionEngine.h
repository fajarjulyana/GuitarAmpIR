#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <complex>
#include <atomic>

class ConvolutionEngine
{
public:
    ConvolutionEngine();
    ~ConvolutionEngine() = default;

    // Struct IRPartition dijadikan public atau ditempatkan sebelum method private yang menggunakannya
    struct IRPartition
    {
        std::vector<std::complex<float>> spectrum;
    };

    void loadIR (const juce::AudioBuffer<float>& irBuffer, double irSampleRate);
    void clearIR();
    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (float* data, int numSamples);

    bool hasIR() const noexcept { return irLoaded.load(); }

private:
    static constexpr int kMinFFTOrder = 9;   // 512 samples minimum
    static constexpr int kMaxIRSeconds = 4;  // cap IR length for memory

    std::unique_ptr<juce::dsp::FFT> fft;
    int fftOrder  = kMinFFTOrder;
    int fftSize   = 1 << kMinFFTOrder;
    int blockSize = 1 << (kMinFFTOrder - 1);

    std::vector<IRPartition> irPartitions;
    std::atomic<bool> irLoaded { false };

    struct PendingIR
    {
        std::vector<IRPartition> partitions;
        bool ready = false;
    };
    PendingIR pendingIR;
    juce::SpinLock irSwapLock;

    std::vector<float> inputFifo;
    int                inputFifoPos = 0;

    std::vector<std::vector<std::complex<float>>> fdlSpectra;
    int                                           fdlHead = 0;

    std::vector<float> overlapTail;
    std::vector<float> workBuffer;

    double sampleRate = 44100.0;
    int    maxBlockSize = 512;

    // Deklarasi fungsi helper baru
    std::vector<IRPartition> rebuildFromIRLocal (const juce::AudioBuffer<float>& ir, double irSR);
    void swapPendingIR   ();
    void processOneBlock (const float* input, float* output);

    void forwardFFT  (const float* real, std::complex<float>* out);
    void inverseFFT  (const std::complex<float>* in, float* out);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionEngine)
};