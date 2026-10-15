#include "ConvolutionEngine.h"
#include <algorithm>
#include <cstring>
#include <cmath>

ConvolutionEngine::ConvolutionEngine() = default;

void ConvolutionEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate   = spec.sampleRate;
    maxBlockSize = static_cast<int> (spec.maximumBlockSize);

    fftOrder  = kMinFFTOrder;
    blockSize = 1 << (fftOrder - 1);
    while (blockSize < maxBlockSize)
    {
        ++fftOrder;
        blockSize = 1 << (fftOrder - 1);
    }
    fftSize = 1 << fftOrder;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    inputFifo.assign   (static_cast<size_t> (fftSize), 0.0f);
    overlapTail.assign (static_cast<size_t> (fftSize), 0.0f);
    workBuffer.assign  (static_cast<size_t> (fftSize * 2), 0.0f);

    inputFifoPos = 0;
    fdlHead      = 0;

    reset();
}

void ConvolutionEngine::reset()
{
    std::fill (inputFifo.begin(),   inputFifo.end(),   0.0f);
    std::fill (overlapTail.begin(), overlapTail.end(), 0.0f);
    std::fill (workBuffer.begin(),  workBuffer.end(),  0.0f);
    inputFifoPos = 0;
    fdlHead      = 0;
}

void ConvolutionEngine::clearIR()
{
    juce::SpinLock::ScopedLockType lock (irSwapLock);
    irPartitions.clear();
    fdlSpectra.clear();
    irLoaded.store (false);
}

void ConvolutionEngine::loadIR (const juce::AudioBuffer<float>& irBuffer, double irSampleRate)
{
    auto newPartitions = rebuildFromIRLocal (irBuffer, irSampleRate);

    {
        juce::SpinLock::ScopedLockType lock (irSwapLock);
        pendingIR.partitions = std::move (newPartitions);
        pendingIR.ready      = true;
    }
}

void ConvolutionEngine::process (float* data, int numSamples)
{
    if (!irLoaded.load())
        return;

    swapPendingIR();

    int remaining = numSamples;
    int offset    = 0;

    while (remaining > 0)
    {
        int toCopy = std::min (remaining, blockSize - inputFifoPos);
        std::memcpy (inputFifo.data() + inputFifoPos, data + offset,
                     static_cast<size_t> (toCopy) * sizeof (float));
        inputFifoPos += toCopy;
        offset       += toCopy;
        remaining    -= toCopy;

        if (inputFifoPos == blockSize)
        {
            processOneBlock (inputFifo.data(), data + (offset - blockSize));
            inputFifoPos = 0;
        }
    }
}

std::vector<ConvolutionEngine::IRPartition> ConvolutionEngine::rebuildFromIRLocal (const juce::AudioBuffer<float>& irBuf, double irSR)
{
    int irLen = irBuf.getNumSamples();
    int maxSamples = static_cast<int> (irSR * kMaxIRSeconds);
    irLen = std::min (irLen, maxSamples);

    std::vector<float> mono (static_cast<size_t> (irLen), 0.0f);
    for (int ch = 0; ch < irBuf.getNumChannels(); ++ch)
    {
        const float* src = irBuf.getReadPointer (ch);
        for (int i = 0; i < irLen; ++i)
            mono[static_cast<size_t> (i)] += src[i];
    }
    float scale = 1.0f / static_cast<float> (irBuf.getNumChannels());
    for (auto& s : mono)
        s *= scale;

    if (std::abs (irSR - sampleRate) > 1.0)
    {
        double ratio = sampleRate / irSR;
        int newLen   = static_cast<int> (std::round (irLen * ratio));
        std::vector<float> resampled (static_cast<size_t> (newLen), 0.0f);
        for (int i = 0; i < newLen; ++i)
        {
            double srcPos = i / ratio;
            int    srcIdx = static_cast<int> (srcPos);
            float  frac   = static_cast<float> (srcPos - srcIdx);
            float  a      = mono[static_cast<size_t> (std::min (srcIdx,     irLen - 1))];
            float  b      = mono[static_cast<size_t> (std::min (srcIdx + 1, irLen - 1))];
            resampled[static_cast<size_t> (i)] = a + frac * (b - a);
        }
        mono  = std::move (resampled);
        irLen = newLen;
    }

    int numPartitions = (irLen + blockSize - 1) / blockSize;
    std::vector<IRPartition> localPartitions (static_cast<size_t> (numPartitions));
    std::vector<float> partBuf (static_cast<size_t> (fftSize), 0.0f);

    for (int p = 0; p < numPartitions; ++p)
    {
        std::fill (partBuf.begin(), partBuf.end(), 0.0f);
        int start   = p * blockSize;
        int copyLen = std::min (blockSize, irLen - start);
        std::memcpy (partBuf.data(), mono.data() + start,
                     static_cast<size_t> (copyLen) * sizeof (float));

        localPartitions[static_cast<size_t> (p)].spectrum.resize (static_cast<size_t> (fftSize));
        forwardFFT (partBuf.data(), localPartitions[static_cast<size_t> (p)].spectrum.data());
    }

    return localPartitions;
}

void ConvolutionEngine::swapPendingIR()
{
    juce::SpinLock::ScopedTryLockType lock (irSwapLock);
    if (!lock.isLocked())   return;
    if (!pendingIR.ready)   return;

    irPartitions = std::move (pendingIR.partitions);
    int numPart  = static_cast<int> (irPartitions.size());
    fdlSpectra.assign (static_cast<size_t> (numPart),
                       std::vector<std::complex<float>> (static_cast<size_t> (fftSize),
                                                         { 0.0f, 0.0f }));
    fdlHead         = 0;
    pendingIR.ready = false;
    irLoaded.store  (true);
}

void ConvolutionEngine::processOneBlock (const float* input, float* output)
{
    int numPartitions = static_cast<int> (irPartitions.size());
    if (numPartitions == 0)
    {
        std::memcpy (output, input, static_cast<size_t> (blockSize) * sizeof (float));
        return;
    }

    std::fill (workBuffer.begin(), workBuffer.end(), 0.0f);
    std::memcpy (workBuffer.data(), input, static_cast<size_t> (blockSize) * sizeof (float));

    std::vector<std::complex<float>> inputSpec (static_cast<size_t> (fftSize));
    forwardFFT (workBuffer.data(), inputSpec.data());

    fdlSpectra[static_cast<size_t> (fdlHead)] = inputSpec;

    std::vector<std::complex<float>> outputSpec (static_cast<size_t> (fftSize), { 0.0f, 0.0f });
    for (int k = 0; k < numPartitions; ++k)
    {
        int fdlIdx = (fdlHead - k + numPartitions) % numPartitions;
        const auto& Hk  = irPartitions[static_cast<size_t> (k)].spectrum;
        const auto& Xnk = fdlSpectra[static_cast<size_t> (fdlIdx)];
        for (int bin = 0; bin < fftSize; ++bin)
            outputSpec[static_cast<size_t> (bin)] += Hk[static_cast<size_t> (bin)]
                                                   * Xnk[static_cast<size_t> (bin)];
    }

    fdlHead = (fdlHead + 1) % numPartitions;

    std::fill (workBuffer.begin(), workBuffer.end(), 0.0f);
    inverseFFT (outputSpec.data(), workBuffer.data());

    for (int i = 0; i < blockSize; ++i)
        output[i] = workBuffer[static_cast<size_t> (i)]
                  + overlapTail[static_cast<size_t> (i)];

    std::memcpy (overlapTail.data(),
                 workBuffer.data() + blockSize,
                 static_cast<size_t> (blockSize) * sizeof (float));
}

void ConvolutionEngine::forwardFFT (const float* real, std::complex<float>* out)
{
    if (!fft || fftSize <= 0)
        return;

    // Alokasi buffer FFT 2x fftSize
    std::vector<float> buf (static_cast<size_t> (fftSize * 2), 0.0f);
    
    for (int i = 0; i < fftSize; ++i)
        buf[static_cast<size_t> (i)] = real[i];

    fft->performRealOnlyForwardTransform (buf.data(), true);

    for (int i = 0; i < fftSize; ++i)
        out[static_cast<size_t> (i)] = { buf[static_cast<size_t> (i * 2)],
                                         buf[static_cast<size_t> (i * 2 + 1)] };
}

void ConvolutionEngine::inverseFFT (const std::complex<float>* in, float* out)
{
    if (!fft || fftSize <= 0)
        return;

    std::vector<float> buf (static_cast<size_t> (fftSize * 2), 0.0f);
    for (int i = 0; i < fftSize; ++i)
    {
        buf[static_cast<size_t> (i * 2)]     = in[static_cast<size_t> (i)].real();
        buf[static_cast<size_t> (i * 2 + 1)] = in[static_cast<size_t> (i)].imag();
    }

    fft->performRealOnlyInverseTransform (buf.data());

    float norm = 1.0f / static_cast<float> (fftSize);
    for (int i = 0; i < fftSize; ++i)
        out[i] = buf[static_cast<size_t> (i)] * norm;
}