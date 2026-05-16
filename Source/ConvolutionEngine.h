#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <complex>
#include <atomic>

/**
 * ConvolutionEngine
 * -----------------
 * FFT-based uniformly-partitioned overlap-add convolution.
 *
 * The IR is split into equal-length blocks of size `blockSize`.  For each
 * incoming audio block we compute one FFT, multiply against all stored IR
 * spectra, accumulate in the frequency domain, then IFFT the result and add
 * the overlap tail — classic Overlap-Add (OLA).
 *
 * This gives O(N · log N) complexity per block while keeping latency equal
 * to exactly one block (zero additional look-ahead).
 */
class ConvolutionEngine
{
public:
    ConvolutionEngine();
    ~ConvolutionEngine() = default;

    /** Load an impulse response from a stereo/mono AudioBuffer.
     *  May be called from any thread; internally double-buffered. */
    void loadIR (const juce::AudioBuffer<float>& irBuffer, double irSampleRate);

    /** Clear the loaded IR (passthrough mode). */
    void clearIR();

    /** Must be called before first process(). */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /** Reset all delay-line state (call on transport restart). */
    void reset();

    /** Process a mono block in-place.
     *  @param data   pointer to audio samples
     *  @param numSamples  number of samples to process */
    void process (float* data, int numSamples);

    bool hasIR() const noexcept { return irLoaded.load(); }

private:
    // ── Configuration ─────────────────────────────────────────────────────
    static constexpr int kMinFFTOrder = 9;   // 512 samples minimum
    static constexpr int kMaxIRSeconds = 4;  // cap IR length for memory

    // ── FFT machinery ─────────────────────────────────────────────────────
    std::unique_ptr<juce::dsp::FFT> fft;
    int fftOrder  = kMinFFTOrder;
    int fftSize   = 1 << kMinFFTOrder;   // 2^order
    int blockSize = 1 << (kMinFFTOrder - 1);

    // ── IR storage (frequency domain, one spectrum per partition) ─────────
    struct IRPartition
    {
        std::vector<std::complex<float>> spectrum; // fftSize complex bins
    };
    std::vector<IRPartition> irPartitions;
    std::atomic<bool> irLoaded { false };

    // Double-buffer swap: pending IR written here, swapped in on next block
    struct PendingIR
    {
        std::vector<IRPartition> partitions;
        bool ready = false;
    };
    PendingIR pendingIR;
    juce::SpinLock irSwapLock;

    // ── Overlap-add state ─────────────────────────────────────────────────
    std::vector<float>               inputFifo;     // circular input buffer
    int                              inputFifoPos = 0;

    // Frequency-domain delay line (one spectrum per partition step)
    std::vector<std::vector<std::complex<float>>> fdlSpectra;
    int                                          fdlHead = 0;

    std::vector<float> overlapTail;   // OLA tail from previous block
    std::vector<float> workBuffer;    // scratch space for FFT I/O

    double sampleRate = 44100.0;
    int    maxBlockSize = 512;

    // ── Private helpers ────────────────────────────────────────────────────
    void rebuildFromIR   (const juce::AudioBuffer<float>& ir, double irSR);
    void swapPendingIR   ();
    void processOneBlock (const float* input, float* output);

    /** Forward FFT: interleaved real→complex stored in `out` (size fftSize). */
    void forwardFFT  (const float* real, std::complex<float>* out);
    /** Inverse FFT: complex→real, result in `out` (size fftSize). */
    void inverseFFT  (const std::complex<float>* in, float* out);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionEngine)
};
