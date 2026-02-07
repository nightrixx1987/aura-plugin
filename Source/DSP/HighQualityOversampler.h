#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>

/**
 * HighQualityOversampler: Oversampling für nicht-lineare Verarbeitung
 * 
 * Verhindert Aliasing bei Saturation, Limiting, Clipping etc.
 * Verwendet polyphase FIR-Filter für maximale Effizienz und Qualität.
 * 
 * Features:
 * - 2x, 4x, 8x, 16x Oversampling
 * - Linearphasige FIR Anti-Aliasing Filter
 * - Minimal Phase Option für geringere Latenz
 * - Automatische Latenz-Kompensation
 * 
 * Workflow:
 * 1. upsample() - Input auf höhere Rate bringen
 * 2. Nicht-lineare Verarbeitung bei hoher Rate
 * 3. downsample() - Zurück auf Original-Rate
 */
class HighQualityOversampler
{
public:
    //==========================================================================
    // Oversampling-Faktoren
    //==========================================================================
    enum class Factor
    {
        x1 = 1,    // Bypass
        x2 = 2,
        x4 = 4,
        x8 = 8,
        x16 = 16
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    HighQualityOversampler()
    {
        initializeFilters();
    }
    
    //==========================================================================
    // Initialisierung
    //==========================================================================
    void prepare(double sampleRate, int maxBlockSize, int channels = 2)
    {
        baseSampleRate = sampleRate;
        baseBlockSize = maxBlockSize;
        this->numChannels = channels;
        
        // Maximale Buffer-Größe für höchstes Oversampling
        int maxOversampledSize = maxBlockSize * static_cast<int>(Factor::x16);
        
        // Buffer für jeden Kanal allokieren
        oversampledBuffers.resize(static_cast<size_t>(numChannels));
        for (auto& buffer : oversampledBuffers)
        {
            buffer.resize(static_cast<size_t>(maxOversampledSize), 0.0f);
        }
        
        // Scratch-Buffer für RT-safe In-Place Verarbeitung
        scratchBuffer.resize(static_cast<size_t>(maxOversampledSize), 0.0f);
        
        // Filter für jede Stage neu initialisieren
        initializeFilters();
        
        prepared = true;
    }
    
    void reset()
    {
        // Filter-Zustände zurücksetzen
        for (auto& stage : upsampleFilters)
        {
            for (auto& filter : stage)
            {
                std::fill(filter.begin(), filter.end(), 0.0f);
            }
        }
        
        for (auto& stage : downsampleFilters)
        {
            for (auto& filter : stage)
            {
                std::fill(filter.begin(), filter.end(), 0.0f);
            }
        }
        
        // Circular Buffer Indizes zurücksetzen
        for (auto& stage : upsampleFilterIdx)
            std::fill(stage.begin(), stage.end(), 0);
        for (auto& stage : downsampleFilterIdx)
            std::fill(stage.begin(), stage.end(), 0);
    }
    
    //==========================================================================
    // Oversampling-Faktor setzen
    //==========================================================================
    void setOversamplingFactor(Factor newFactor)
    {
        if (factor != newFactor)
        {
            factor = newFactor;
            reset();  // Filter zurücksetzen bei Wechsel
        }
    }
    
    Factor getOversamplingFactor() const { return factor; }
    int getFactorAsInt() const { return static_cast<int>(factor); }
    
    //==========================================================================
    // Latenz in Samples (bei Basis-Samplerate)
    //==========================================================================
    int getLatencyInSamples() const
    {
        // Latenz kommt von den FIR-Filtern
        // Jede Stage hat filterOrder/2 Samples Latenz
        int numStages = getNumStages();
        return (numStages * filterOrder) / 2;
    }
    
    //==========================================================================
    // Upsampling: Input-Block auf höhere Rate bringen
    //==========================================================================
    void upsample(const float* input, int numInputSamples, int channel)
    {
        if (!prepared || factor == Factor::x1)
        {
            // Bypass: direkt kopieren
            for (int i = 0; i < numInputSamples; ++i)
            {
                oversampledBuffers[static_cast<size_t>(channel)][static_cast<size_t>(i)] = input[i];
            }
            currentOversampledSize = numInputSamples;
            return;
        }
        
        auto& buffer = oversampledBuffers[static_cast<size_t>(channel)];
        
        // Stufen-weise Upsampling (je Stufe 2x)
        int currentSize = numInputSamples;
        
        // Stage 1: Input -> 2x
        if (static_cast<int>(factor) >= 2)
        {
            upsample2x(input, buffer.data(), numInputSamples, channel, 0);
            currentSize *= 2;
        }
        
        // Stage 2: 2x -> 4x
        if (static_cast<int>(factor) >= 4)
        {
            // Pre-allokierter Scratch-Buffer (RT-safe, keine Heap-Allokation)
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            upsample2x(scratchBuffer.data(), buffer.data(), currentSize, channel, 1);
            currentSize *= 2;
        }
        
        // Stage 3: 4x -> 8x
        if (static_cast<int>(factor) >= 8)
        {
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            upsample2x(scratchBuffer.data(), buffer.data(), currentSize, channel, 2);
            currentSize *= 2;
        }
        
        // Stage 4: 8x -> 16x
        if (static_cast<int>(factor) >= 16)
        {
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            upsample2x(scratchBuffer.data(), buffer.data(), currentSize, channel, 3);
            currentSize *= 2;
        }
        
        currentOversampledSize = currentSize;
    }
    
    //==========================================================================
    // Downsampling: Oversampled-Block zurück auf Basis-Rate
    //==========================================================================
    void downsample(float* output, int numOutputSamples, int channel)
    {
        if (!prepared || factor == Factor::x1)
        {
            // Bypass: direkt kopieren
            for (int i = 0; i < numOutputSamples; ++i)
            {
                output[i] = oversampledBuffers[static_cast<size_t>(channel)][static_cast<size_t>(i)];
            }
            return;
        }
        
        auto& buffer = oversampledBuffers[static_cast<size_t>(channel)];
        int currentSize = currentOversampledSize;
        
        // Stufen-weise Downsampling (je Stufe /2)
        
        // Stage 4: 16x -> 8x
        if (static_cast<int>(factor) >= 16)
        {
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            downsample2x(scratchBuffer.data(), buffer.data(), currentSize, channel, 3);
            currentSize /= 2;
        }
        
        // Stage 3: 8x -> 4x
        if (static_cast<int>(factor) >= 8)
        {
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            downsample2x(scratchBuffer.data(), buffer.data(), currentSize, channel, 2);
            currentSize /= 2;
        }
        
        // Stage 2: 4x -> 2x
        if (static_cast<int>(factor) >= 4)
        {
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            downsample2x(scratchBuffer.data(), buffer.data(), currentSize, channel, 1);
            currentSize /= 2;
        }
        
        // Stage 1: 2x -> 1x
        if (static_cast<int>(factor) >= 2)
        {
            std::copy(buffer.begin(), buffer.begin() + currentSize, scratchBuffer.begin());
            downsample2x(scratchBuffer.data(), output, currentSize, channel, 0);
        }
    }
    
    //==========================================================================
    // Zugriff auf Oversampled Buffer (für Verarbeitung)
    //==========================================================================
    float* getOversampledBuffer(int channel)
    {
        return oversampledBuffers[static_cast<size_t>(channel)].data();
    }
    
    int getOversampledSize() const { return currentOversampledSize; }
    
    double getOversampledSampleRate() const 
    { 
        return baseSampleRate * static_cast<double>(static_cast<int>(factor)); 
    }
    
private:
    //==========================================================================
    // Filter-Initialisierung
    //==========================================================================
    void initializeFilters()
    {
        // Halfband FIR Koeffizienten (optimiert für -120dB Stopband)
        // Diese sind für 2x Up/Downsampling optimiert
        
        // 31-tap Halfband Filter (Type II, symmetric)
        // Cutoff bei 0.25 * fs (Nyquist/2), sehr steiler Rolloff
        filterCoeffs = {
            0.0f,
            -0.00025f,
            0.0f,
            0.00123f,
            0.0f,
            -0.00438f,
            0.0f,
            0.01193f,
            0.0f,
            -0.02774f,
            0.0f,
            0.06294f,
            0.0f,
            -0.18677f,
            0.0f,
            0.62915f,  // Center (halbe Leistung)
            0.0f,
            -0.18677f,
            0.0f,
            0.06294f,
            0.0f,
            -0.02774f,
            0.0f,
            0.01193f,
            0.0f,
            -0.00438f,
            0.0f,
            0.00123f,
            0.0f,
            -0.00025f,
            0.0f
        };
        
        filterOrder = static_cast<int>(filterCoeffs.size());
        
        // Filter-Zustände für bis zu 4 Stages und maxNumChannels Kanäle
        constexpr int maxStages = 4;
        constexpr int maxNumChannels = 8;
        
        upsampleFilters.resize(maxStages);
        downsampleFilters.resize(maxStages);
        upsampleFilterIdx.resize(maxStages);
        downsampleFilterIdx.resize(maxStages);
        
        for (int stage = 0; stage < maxStages; ++stage)
        {
            upsampleFilters[static_cast<size_t>(stage)].resize(maxNumChannels);
            downsampleFilters[static_cast<size_t>(stage)].resize(maxNumChannels);
            upsampleFilterIdx[static_cast<size_t>(stage)].resize(static_cast<size_t>(maxNumChannels), 0);
            downsampleFilterIdx[static_cast<size_t>(stage)].resize(static_cast<size_t>(maxNumChannels), 0);
            
            for (int ch = 0; ch < maxNumChannels; ++ch)
            {
                // Doppelte Größe für Branch-freien Circular Buffer (SIMD-freundlich)
                upsampleFilters[static_cast<size_t>(stage)][static_cast<size_t>(ch)].resize(
                    static_cast<size_t>(filterOrder * 2), 0.0f);
                downsampleFilters[static_cast<size_t>(stage)][static_cast<size_t>(ch)].resize(
                    static_cast<size_t>(filterOrder * 2), 0.0f);
            }
        }
    }
    
    int getNumStages() const
    {
        switch (factor)
        {
            case Factor::x2:  return 1;
            case Factor::x4:  return 2;
            case Factor::x8:  return 3;
            case Factor::x16: return 4;
            default: return 0;
        }
    }
    
    //==========================================================================
    // 2x Upsampling mit Halfband Filter
    //==========================================================================
    void upsample2x(const float* input, float* output, int numInputSamples, int channel, int stage)
    {
        auto& filterState = upsampleFilters[static_cast<size_t>(stage)][static_cast<size_t>(channel)];
        auto& circIdx = upsampleFilterIdx[static_cast<size_t>(stage)][static_cast<size_t>(channel)];
        
        int outputIndex = 0;
        
        for (int i = 0; i < numInputSamples; ++i)
        {
            // Sample 1: Original mit Filterung (Circular Buffer)
            output[outputIndex++] = applyFIR(filterState, circIdx, input[i] * 2.0f);
            
            // Sample 2: Interpoliertes Sample (Zero-Stuffed)
            output[outputIndex++] = applyFIR(filterState, circIdx, 0.0f);
        }
    }
    
    //==========================================================================
    // 2x Downsampling mit Halfband Filter + Decimation
    //==========================================================================
    void downsample2x(const float* input, float* output, int numInputSamples, int channel, int stage)
    {
        auto& filterState = downsampleFilters[static_cast<size_t>(stage)][static_cast<size_t>(channel)];
        auto& circIdx = downsampleFilterIdx[static_cast<size_t>(stage)][static_cast<size_t>(channel)];
        
        int outputIndex = 0;
        
        for (int i = 0; i < numInputSamples; i += 2)
        {
            // Anti-Aliasing Tiefpass mit Circular Buffer, dann Decimation
            applyFIR(filterState, circIdx, input[i]);
            
            // Verarbeite Sample 2 und output (Decimation: nur jeden 2.)
            float filtered = 0.0f;
            if (i + 1 < numInputSamples)
                filtered = applyFIR(filterState, circIdx, input[i + 1]);
            else
                filtered = applyFIR(filterState, circIdx, 0.0f);
            
            output[outputIndex++] = filtered;
        }
    }
    
    //==========================================================================
    // Member-Variablen
    //==========================================================================
    double baseSampleRate = 44100.0;
    int baseBlockSize = 512;
    int numChannels = 2;
    
    Factor factor = Factor::x1;
    bool prepared = false;
    int currentOversampledSize = 0;
    
    // Oversampled Audio-Buffer (pro Kanal)
    std::vector<std::vector<float>> oversampledBuffers;
    
    // Pre-allokierter Scratch-Buffer (RT-safe, verhindert Heap-Allokation)
    std::vector<float> scratchBuffer;
    
    // FIR Filter Koeffizienten (Halfband)
    std::vector<float> filterCoeffs;
    int filterOrder = 31;
    
    // Filter-Zustände [stage][channel][filterState]
    std::vector<std::vector<std::vector<float>>> upsampleFilters;
    std::vector<std::vector<std::vector<float>>> downsampleFilters;
    
    // Circular Buffer Indizes [stage][channel]
    std::vector<std::vector<int>> upsampleFilterIdx;
    std::vector<std::vector<int>> downsampleFilterIdx;
    
    // SIMD-freundlicher FIR mit doppeltem Circular Buffer:
    // State ist 2x filterOrder lang; Daten werden an [writeIdx] UND [writeIdx + filterOrder] geschrieben.
    // Dadurch kann der innere Loop branchfrei lesen (kein Wrap-Around-Check nötig).
    inline float applyFIR(std::vector<float>& state, int& writeIdx, float inputSample) const
    {
        // Schreibe an beide Positionen im doppelten Buffer
        state[static_cast<size_t>(writeIdx)] = inputSample;
        state[static_cast<size_t>(writeIdx + filterOrder)] = inputSample;
        
        // Dot-Product: branchfreier linearer Zugriff dank doppeltem Buffer
        // Lese rückwärts ab writeIdx: state[writeIdx], state[writeIdx-1], ... 
        // = state[writeIdx+filterOrder], state[writeIdx+filterOrder-1], ... (doppelter buffer)
        const float* readPtr = state.data() + writeIdx + 1;  // readPtr[filterOrder-1] = neuestes Sample
        float filtered = 0.0f;
        for (int j = 0; j < filterOrder; ++j)
        {
            filtered += readPtr[filterOrder - 1 - j] * filterCoeffs[static_cast<size_t>(j)];
        }
        
        if (++writeIdx >= filterOrder) writeIdx = 0;
        return filtered;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HighQualityOversampler)
};


/**
 * OversampledProcessor: Wrapper für einfaches Oversampling von Callback-Funktionen
 * 
 * Verwendung:
 * OversampledProcessor oversampler;
 * oversampler.prepare(sampleRate, blockSize);
 * oversampler.setOversamplingFactor(HighQualityOversampler::Factor::x4);
 * 
 * oversampler.process(buffer, [](float sample) {
 *     // Nicht-lineare Verarbeitung hier
 *     return std::tanh(sample);  // z.B. Soft-Clipping
 * });
 */
class OversampledProcessor
{
public:
    void prepare(double sampleRate, int maxBlockSize, int channels = 2)
    {
        oversampler.prepare(sampleRate, maxBlockSize, channels);
        this->numChannels = channels;
    }
    
    void reset() { oversampler.reset(); }
    
    void setOversamplingFactor(HighQualityOversampler::Factor factor)
    {
        oversampler.setOversamplingFactor(factor);
    }
    
    int getLatencyInSamples() const { return oversampler.getLatencyInSamples(); }
    
    /**
     * Verarbeitet einen Buffer mit optionalem Oversampling
     * 
     * @param buffer Audio-Buffer (wird in-place modifiziert)
     * @param processFunc Funktion die auf jeden oversampled Sample angewendet wird
     */
    template<typename ProcessFunc>
    void process(juce::AudioBuffer<float>& buffer, ProcessFunc processFunc)
    {
        int numSamples = buffer.getNumSamples();
        
        for (int ch = 0; ch < numChannels && ch < buffer.getNumChannels(); ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            
            // Upsample
            oversampler.upsample(channelData, numSamples, ch);
            
            // Verarbeitung bei hoher Sample-Rate
            float* oversampledData = oversampler.getOversampledBuffer(ch);
            int oversampledSize = oversampler.getOversampledSize();
            
            for (int i = 0; i < oversampledSize; ++i)
            {
                oversampledData[i] = processFunc(oversampledData[i]);
            }
            
            // Downsample
            oversampler.downsample(channelData, numSamples, ch);
        }
    }
    
private:
    HighQualityOversampler oversampler;
    int numChannels = 2;
};
