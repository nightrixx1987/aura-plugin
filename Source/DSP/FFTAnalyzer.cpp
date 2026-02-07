#include "FFTAnalyzer.h"

FFTAnalyzer::FFTAnalyzer()
{
    // Standard-Auflösung: Medium (2048)
    setResolution(FFTResolution::Medium);
}

void FFTAnalyzer::reallocateBuffers()
{
    // FFT-Objekte neu erstellen
    fft = std::make_unique<juce::dsp::FFT>(currentFFTOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        static_cast<size_t>(currentFFTSize),
        juce::dsp::WindowingFunction<float>::hann
    );

    // Vektoren auf neue Größe anpassen
    fifo.resize(static_cast<size_t>(currentFFTSize));
    fftData.resize(static_cast<size_t>(currentFFTSize * 2));
    magnitudes.resize(static_cast<size_t>(currentNumBins));
    smoothedMagnitudes.resize(static_cast<size_t>(currentNumBins));
    frozenMagnitudes.resize(static_cast<size_t>(currentNumBins));

    // Alle auf Null/Floor setzen
    std::fill(fifo.begin(), fifo.end(), 0.0f);
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(magnitudes.begin(), magnitudes.end(), floorDB);
    std::fill(smoothedMagnitudes.begin(), smoothedMagnitudes.end(), floorDB);
    std::fill(frozenMagnitudes.begin(), frozenMagnitudes.end(), floorDB);

    fifoIndex = 0;
    fifoReady = false;
}

void FFTAnalyzer::setResolution(FFTResolution resolution)
{
    if (currentResolution == resolution)
        return;

    const juce::SpinLock::ScopedLockType lock(resolutionLock);
    
    currentResolution = resolution;
    currentFFTOrder = static_cast<int>(resolution);
    currentFFTSize = 1 << currentFFTOrder;
    currentNumBins = currentFFTSize / 2 + 1;

    reallocateBuffers();
}

void FFTAnalyzer::setSpeed(AnalyzerSpeed speed)
{
    currentSpeed = speed;

    // Pro-Q-ähnliche Geschwindigkeits-Presets
    switch (speed)
    {
        case AnalyzerSpeed::VerySlow:
            attackCoeff = 0.7f;
            releaseCoeff = 0.97f;
            break;
        case AnalyzerSpeed::Slow:
            attackCoeff = 0.6f;
            releaseCoeff = 0.93f;
            break;
        case AnalyzerSpeed::Medium:
            attackCoeff = 0.5f;
            releaseCoeff = 0.85f;
            break;
        case AnalyzerSpeed::Fast:
            attackCoeff = 0.3f;
            releaseCoeff = 0.70f;
            break;
        case AnalyzerSpeed::VeryFast:
            attackCoeff = 0.15f;
            releaseCoeff = 0.45f;
            break;
    }
}

void FFTAnalyzer::setCustomSmoothing(float attack, float release)
{
    attackCoeff = juce::jlimit(0.0f, 0.99f, attack);
    releaseCoeff = juce::jlimit(0.0f, 0.99f, release);
}

void FFTAnalyzer::setFrozen(bool freeze)
{
    if (freeze && !frozen.load())
    {
        // Beim Einfrieren: aktuelle Magnitudes speichern
        frozenMagnitudes = smoothedMagnitudes;
    }
    frozen.store(freeze);
}

void FFTAnalyzer::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    reset();
}

void FFTAnalyzer::reset()
{
    // Sichere, dass Buffers initialisiert sind
    if (fifo.empty() || fftData.empty())
        reallocateBuffers();

    std::fill(fifo.begin(), fifo.end(), 0.0f);
    fifoIndex = 0;
    fifoReady = false;
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::fill(magnitudes.begin(), magnitudes.end(), floorDB);
    std::fill(smoothedMagnitudes.begin(), smoothedMagnitudes.end(), floorDB);
    newDataAvailable = false;
}

void FFTAnalyzer::pushSamples(const float* samples, int numSamples)
{
    // Im Freeze-Modus keine neuen Samples verarbeiten
    if (frozen.load())
        return;

    // TryLock: Falls Resolution gerade geaendert wird, Samples verwerfen (RT-safe)
    const juce::SpinLock::ScopedTryLockType lock(resolutionLock);
    if (!lock.isLocked())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        fifo[static_cast<size_t>(fifoIndex)] = samples[i];
        ++fifoIndex;

        if (fifoIndex >= currentFFTSize)
        {
            fifoReady = true;
            fifoIndex = 0;
            processFFT();
        }
    }
}

void FFTAnalyzer::pushBuffer(const juce::AudioBuffer<float>& buffer)
{
    // Im Freeze-Modus keine neuen Samples verarbeiten
    if (frozen.load())
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // Mono-Mix erstellen
    if (numChannels == 1)
    {
        pushSamples(buffer.getReadPointer(0), numSamples);
    }
    else
    {
        // Stereo zu Mono mischen
        const float* left = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);

        for (int i = 0; i < numSamples; ++i)
        {
            float monoSample = (left[i] + right[i]) * 0.5f;

            fifo[static_cast<size_t>(fifoIndex)] = monoSample;
            ++fifoIndex;

            if (fifoIndex >= currentFFTSize)
            {
                fifoReady = true;
                fifoIndex = 0;
                processFFT();
            }
        }
    }
}

void FFTAnalyzer::processFFT()
{
    if (!fifoReady || frozen.load())
        return;

    // FIFO in FFT-Daten kopieren
    std::copy(fifo.begin(), fifo.begin() + currentFFTSize, fftData.begin());

    // Windowing anwenden
    window->multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(currentFFTSize));

    // FFT durchführen (in-place)
    fft->performFrequencyOnlyForwardTransform(fftData.data());

    // Magnitude berechnen und in dB umwandeln mit asymmetrischem Smoothing
    for (int i = 0; i < currentNumBins; ++i)
    {
        float magnitude = fftData[static_cast<size_t>(i)];

        // Normalisieren
        magnitude /= static_cast<float>(currentFFTSize);

        // In dB umwandeln
        float magnitudeDB = juce::Decibels::gainToDecibels(magnitude, floorDB);

        // Asymmetrisches Smoothing: schneller Attack, langsamer Release
        float currentValue = smoothedMagnitudes[static_cast<size_t>(i)];

        if (magnitudeDB > currentValue)
        {
            // Attack: schneller Anstieg
            smoothedMagnitudes[static_cast<size_t>(i)] =
                attackCoeff * currentValue + (1.0f - attackCoeff) * magnitudeDB;
        }
        else
        {
            // Release: langsamer Abfall
            smoothedMagnitudes[static_cast<size_t>(i)] =
                releaseCoeff * currentValue + (1.0f - releaseCoeff) * magnitudeDB;
        }

        magnitudes[static_cast<size_t>(i)] = magnitudeDB;
    }

    fifoReady = false;
    newDataAvailable = true;
}

float FFTAnalyzer::applyTiltCompensation(float frequency, float magnitudeDb) const
{
    if (!tiltEnabled || frequency <= 0.0f)
        return magnitudeDb;

    // Oktaven von der Referenzfrequenz berechnen
    float octaves = std::log2(frequency / tiltCenterFreq);

    // Tilt anwenden: positive Steigung hebt hohe Frequenzen an
    return magnitudeDb + (octaves * tiltSlope);
}

float FFTAnalyzer::getFrequencyForBin(int bin) const
{
    return static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(currentFFTSize);
}

int FFTAnalyzer::getBinForFrequency(float frequency) const
{
    int bin = static_cast<int>(frequency * static_cast<float>(currentFFTSize) / static_cast<float>(sampleRate));
    return juce::jlimit(0, currentNumBins - 1, bin);
}

float FFTAnalyzer::getRawMagnitudeForFrequency(float frequency) const
{
    // KRITISCH: Safety-Check - wenn prepare() nicht aufgerufen wurde
    if (sampleRate <= 0.0 || currentFFTSize <= 0 || currentNumBins <= 0)
        return floorDB;

    // Im Freeze-Modus: eingefrorene Magnitudes verwenden
    const auto& mags = frozen.load() ? frozenMagnitudes : smoothedMagnitudes;
    
    // Pruefen ob Vektoren alloziert sind
    if (mags.empty())
        return floorDB;

    // Lineare Interpolation zwischen benachbarten Bins
    float exactBin = frequency * static_cast<float>(currentFFTSize) / static_cast<float>(sampleRate);
    int lowerBin = static_cast<int>(exactBin);
    int upperBin = lowerBin + 1;

    if (lowerBin < 0)
        return mags[0];
    if (upperBin >= currentNumBins)
        return mags[static_cast<size_t>(currentNumBins - 1)];

    float fraction = exactBin - static_cast<float>(lowerBin);

    return mags[static_cast<size_t>(lowerBin)] * (1.0f - fraction) +
           mags[static_cast<size_t>(upperBin)] * fraction;
}

float FFTAnalyzer::getMagnitudeForFrequency(float frequency) const
{
    float rawMagnitude = getRawMagnitudeForFrequency(frequency);

    // Tilt-Kompensation anwenden
    return applyTiltCompensation(frequency, rawMagnitude);
}
