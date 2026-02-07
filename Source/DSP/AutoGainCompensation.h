#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <array>

/**
 * AutoGainCompensation: Automatische Lautstärkekorrektur nach EQ-Änderungen
 * 
 * Features:
 * - Echtzeit-Kompensation basierend auf EQ-Kurve
 * - LUFS-basierte Messung
 * - Glatte Gain-Übergänge
 * - Bypass-sicher
 */
class AutoGainCompensation
{
public:
    //==========================================================================
    // Konstruktor
    //==========================================================================
    AutoGainCompensation() = default;
    
    void prepare(double newSampleRate, int newBlockSize)
    {
        sampleRate = newSampleRate;
        blockSize = newBlockSize;
        
        // Smoothing-Koeffizient (ca. 50ms)
        smoothingCoeff = std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.05f / static_cast<float>(blockSize)));
        
        reset();
    }
    
    void reset()
    {
        currentGain = 1.0f;
        targetGain = 1.0f;
        inputRmsAccumulator = 0.0f;
        outputRmsAccumulator = 0.0f;
        measurementCount = 0;
    }
    
    //==========================================================================
    // Messung
    //==========================================================================
    
    /**
     * Misst Input-Level (vor EQ)
     */
    void measureInput(const juce::AudioBuffer<float>& buffer)
    {
        if (!enabled)
            return;
        
        float rms = 0.0f;
        int totalSamples = 0;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                rms += data[i] * data[i];
                ++totalSamples;
            }
        }
        
        if (totalSamples > 0)
        {
            inputRmsAccumulator += rms / static_cast<float>(totalSamples);
            ++measurementCount;
        }
    }
    
    /**
     * Misst Output-Level (nach EQ) und berechnet Kompensation
     */
    void measureOutputAndCompensate(juce::AudioBuffer<float>& buffer)
    {
        if (!enabled)
            return;
        
        float rms = 0.0f;
        int totalSamples = 0;
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                rms += data[i] * data[i];
                ++totalSamples;
            }
        }
        
        if (totalSamples > 0)
        {
            outputRmsAccumulator += rms / static_cast<float>(totalSamples);
        }
        
        // Nach genügend Samples: Gain berechnen
        if (measurementCount >= measurementWindow)
        {
            calculateCompensationGain();
            measurementCount = 0;
            inputRmsAccumulator = 0.0f;
            outputRmsAccumulator = 0.0f;
        }
        
        // Gain anwenden
        applyGain(buffer);
    }
    
    //==========================================================================
    // Alternative: Kurven-basierte Berechnung
    //==========================================================================
    
    /**
     * Berechnet Gain-Kompensation basierend auf EQ-Kurve (ohne Audio-Messung)
     * Nützlich für schnellere Reaktion
     * 
     * @param eqGains Array mit EQ-Gains in dB für jedes Band
     * @param eqFrequencies Entsprechende Frequenzen
     * @param numBands Anzahl der Bänder
     */
    void calculateFromEQCurve(const float* eqGains, const float* eqFrequencies, int numBands)
    {
        if (!enabled)
            return;
        
        // Gewichteten Durchschnitt der Gains berechnen (im dB-Bereich)
        // Gewichtung nach Frequenz (Mitten wichtiger)
        float weightedDbSum = 0.0f;
        float totalWeight = 0.0f;
        
        for (int i = 0; i < numBands; ++i)
        {
            if (std::abs(eqGains[i]) > 0.1f)  // Nur aktive Bänder
            {
                // Gewichtung: 1-4kHz Bereich hat mehr Gewicht
                float freq = eqFrequencies[i];
                float weight = 1.0f;
                
                if (freq >= 500.0f && freq <= 4000.0f)
                    weight = 2.0f;
                else if (freq < 100.0f || freq > 10000.0f)
                    weight = 0.5f;
                
                // Gewichtete Summe im dB-Bereich (korrekt für Lautstärkewahrnehmung)
                weightedDbSum += eqGains[i] * weight;
                totalWeight += weight;
            }
        }
        
        if (totalWeight > 0.0f)
        {
            float avgDb = weightedDbSum / totalWeight;
            float avgLinearGain = std::pow(10.0f, avgDb / 20.0f);
            targetGain = 1.0f / avgLinearGain;  // Invertieren für Kompensation
            targetGain = juce::jlimit(0.25f, 4.0f, targetGain);  // -12dB bis +12dB
        }
    }
    
    //==========================================================================
    // Getter / Setter
    //==========================================================================
    
    void setEnabled(bool shouldBeEnabled)
    {
        enabled = shouldBeEnabled;
        if (!enabled)
        {
            targetGain = 1.0f;
            currentGain = 1.0f;
        }
    }
    
    bool isEnabled() const { return enabled; }
    
    float getCurrentGainDb() const
    {
        return 20.0f * std::log10(currentGain + 1e-10f);
    }
    
    float getTargetGainDb() const
    {
        return 20.0f * std::log10(targetGain + 1e-10f);
    }
    
    void setMaxCompensation(float maxDb)
    {
        maxCompensationDb = juce::jlimit(0.0f, 24.0f, maxDb);
    }
    
private:
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    bool enabled = false;
    
    float currentGain = 1.0f;
    float targetGain = 1.0f;
    float smoothingCoeff = 0.99f;
    
    float inputRmsAccumulator = 0.0f;
    float outputRmsAccumulator = 0.0f;
    int measurementCount = 0;
    int measurementWindow = 10;  // Blöcke für Mittelung
    
    float maxCompensationDb = 12.0f;
    
    //==========================================================================
    // Interne Funktionen
    //==========================================================================
    
    void calculateCompensationGain()
    {
        if (measurementCount == 0)
            return;
        
        float inputRms = std::sqrt(inputRmsAccumulator / static_cast<float>(measurementCount));
        float outputRms = std::sqrt(outputRmsAccumulator / static_cast<float>(measurementCount));
        
        if (outputRms > 1e-6f && inputRms > 1e-6f)
        {
            // Gain = Input / Output (um Output auf Input-Level zu bringen)
            float newGain = inputRms / outputRms;
            
            // Limit
            float maxGainLinear = std::pow(10.0f, maxCompensationDb / 20.0f);
            float minGainLinear = std::pow(10.0f, -maxCompensationDb / 20.0f);
            
            targetGain = juce::jlimit(minGainLinear, maxGainLinear, newGain);
        }
    }
    
    void applyGain(juce::AudioBuffer<float>& buffer)
    {
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        
        // Channel-Pointer einmalig cachen (statt numSamples*numChannels mal getWritePointer)
        float* channelPtrs[16];
        const int maxCh = std::min(numCh, 16);
        for (int ch = 0; ch < maxCh; ++ch)
            channelPtrs[ch] = buffer.getWritePointer(ch);
        
        // Smoothed Gain-Übergang
        for (int i = 0; i < numSamples; ++i)
        {
            currentGain = smoothingCoeff * currentGain + (1.0f - smoothingCoeff) * targetGain;
            
            for (int ch = 0; ch < maxCh; ++ch)
            {
                channelPtrs[ch][i] *= currentGain;
            }
        }
    }
};
