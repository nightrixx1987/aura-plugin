#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <numeric>

/**
 * SpectralAnalysis: Fortgeschrittene Spektralanalyse mit Timbral Descriptors
 * 
 * Features:
 * - Mel-Frequency Cepstral Coefficients (MFCC)
 * - Spectral Centroid (Helligkeitsmaß)
 * - Spectral Spread (Frequenzverteilung)
 * - Spectral Flatness (Rauschen vs. Tonalität)
 * - Spectral Rolloff (Hochfrequenz-Energie)
 * - Spectral Crest Factor (Peak-to-Average Ratio)
 * - Spectral Flux (Zeitliche Veränderung)
 * - Zero Crossing Rate
 */
class SpectralAnalysis
{
public:
    //==========================================================================
    // Spektrale Metriken Struktur
    //==========================================================================
    struct SpectralMetrics
    {
        float centroid = 0.0f;          // Gewichteter Frequenz-Schwerpunkt (Hz)
        float spread = 0.0f;            // Standardabweichung um Centroid (Hz)
        float flatness = 0.0f;          // 0 = tonal, 1 = rauschig
        float rolloff = 0.0f;           // Frequenz bei 85% der Energie (Hz)
        float crestFactor = 0.0f;       // Peak/RMS Verhältnis (dB)
        float flux = 0.0f;              // Spektrale Veränderung
        float brightness = 0.0f;        // Energie über 4kHz / Gesamt (0-1)
        float warmth = 0.0f;            // Energie 100-500Hz / Gesamt (0-1)
        float presence = 0.0f;          // Energie 2-5kHz / Gesamt (0-1)
        float airiness = 0.0f;          // Energie über 10kHz / Gesamt (0-1)
        float muddiness = 0.0f;         // Energie 200-400Hz / Gesamt (0-1)
        float harshness = 0.0f;         // Resonanzen im 2-5kHz Bereich
        float tonality = 0.0f;          // Tonalität vs. Rauschen (0-1)
        float dynamicRange = 0.0f;      // Dynamischer Bereich (dB)
        
        // MFCC Koeffizienten (typisch 13)
        std::vector<float> mfcc;
        
        // Mel-Band Energien (typisch 26-40 Bänder)
        std::vector<float> melBands;
        
        // Bark-Band Energien (24 kritische Bänder)
        std::vector<float> barkBands;
    };
    
    //==========================================================================
    // Konstruktor / Destruktor
    //==========================================================================
    SpectralAnalysis()
    {
        initializeMelFilterbank();
        initializeBarkFilterbank();
    }
    
    void prepare(double newSampleRate, int newFftSize = 2048)
    {
        sampleRate = newSampleRate;
        fftSize = newFftSize;
        numBins = fftSize / 2 + 1;
        
        previousMagnitudes.resize(static_cast<size_t>(numBins), 0.0f);
        
        initializeMelFilterbank();
        initializeBarkFilterbank();
    }
    
    //==========================================================================
    // Hauptanalyse
    //==========================================================================
    SpectralMetrics analyze(const std::vector<float>& magnitudes)
    {
        SpectralMetrics metrics;
        
        if (magnitudes.empty() || magnitudes.size() < 2)
            return metrics;
        
        // Konvertiere zu linearer Energie wenn nötig (Eingang ist dB)
        std::vector<float> linearMags(magnitudes.size());
        for (size_t i = 0; i < magnitudes.size(); ++i)
        {
            // dB zu linear: 10^(dB/20)
            linearMags[i] = std::pow(10.0f, magnitudes[i] / 20.0f);
        }
        
        // Spektrale Metriken berechnen
        metrics.centroid = calculateCentroid(linearMags);
        metrics.spread = calculateSpread(linearMags, metrics.centroid);
        metrics.flatness = calculateFlatness(linearMags);
        metrics.rolloff = calculateRolloff(linearMags, 0.85f);
        metrics.crestFactor = calculateCrestFactor(linearMags);
        metrics.flux = calculateFlux(linearMags);
        
        // Frequenzband-Energie-Verhältnisse
        metrics.brightness = calculateBandRatio(linearMags, 4000.0f, 20000.0f);
        metrics.warmth = calculateBandRatio(linearMags, 100.0f, 500.0f);
        metrics.presence = calculateBandRatio(linearMags, 2000.0f, 5000.0f);
        metrics.airiness = calculateBandRatio(linearMags, 10000.0f, 20000.0f);
        metrics.muddiness = calculateBandRatio(linearMags, 200.0f, 400.0f);
        
        // Tonalität (inverse von Flatness mit Scaling)
        metrics.tonality = 1.0f - metrics.flatness;
        
        // Harshness Detection (Resonanzen im kritischen Bereich)
        metrics.harshness = detectHarshness(magnitudes);
        
        // Dynamischer Bereich
        metrics.dynamicRange = calculateDynamicRange(magnitudes);
        
        // Mel-Band Energien
        metrics.melBands = calculateMelBands(linearMags);
        
        // Bark-Band Energien  
        metrics.barkBands = calculateBarkBands(linearMags);
        
        // MFCC berechnen
        metrics.mfcc = calculateMFCC(metrics.melBands);
        
        // Vorherige Magnitudes für Flux-Berechnung speichern
        previousMagnitudes = linearMags;
        
        return metrics;
    }
    
    //==========================================================================
    // Getter
    //==========================================================================
    double getSampleRate() const { return sampleRate; }
    int getFFTSize() const { return fftSize; }
    int getNumBins() const { return numBins; }
    
private:
    double sampleRate = 44100.0;
    int fftSize = 2048;
    int numBins = 1025;
    
    std::vector<float> previousMagnitudes;
    
    // Mel-Filterbank
    static constexpr int numMelBands = 26;
    std::vector<std::vector<float>> melFilterbank;
    
    // Bark-Filterbank (24 kritische Bänder)
    static constexpr int numBarkBands = 24;
    std::vector<std::vector<float>> barkFilterbank;
    
    //==========================================================================
    // Hilfsfunktionen
    //==========================================================================
    float binToFrequency(int bin) const
    {
        return static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    }
    
    int frequencyToBin(float freq) const
    {
        return static_cast<int>(std::round(freq * static_cast<float>(fftSize) / static_cast<float>(sampleRate)));
    }
    
    // Mel-Skala Konvertierung
    static float hzToMel(float hz)
    {
        return 2595.0f * std::log10(1.0f + hz / 700.0f);
    }
    
    static float melToHz(float mel)
    {
        return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
    }
    
    // Bark-Skala Konvertierung
    static float hzToBark(float hz)
    {
        return 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
    }
    
    //==========================================================================
    // Spektrale Metriken
    //==========================================================================
    float calculateCentroid(const std::vector<float>& mags) const
    {
        float weightedSum = 0.0f;
        float totalEnergy = 0.0f;
        
        for (size_t i = 1; i < mags.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            float energy = mags[i] * mags[i];
            weightedSum += freq * energy;
            totalEnergy += energy;
        }
        
        return totalEnergy > 0.0f ? weightedSum / totalEnergy : 0.0f;
    }
    
    float calculateSpread(const std::vector<float>& mags, float centroid) const
    {
        float varianceSum = 0.0f;
        float totalEnergy = 0.0f;
        
        for (size_t i = 1; i < mags.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            float energy = mags[i] * mags[i];
            float diff = freq - centroid;
            varianceSum += diff * diff * energy;
            totalEnergy += energy;
        }
        
        return totalEnergy > 0.0f ? std::sqrt(varianceSum / totalEnergy) : 0.0f;
    }
    
    float calculateFlatness(const std::vector<float>& mags) const
    {
        // Geometrischer vs. Arithmetischer Mittelwert
        float logSum = 0.0f;
        float linearSum = 0.0f;
        int count = 0;
        
        for (size_t i = 1; i < mags.size(); ++i)
        {
            if (mags[i] > 1e-10f)
            {
                logSum += std::log(mags[i]);
                linearSum += mags[i];
                ++count;
            }
        }
        
        if (count == 0 || linearSum == 0.0f)
            return 0.0f;
        
        float geometricMean = std::exp(logSum / static_cast<float>(count));
        float arithmeticMean = linearSum / static_cast<float>(count);
        
        return juce::jlimit(0.0f, 1.0f, geometricMean / arithmeticMean);
    }
    
    float calculateRolloff(const std::vector<float>& mags, float percentage) const
    {
        float totalEnergy = 0.0f;
        for (size_t i = 1; i < mags.size(); ++i)
            totalEnergy += mags[i] * mags[i];
        
        float threshold = totalEnergy * percentage;
        float cumulativeEnergy = 0.0f;
        
        for (size_t i = 1; i < mags.size(); ++i)
        {
            cumulativeEnergy += mags[i] * mags[i];
            if (cumulativeEnergy >= threshold)
                return binToFrequency(static_cast<int>(i));
        }
        
        return binToFrequency(static_cast<int>(mags.size() - 1));
    }
    
    float calculateCrestFactor(const std::vector<float>& mags) const
    {
        if (mags.empty())
            return 0.0f;
        
        float peak = *std::max_element(mags.begin(), mags.end());
        float rms = 0.0f;
        
        for (const auto& m : mags)
            rms += m * m;
        
        rms = std::sqrt(rms / static_cast<float>(mags.size()));
        
        if (rms > 0.0f)
            return 20.0f * std::log10(peak / rms);
        
        return 0.0f;
    }
    
    float calculateFlux(const std::vector<float>& mags) const
    {
        if (previousMagnitudes.size() != mags.size())
            return 0.0f;
        
        float flux = 0.0f;
        for (size_t i = 0; i < mags.size(); ++i)
        {
            float diff = mags[i] - previousMagnitudes[i];
            // Half-wave rectification (nur positive Änderungen)
            flux += std::max(0.0f, diff) * std::max(0.0f, diff);
        }
        
        return std::sqrt(flux);
    }
    
    float calculateBandRatio(const std::vector<float>& mags, float lowFreq, float highFreq) const
    {
        int lowBin = frequencyToBin(lowFreq);
        int highBin = frequencyToBin(highFreq);
        lowBin = juce::jlimit(0, numBins - 1, lowBin);
        highBin = juce::jlimit(lowBin, numBins, highBin);
        
        float bandEnergy = 0.0f;
        float totalEnergy = 0.0f;
        
        for (size_t i = 1; i < mags.size(); ++i)
        {
            float energy = mags[i] * mags[i];
            totalEnergy += energy;
            
            if (static_cast<int>(i) >= lowBin && static_cast<int>(i) < highBin)
                bandEnergy += energy;
        }
        
        return totalEnergy > 0.0f ? bandEnergy / totalEnergy : 0.0f;
    }
    
    float detectHarshness(const std::vector<float>& dbMags) const
    {
        // Suche nach Resonanzen im 2-5 kHz Bereich
        int lowBin = frequencyToBin(2000.0f);
        int highBin = frequencyToBin(5000.0f);
        lowBin = juce::jlimit(0, numBins - 1, lowBin);
        highBin = juce::jlimit(lowBin + 1, numBins, highBin);
        
        float maxPeak = -120.0f;
        float avgLevel = 0.0f;
        int count = 0;
        
        for (int i = lowBin; i < highBin; ++i)
        {
            float mag = dbMags[static_cast<size_t>(i)];
            maxPeak = std::max(maxPeak, mag);
            avgLevel += mag;
            ++count;
        }
        
        if (count > 0)
            avgLevel /= static_cast<float>(count);
        
        // Harshness = wie viel der Peak über dem Durchschnitt liegt
        float deviation = maxPeak - avgLevel;
        return juce::jlimit(0.0f, 1.0f, deviation / 20.0f);
    }
    
    float calculateDynamicRange(const std::vector<float>& dbMags) const
    {
        float maxLevel = -120.0f;
        float minLevel = 0.0f;
        
        for (const auto& db : dbMags)
        {
            if (db > -100.0f)  // Ignoriere sehr leise Bins
            {
                maxLevel = std::max(maxLevel, db);
                minLevel = std::min(minLevel, db);
            }
        }
        
        return maxLevel - minLevel;
    }
    
    //==========================================================================
    // Mel-Filterbank
    //==========================================================================
    void initializeMelFilterbank()
    {
        melFilterbank.clear();
        melFilterbank.resize(numMelBands);
        
        float melMin = hzToMel(20.0f);
        float melMax = hzToMel(static_cast<float>(sampleRate) / 2.0f);
        
        std::vector<float> melPoints(numMelBands + 2);
        for (int i = 0; i <= numMelBands + 1; ++i)
        {
            melPoints[static_cast<size_t>(i)] = melMin + static_cast<float>(i) * (melMax - melMin) / static_cast<float>(numMelBands + 1);
        }
        
        std::vector<int> binPoints(numMelBands + 2);
        for (int i = 0; i <= numMelBands + 1; ++i)
        {
            float hz = melToHz(melPoints[static_cast<size_t>(i)]);
            binPoints[static_cast<size_t>(i)] = frequencyToBin(hz);
        }
        
        for (int m = 0; m < numMelBands; ++m)
        {
            melFilterbank[static_cast<size_t>(m)].resize(static_cast<size_t>(numBins), 0.0f);
            
            for (int k = binPoints[static_cast<size_t>(m)]; k < binPoints[static_cast<size_t>(m + 1)]; ++k)
            {
                if (k >= 0 && k < numBins)
                {
                    melFilterbank[static_cast<size_t>(m)][static_cast<size_t>(k)] = 
                        static_cast<float>(k - binPoints[static_cast<size_t>(m)]) / 
                        static_cast<float>(binPoints[static_cast<size_t>(m + 1)] - binPoints[static_cast<size_t>(m)]);
                }
            }
            
            for (int k = binPoints[static_cast<size_t>(m + 1)]; k < binPoints[static_cast<size_t>(m + 2)]; ++k)
            {
                if (k >= 0 && k < numBins)
                {
                    melFilterbank[static_cast<size_t>(m)][static_cast<size_t>(k)] = 
                        static_cast<float>(binPoints[static_cast<size_t>(m + 2)] - k) / 
                        static_cast<float>(binPoints[static_cast<size_t>(m + 2)] - binPoints[static_cast<size_t>(m + 1)]);
                }
            }
        }
    }
    
    std::vector<float> calculateMelBands(const std::vector<float>& mags) const
    {
        std::vector<float> melEnergies(numMelBands, 0.0f);
        
        for (int m = 0; m < numMelBands; ++m)
        {
            float energy = 0.0f;
            for (size_t k = 0; k < std::min(mags.size(), melFilterbank[static_cast<size_t>(m)].size()); ++k)
            {
                energy += mags[k] * mags[k] * melFilterbank[static_cast<size_t>(m)][k];
            }
            melEnergies[static_cast<size_t>(m)] = energy;
        }
        
        return melEnergies;
    }
    
    //==========================================================================
    // Bark-Filterbank (24 kritische Bänder)
    //==========================================================================
    void initializeBarkFilterbank()
    {
        // Bark-Band Grenzen (Hz) - basierend auf kritischen Bändern
        static const float barkEdges[25] = {
            20, 100, 200, 300, 400, 510, 630, 770, 920, 1080,
            1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300,
            6400, 7700, 9500, 12000, 15500
        };
        
        barkFilterbank.clear();
        barkFilterbank.resize(numBarkBands);
        
        for (int b = 0; b < numBarkBands; ++b)
        {
            barkFilterbank[static_cast<size_t>(b)].resize(static_cast<size_t>(numBins), 0.0f);
            
            int lowBin = frequencyToBin(barkEdges[b]);
            int highBin = frequencyToBin(barkEdges[b + 1]);
            lowBin = juce::jlimit(0, numBins - 1, lowBin);
            highBin = juce::jlimit(lowBin, numBins, highBin);
            
            for (int k = lowBin; k < highBin; ++k)
            {
                barkFilterbank[static_cast<size_t>(b)][static_cast<size_t>(k)] = 1.0f;
            }
        }
    }
    
    std::vector<float> calculateBarkBands(const std::vector<float>& mags) const
    {
        std::vector<float> barkEnergies(numBarkBands, 0.0f);
        
        for (int b = 0; b < numBarkBands; ++b)
        {
            float energy = 0.0f;
            int count = 0;
            
            for (size_t k = 0; k < std::min(mags.size(), barkFilterbank[static_cast<size_t>(b)].size()); ++k)
            {
                if (barkFilterbank[static_cast<size_t>(b)][k] > 0.0f)
                {
                    energy += mags[k] * mags[k];
                    ++count;
                }
            }
            
            barkEnergies[static_cast<size_t>(b)] = count > 0 ? energy / static_cast<float>(count) : 0.0f;
        }
        
        return barkEnergies;
    }
    
    //==========================================================================
    // MFCC Berechnung
    //==========================================================================
    std::vector<float> calculateMFCC(const std::vector<float>& melEnergies) const
    {
        static constexpr int numMFCC = 13;
        std::vector<float> mfcc(numMFCC, 0.0f);
        
        // Log-Energie der Mel-Bänder
        std::vector<float> logMelEnergies(melEnergies.size());
        for (size_t i = 0; i < melEnergies.size(); ++i)
        {
            logMelEnergies[i] = std::log(std::max(1e-10f, melEnergies[i]));
        }
        
        // DCT (Discrete Cosine Transform) Type-II
        for (int i = 0; i < numMFCC; ++i)
        {
            float sum = 0.0f;
            for (size_t j = 0; j < logMelEnergies.size(); ++j)
            {
                sum += logMelEnergies[j] * std::cos(
                    juce::MathConstants<float>::pi * static_cast<float>(i) * 
                    (static_cast<float>(j) + 0.5f) / static_cast<float>(logMelEnergies.size())
                );
            }
            mfcc[static_cast<size_t>(i)] = sum;
        }
        
        return mfcc;
    }
};
