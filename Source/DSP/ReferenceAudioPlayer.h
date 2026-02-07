#pragma once

#include <JuceHeader.h>
#include "FFTAnalyzer.h"

/**
 * ReferenceAudioPlayer: Lädt und analysiert Reference-Audio für Spektralvergleich.
 * 
 * Features:
 * - Unterstützt WAV, AIFF, FLAC, MP3
 * - Automatisches Resampling auf Plugin-Samplerate
 * - Spektralanalyse des Reference-Tracks
 * - A/B-Vergleich mit aktuellem Signal
 * - Waveform-Thumbnail für UI
 */
class ReferenceAudioPlayer : public juce::ChangeListener
{
public:
    //==========================================================================
    // Konstruktor / Destruktor
    //==========================================================================
    ReferenceAudioPlayer()
        : thumbnailCache(5),
          thumbnail(512, formatManager, thumbnailCache)
    {
        // Standard-Formate registrieren
        formatManager.registerBasicFormats();
        thumbnail.addChangeListener(this);
    }
    
    ~ReferenceAudioPlayer() override
    {
        thumbnail.removeChangeListener(this);
    }
    
    //==========================================================================
    // Audio-Datei laden
    //==========================================================================
    bool loadFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;
        
        // Reader erstellen
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr)
            return false;
        
        // Metadaten speichern
        currentFile = file;
        originalSampleRate = reader->sampleRate;
        originalNumChannels = static_cast<int>(reader->numChannels);
        originalLengthInSamples = reader->lengthInSamples;
        durationSeconds = static_cast<float>(reader->lengthInSamples) / static_cast<float>(reader->sampleRate);
        
        // Audio in Buffer laden
        referenceBuffer.setSize(static_cast<int>(reader->numChannels), 
                                static_cast<int>(reader->lengthInSamples));
        reader->read(&referenceBuffer, 0, 
                     static_cast<int>(reader->lengthInSamples), 0, true, true);
        
        // Thumbnail laden
        thumbnail.setSource(new juce::FileInputSource(file));
        
        // Resampling falls nötig
        if (currentSampleRate > 0 && std::abs(reader->sampleRate - currentSampleRate) > 1.0)
        {
            resampleBuffer();
        }
        
        loaded = true;
        
        // Spektrum analysieren
        analyzeSpectrum();
        
        // Listener benachrichtigen
        if (onFileLoaded)
            onFileLoaded(file);
        
        return true;
    }
    
    void unloadFile()
    {
        referenceBuffer.setSize(0, 0);
        resampledBuffer.setSize(0, 0);
        spectrumMagnitudes.clear();
        currentFile = juce::File();
        loaded = false;
        thumbnail.clear();
        
        if (onFileUnloaded)
            onFileUnloaded();
    }
    
    //==========================================================================
    // Samplerate setzen (für Resampling)
    //==========================================================================
    void prepare(double sampleRate, int blockSize)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = blockSize;
        
        // FFT für Spektralanalyse vorbereiten
        fftAnalyzer.prepare(sampleRate);
        
        // Wenn bereits geladen, neu resamplen
        if (loaded && std::abs(originalSampleRate - currentSampleRate) > 1.0)
        {
            resampleBuffer();
            analyzeSpectrum();
        }
    }
    
    //==========================================================================
    // Getters
    //==========================================================================
    bool isLoaded() const { return loaded; }
    const juce::File& getCurrentFile() const { return currentFile; }
    juce::String getFileName() const { return currentFile.getFileName(); }
    float getDurationSeconds() const { return durationSeconds; }
    int getNumChannels() const { return originalNumChannels; }
    double getOriginalSampleRate() const { return originalSampleRate; }
    
    // Buffer für Playback/Analyse
    const juce::AudioBuffer<float>& getBuffer() const
    {
        if (resampledBuffer.getNumSamples() > 0)
            return resampledBuffer;
        return referenceBuffer;
    }
    
    // Spektrum-Daten für Overlay
    const std::vector<float>& getSpectrumMagnitudes() const { return spectrumMagnitudes; }
    
    // Thumbnail für Waveform-Anzeige
    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    
    //==========================================================================
    // Playback (optional - für Vorhören)
    //==========================================================================
    void setPlaybackPosition(float normalizedPosition)
    {
        playbackPosition = juce::jlimit(0.0f, 1.0f, normalizedPosition);
        playbackSampleIndex = static_cast<juce::int64>(playbackPosition * static_cast<float>(getBuffer().getNumSamples()));
    }
    
    float getPlaybackPosition() const { return playbackPosition; }
    
    void setPlaying(bool shouldPlay) { playing = shouldPlay; }
    bool isPlaying() const { return playing; }
    
    // Holt nächsten Block für Playback (für Mix mit Eingangssignal)
    void getNextBlock(juce::AudioBuffer<float>& outputBuffer)
    {
        if (!loaded || !playing)
            return;
        
        const auto& sourceBuffer = getBuffer();
        int numSamples = outputBuffer.getNumSamples();
        int numChannels = std::min(outputBuffer.getNumChannels(), sourceBuffer.getNumChannels());
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                juce::int64 sampleIdx = playbackSampleIndex + i;
                if (sampleIdx < sourceBuffer.getNumSamples())
                {
                    outputBuffer.addSample(ch, i, 
                        sourceBuffer.getSample(ch, static_cast<int>(sampleIdx)) * playbackGain);
                }
            }
        }
        
        playbackSampleIndex += numSamples;
        
        // Loop oder Stop am Ende
        if (playbackSampleIndex >= sourceBuffer.getNumSamples())
        {
            if (looping)
                playbackSampleIndex = 0;
            else
                playing = false;
        }
        
        playbackPosition = static_cast<float>(playbackSampleIndex) / static_cast<float>(sourceBuffer.getNumSamples());
    }
    
    void setPlaybackGain(float gain) { playbackGain = juce::jlimit(0.0f, 2.0f, gain); }
    float getPlaybackGain() const { return playbackGain; }
    
    void setLooping(bool shouldLoop) { looping = shouldLoop; }
    bool isLooping() const { return looping; }
    
    //==========================================================================
    // Callbacks
    //==========================================================================
    std::function<void(const juce::File&)> onFileLoaded;
    std::function<void()> onFileUnloaded;
    std::function<void()> onThumbnailChanged;
    
private:
    //==========================================================================
    // ChangeListener für Thumbnail
    //==========================================================================
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        if (onThumbnailChanged)
            onThumbnailChanged();
    }
    
    //==========================================================================
    // Resampling
    //==========================================================================
    void resampleBuffer()
    {
        if (referenceBuffer.getNumSamples() == 0 || currentSampleRate <= 0)
            return;
        
        double ratio = currentSampleRate / originalSampleRate;
        int newNumSamples = static_cast<int>(static_cast<double>(referenceBuffer.getNumSamples()) * ratio);
        
        resampledBuffer.setSize(referenceBuffer.getNumChannels(), newNumSamples);
        
        // Einfaches lineares Resampling (könnte durch LagrangeInterpolator verbessert werden)
        for (int ch = 0; ch < referenceBuffer.getNumChannels(); ++ch)
        {
            const float* input = referenceBuffer.getReadPointer(ch);
            float* output = resampledBuffer.getWritePointer(ch);
            
            for (int i = 0; i < newNumSamples; ++i)
            {
                double srcPos = static_cast<double>(i) / ratio;
                int srcIdx = static_cast<int>(srcPos);
                float frac = static_cast<float>(srcPos - srcIdx);
                
                if (srcIdx + 1 < referenceBuffer.getNumSamples())
                {
                    output[i] = input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac;
                }
                else if (srcIdx < referenceBuffer.getNumSamples())
                {
                    output[i] = input[srcIdx];
                }
            }
        }
    }
    
    //==========================================================================
    // Spektralanalyse
    //==========================================================================
    void analyzeSpectrum()
    {
        if (!loaded)
            return;
        
        const auto& buffer = getBuffer();
        if (buffer.getNumSamples() == 0)
            return;
        
        // Analysiere mehrere Abschnitte und mittele das Spektrum
        const int numSections = 8;
        const int fftSize = 4096;
        const int numBins = fftSize / 2 + 1;
        
        spectrumMagnitudes.clear();
        spectrumMagnitudes.resize(static_cast<size_t>(numBins), 0.0f);
        
        std::vector<float> windowedSamples(static_cast<size_t>(fftSize), 0.0f);
        std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);
        
        juce::dsp::FFT fft(static_cast<int>(std::log2(fftSize)));
        juce::dsp::WindowingFunction<float> window(static_cast<size_t>(fftSize), 
            juce::dsp::WindowingFunction<float>::hann);
        
        int sectionLength = buffer.getNumSamples() / numSections;
        int validSections = 0;
        
        for (int section = 0; section < numSections; ++section)
        {
            int startSample = section * sectionLength + (sectionLength - fftSize) / 2;
            if (startSample < 0 || startSample + fftSize > buffer.getNumSamples())
                continue;
            
            // Mono-Mix der Kanäle
            for (int i = 0; i < fftSize; ++i)
            {
                float sample = 0.0f;
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    sample += buffer.getSample(ch, startSample + i);
                }
                windowedSamples[static_cast<size_t>(i)] = sample / static_cast<float>(buffer.getNumChannels());
            }
            
            // Fenster anwenden
            window.multiplyWithWindowingTable(windowedSamples.data(), static_cast<size_t>(fftSize));
            
            // FFT durchführen
            std::copy(windowedSamples.begin(), windowedSamples.end(), fftData.begin());
            fft.performFrequencyOnlyForwardTransform(fftData.data());
            
            // Magnitudes akkumulieren
            for (int i = 0; i < numBins; ++i)
            {
                float magnitude = fftData[static_cast<size_t>(i)];
                float dB = juce::Decibels::gainToDecibels(magnitude, -100.0f);
                spectrumMagnitudes[static_cast<size_t>(i)] += dB;
            }
            
            ++validSections;
        }
        
        // Durchschnitt bilden
        if (validSections > 0)
        {
            for (auto& mag : spectrumMagnitudes)
            {
                mag /= static_cast<float>(validSections);
            }
        }
    }
    
    //==========================================================================
    // Member-Variablen
    //==========================================================================
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;
    
    juce::AudioBuffer<float> referenceBuffer;
    juce::AudioBuffer<float> resampledBuffer;
    std::vector<float> spectrumMagnitudes;
    
    FFTAnalyzer fftAnalyzer;
    
    juce::File currentFile;
    double originalSampleRate = 0.0;
    int originalNumChannels = 0;
    juce::int64 originalLengthInSamples = 0;
    float durationSeconds = 0.0f;
    
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    bool loaded = false;
    
    // Playback
    bool playing = false;
    bool looping = true;
    juce::int64 playbackSampleIndex = 0;
    float playbackPosition = 0.0f;
    float playbackGain = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceAudioPlayer)
};
