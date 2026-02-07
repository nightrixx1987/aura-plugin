#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <cmath>

/**
 * SpectralMatcher: Reference Track EQ-Matching System
 * 
 * Vergleicht Live-Input mit Reference-Track Spektrum und generiert
 * automatisch eine EQ-Kurve zur Angleichung.
 * 
 * Features:
 * - Echtzeit-Spektralvergleich (Live vs. Reference)
 * - Automatische EQ-Kurven-Generierung (wie FabFilter Pro-Q4)
 * - 1/3-Oktave-Glättung für natürliche Kurven
 * - Frequenzgewichtete Interpolation
 * - Max ±12dB Begrenzung für sichere Anwendung
 * - Multi-Band EQ-Punkt-Generierung
 * 
 * Inspiriert von: FabFilter Pro-Q4, Sonible smart:EQ 4, iZotope Neutron
 */
class SpectralMatcher
{
public:
    //==========================================================================
    // EQ-Punkt für Match-Kurve
    //==========================================================================
    struct MatchPoint
    {
        float frequency = 1000.0f;  // Hz
        float gainDB = 0.0f;        // Korrektur in dB
        float q = 1.0f;             // Q-Faktor (Bandbreite)
        float weight = 1.0f;        // Wichtigkeit (0-1)
        bool isBoost = false;       // true = Boost, false = Cut
    };
    
    //==========================================================================
    // Einstellungen
    //==========================================================================
    struct Settings
    {
        float maxGainDB = 6.0f;            // Max Korrektur (±dB) - konservativ!
        float maxBoostDB = 4.0f;           // Max Boost (dB) - weniger als Cuts
        float maxCutDB = 8.0f;             // Max Cut (dB)
        float smoothingOctaves = 0.33f;    // 1/3 Oktave Standard
        float minSignificantDB = 2.0f;     // Min. Differenz für Korrektur (erhöht)
        float lowFreqLimit = 40.0f;        // Untere Grenzfrequenz
        float highFreqLimit = 16000.0f;    // Obere Grenzfrequenz
        int maxMatchPoints = 12;           // Max Anzahl EQ-Punkte (erhöht für 12-Band EQ)
        float matchStrength = 0.5f;        // 0-1, wie stark gematcht wird (50%)
        bool perceptualWeighting = true;   // ISO 226 Gewichtung
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    SpectralMatcher()
    {
        initializeSmoothingWeights();
    }
    
    void prepare(double newSampleRate, int newFftSize = 4096)
    {
        sampleRate = newSampleRate;
        fftSize = newFftSize;
        numBins = fftSize / 2 + 1;
        
        // Frequenz-Array initialisieren
        binFrequencies.resize(static_cast<size_t>(numBins));
        for (int i = 0; i < numBins; ++i)
        {
            binFrequencies[static_cast<size_t>(i)] = 
                static_cast<float>(i) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        }
        
        // Smoothing-Gewichte neu berechnen
        initializeSmoothingWeights();
        
        // WICHTIG: Reference-Spektrum NICHT löschen wenn schon vorhanden!
        // Nur temporäre Buffer zurücksetzen
        inputSpectrum.clear();
        correctionCurve.clear();
        matchPoints.clear();
        needsRecalculation = true;
    }
    
    //==========================================================================
    // Reference-Spektrum setzen
    //==========================================================================
    void setReferenceSpectrum(const std::vector<float>& spectrum)
    {
        if (spectrum.empty())
        {
#if JUCE_DEBUG
            DBG("SpectralMatcher::setReferenceSpectrum - LEER! Abbruch.");
#endif
            return;
        }
        
        // Kopieren und optional glätten
        referenceSpectrum = spectrum;
        
        // 1/3-Oktave Glättung anwenden
        smoothSpectrum(referenceSpectrum);
        
        hasReference = true;
        needsRecalculation = true;
        
#if JUCE_DEBUG
        DBG("SpectralMatcher::setReferenceSpectrum - " + juce::String(referenceSpectrum.size()) + 
            " bins geladen, hasReference=true");
#endif
    }
    
    void clearReference()
    {
        referenceSpectrum.clear();
        correctionCurve.clear();
        matchPoints.clear();
        hasReference = false;
    }
    
    //==========================================================================
    // Input-Spektrum aktualisieren (pro Block)
    //==========================================================================
    void updateInputSpectrum(const std::vector<float>& spectrum)
    {
        if (spectrum.empty() || !hasReference || referenceSpectrum.empty())
        {
#if JUCE_DEBUG
            static int debugSkipCounter = 0;
            if (++debugSkipCounter % 500 == 0)
            {
                DBG("SpectralMatcher::updateInputSpectrum SKIPPED - empty=" + 
                    juce::String(spectrum.empty() ? "yes" : "no") +
                    ", hasRef=" + juce::String(hasReference ? "yes" : "no") +
                    ", refSize=" + juce::String(referenceSpectrum.size()));
            }
#endif
            return;
        }
        
        // WICHTIG: Spektrum auf Reference-Größe resamplen wenn nötig
        const std::vector<float>* spectrumToUse = &spectrum;
        
        if (spectrum.size() != referenceSpectrum.size())
        {
            resampledSpectrum_.resize(referenceSpectrum.size());
            float ratio = static_cast<float>(spectrum.size()) / static_cast<float>(referenceSpectrum.size());
            
            for (size_t i = 0; i < referenceSpectrum.size(); ++i)
            {
                float srcIdx = static_cast<float>(i) * ratio;
                size_t idx0 = static_cast<size_t>(srcIdx);
                size_t idx1 = std::min(idx0 + 1, spectrum.size() - 1);
                float frac = srcIdx - static_cast<float>(idx0);
                
                resampledSpectrum_[i] = spectrum[idx0] * (1.0f - frac) + spectrum[idx1] * frac;
            }
            spectrumToUse = &resampledSpectrum_;
            
#if JUCE_DEBUG
            static int resampleDebugCounter = 0;
            if (++resampleDebugCounter % 200 == 0)
            {
                DBG("SpectralMatcher: Resampled input " + juce::String(spectrum.size()) + 
                    " -> " + juce::String(referenceSpectrum.size()) + " bins");
            }
#endif
        }
        
        // Mit vorherigem Spektrum mitteln (Smoothing über Zeit)
        if (inputSpectrum.empty() || inputSpectrum.size() != spectrumToUse->size())
        {
            inputSpectrum = *spectrumToUse;
        }
        else
        {
            // Exponentielles Moving Average für zeitliche Glättung
            constexpr float temporalSmooth = 0.85f;
            for (size_t i = 0; i < inputSpectrum.size(); ++i)
            {
                inputSpectrum[i] = temporalSmooth * inputSpectrum[i] + 
                                   (1.0f - temporalSmooth) * (*spectrumToUse)[i];
            }
        }
        
        // Räumliche Glättung nur auf frisches Spektrum, nicht kumulativ auf EMA
        // (EMA liefert bereits zeitliche Glättung, räumliche Glättung wäre doppelt)
        
        needsRecalculation = true;
    }
    
    //==========================================================================
    // Korrektur-Kurve berechnen
    //==========================================================================
    void calculateCorrectionCurve()
    {
        if (!hasReference || referenceSpectrum.empty() || inputSpectrum.empty())
        {
            matchPoints.clear();
            return;
        }
        
        // WICHTIG: Prüfe ob überhaupt signifikantes Audio-Signal vorhanden ist!
        // Wenn Input zu leise ist, keine Korrektur berechnen
        float inputMaxDB = -100.0f;
        for (float val : inputSpectrum)
        {
            inputMaxDB = std::max(inputMaxDB, val);
        }
        
        // Wenn Input unter -60dB ist, gilt er als "still" - keine Korrektur
        constexpr float silenceThresholdDB = -60.0f;
        if (inputMaxDB < silenceThresholdDB)
        {
            matchPoints.clear();
            correctionCurve.clear();
            
#if JUCE_DEBUG
            static int silenceDebugCounter = 0;
            if (++silenceDebugCounter % 100 == 0)
            {
                DBG("calculateCorrectionCurve: Input zu leise (" + 
                    juce::String(inputMaxDB, 1) + " dB) - keine Korrektur");
            }
#endif
            return;
        }
        
        // Größe anpassen
        size_t minSize = std::min(referenceSpectrum.size(), inputSpectrum.size());
        correctionCurve.resize(minSize);
        
        // Differenz berechnen: Reference - Input = benötigte Korrektur
        for (size_t i = 0; i < minSize; ++i)
        {
            float diff = referenceSpectrum[i] - inputSpectrum[i];
            
            // Frequenz für diesen Bin
            float freq = (i < binFrequencies.size()) ? binFrequencies[i] : 0.0f;
            
            // Frequenzbereich-Prüfung
            if (freq < settings.lowFreqLimit || freq > settings.highFreqLimit)
            {
                correctionCurve[i] = 0.0f;
                continue;
            }
            
            // Perceptual Weighting anwenden (weniger Korrektur bei Extremfrequenzen)
            if (settings.perceptualWeighting)
            {
                diff *= getPerceptualWeight(freq);
            }
            
            // Match Strength anwenden
            diff *= settings.matchStrength;
            
            // Asymmetrische Limits anwenden (Boost vs. Cut unterschiedlich)
            if (diff > 0.0f)
                diff = std::min(diff, settings.maxBoostDB);
            else
                diff = std::max(diff, -settings.maxCutDB);
            
            // Minimale Signifikanz prüfen
            if (std::abs(diff) < settings.minSignificantDB)
            {
                diff = 0.0f;
            }
            
            correctionCurve[i] = diff;
        }
        
        // Match-Punkte aus Kurve extrahieren
        extractMatchPoints();
        
        needsRecalculation = false;
    }
    
    //==========================================================================
    // Match-Punkte für EQ-Bänder generieren
    //==========================================================================
    const std::vector<MatchPoint>& getMatchPoints()
    {
        if (needsRecalculation)
        {
            calculateCorrectionCurve();
            
#if JUCE_DEBUG
            static int matchDebugCounter = 0;
            if (++matchDebugCounter % 100 == 0)
            {
                DBG("SpectralMatcher::getMatchPoints - " + juce::String(matchPoints.size()) + 
                    " Punkte, refSize=" + juce::String(referenceSpectrum.size()) +
                    ", inputSize=" + juce::String(inputSpectrum.size()));
                    
                for (size_t i = 0; i < std::min(matchPoints.size(), size_t(4)); ++i)
                {
                    DBG("  Match " + juce::String(i) + ": " +
                        juce::String(matchPoints[i].frequency, 0) + " Hz, " +
                        juce::String(matchPoints[i].gainDB, 1) + " dB");
                }
            }
#endif
        }
        return matchPoints;
    }
    
    //==========================================================================
    // Korrektur für eine bestimmte Frequenz abfragen
    //==========================================================================
    float getCorrectionAtFrequency(float frequency) const
    {
        if (correctionCurve.empty() || binFrequencies.empty())
            return 0.0f;
        
        // Bin-Index finden
        float binFloat = frequency * static_cast<float>(fftSize) / static_cast<float>(sampleRate);
        int binIndex = static_cast<int>(binFloat);
        
        if (binIndex < 0 || binIndex >= static_cast<int>(correctionCurve.size()) - 1)
            return 0.0f;
        
        // Lineare Interpolation zwischen Bins
        float frac = binFloat - static_cast<float>(binIndex);
        return correctionCurve[static_cast<size_t>(binIndex)] * (1.0f - frac) +
               correctionCurve[static_cast<size_t>(binIndex + 1)] * frac;
    }
    
    //==========================================================================
    // Getter
    //==========================================================================
    bool hasReferenceLoaded() const { return hasReference; }
    const std::vector<float>& getReferenceSpectrum() const { return referenceSpectrum; }
    const std::vector<float>& getInputSpectrum() const { return inputSpectrum; }
    const std::vector<float>& getCorrectionCurve() const { return correctionCurve; }
    
    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }
    
    //==========================================================================
    // Einstellungen
    //==========================================================================
    void setMaxGain(float dB) { settings.maxGainDB = juce::jlimit(1.0f, 24.0f, dB); }
    void setMatchStrength(float strength) { settings.matchStrength = juce::jlimit(0.0f, 1.0f, strength); }
    void setSmoothing(float octaves) { settings.smoothingOctaves = juce::jlimit(0.1f, 1.0f, octaves); }
    
private:
    //==========================================================================
    // Private Hilfsfunktionen
    //==========================================================================
    
    void initializeSmoothingWeights()
    {
        // Smoothing Gewichte basierend auf smoothingOctaves-Setting
        smoothingWeights.clear();
        
        // halfWidth skaliert mit Oktav-Breite: 0.1 Oct -> 2 Bins, 0.33 Oct -> 5, 1.0 Oct -> 15
        int halfWidth = juce::jlimit(2, 15, static_cast<int>(settings.smoothingOctaves * 15.0f));
        float sigma = static_cast<float>(halfWidth) / 2.0f;
        
        for (int i = -halfWidth; i <= halfWidth; ++i)
        {
            float weight = std::exp(-static_cast<float>(i * i) / (2.0f * sigma * sigma));
            smoothingWeights.push_back(weight);
        }
        
        // Normalisieren
        float sum = 0.0f;
        for (float w : smoothingWeights) sum += w;
        for (float& w : smoothingWeights) w /= sum;
    }
    
    void smoothSpectrum(std::vector<float>& spectrum)
    {
        if (spectrum.size() < 3)
            return;
        
        // Wiederverwendbaren Member-Buffer nutzen statt lokaler Allokation
        smoothingBuffer.resize(spectrum.size());
        int halfWidth = static_cast<int>(smoothingWeights.size()) / 2;
        
        for (size_t i = 0; i < spectrum.size(); ++i)
        {
            float sum = 0.0f;
            float weightSum = 0.0f;
            
            for (int j = -halfWidth; j <= halfWidth; ++j)
            {
                int idx = static_cast<int>(i) + j;
                if (idx >= 0 && idx < static_cast<int>(spectrum.size()))
                {
                    float weight = smoothingWeights[static_cast<size_t>(j + halfWidth)];
                    sum += spectrum[static_cast<size_t>(idx)] * weight;
                    weightSum += weight;
                }
            }
            
            smoothingBuffer[i] = (weightSum > 0.0f) ? sum / weightSum : spectrum[i];
        }
        
        std::copy(smoothingBuffer.begin(), smoothingBuffer.begin() + static_cast<std::ptrdiff_t>(spectrum.size()), spectrum.begin());
    }
    
    float getPerceptualWeight(float frequency) const
    {
        // Basiert auf ISO 226:2003 Equal-Loudness Konturen (vereinfacht)
        // Weniger Gewicht bei sehr tiefen und sehr hohen Frequenzen
        
        if (frequency < 100.0f)
        {
            // Unter 100Hz: reduziertes Gewicht
            return 0.5f + 0.5f * (frequency / 100.0f);
        }
        else if (frequency > 8000.0f)
        {
            // Über 8kHz: reduziertes Gewicht
            float rolloff = (frequency - 8000.0f) / 12000.0f;
            return std::max(0.3f, 1.0f - rolloff * 0.7f);
        }
        else if (frequency >= 2000.0f && frequency <= 5000.0f)
        {
            // Empfindlichster Bereich: leicht erhöhtes Gewicht
            return 1.1f;
        }
        
        return 1.0f;
    }
    
    void extractMatchPoints()
    {
        matchPoints.clear();
        
        if (correctionCurve.empty() || binFrequencies.empty())
        {
#if JUCE_DEBUG
            DBG("extractMatchPoints: correctionCurve oder binFrequencies leer!");
#endif
            return;
        }
        
        // Debug: Zeige Korrektur-Kurven-Statistik
        float maxCorr = 0.0f, minCorr = 0.0f;
        for (float val : correctionCurve)
        {
            maxCorr = std::max(maxCorr, val);
            minCorr = std::min(minCorr, val);
        }
        
#if JUCE_DEBUG
        static int extractDebugCounter = 0;
        if (++extractDebugCounter % 100 == 0)
        {
            DBG("extractMatchPoints: corrSize=" + juce::String(correctionCurve.size()) +
                ", min=" + juce::String(minCorr, 1) + ", max=" + juce::String(maxCorr, 1));
        }
#endif
        
        // NEUER ANSATZ: 1/3-Oktav-basierte Match-Punkte für feinere Auflösung
        // 16 Center-Frequenzen über das hörbare Spektrum
        std::vector<float> octaveCenters = {
            50.0f, 80.0f, 125.0f, 200.0f, 315.0f, 500.0f, 800.0f, 1250.0f,
            2000.0f, 3150.0f, 5000.0f, 8000.0f, 12500.0f
        };
        
        for (float centerFreq : octaveCenters)
        {
            if (centerFreq < settings.lowFreqLimit || centerFreq > settings.highFreqLimit)
                continue;
            
            // Finde Frequenzbereich für diese Oktave (±0.5 Oktave)
            float lowFreq = centerFreq / 1.414f;  // sqrt(2)
            float highFreq = centerFreq * 1.414f;
            
            // Finde den Bin-Bereich
            int lowBin = static_cast<int>(lowFreq * static_cast<float>(fftSize) / static_cast<float>(sampleRate));
            int highBin = static_cast<int>(highFreq * static_cast<float>(fftSize) / static_cast<float>(sampleRate));
            
            lowBin = juce::jlimit(0, static_cast<int>(correctionCurve.size()) - 1, lowBin);
            highBin = juce::jlimit(0, static_cast<int>(correctionCurve.size()) - 1, highBin);
            
            if (lowBin >= highBin)
                continue;
            
            // Finde den größten Korrekturwert in diesem Bereich
            float maxVal = 0.0f;
            int maxBin = lowBin;
            
            for (int i = lowBin; i <= highBin; ++i)
            {
                if (std::abs(correctionCurve[static_cast<size_t>(i)]) > std::abs(maxVal))
                {
                    maxVal = correctionCurve[static_cast<size_t>(i)];
                    maxBin = i;
                }
            }
            
            // Nur signifikante Korrekturen hinzufügen
            if (std::abs(maxVal) >= settings.minSignificantDB)
            {
                MatchPoint point;
                point.frequency = binFrequencies[static_cast<size_t>(maxBin)];
                point.gainDB = maxVal;  // matchStrength ist bereits in calculateCorrectionCurve() angewendet
                
                // Separate Limits für Boost und Cut
                if (point.gainDB > 0.0f)
                {
                    // Boosts sind konservativer (zu viel Boost = laut)
                    point.gainDB = std::min(point.gainDB, settings.maxBoostDB);
                }
                else
                {
                    // Cuts können stärker sein
                    point.gainDB = std::max(point.gainDB, -settings.maxCutDB);
                }
                
                point.isBoost = point.gainDB > 0.0f;
                point.weight = std::abs(point.gainDB) / settings.maxGainDB;
                point.q = calculateQFromPeak(maxBin);  // Dynamische Q aus Peak-Breite
                
                matchPoints.push_back(point);
            }
        }
        
        // Limitiere auf maxMatchPoints
        if (matchPoints.size() > static_cast<size_t>(settings.maxMatchPoints))
        {
            // Sortiere nach Wichtigkeit (Gain-Magnitude)
            std::sort(matchPoints.begin(), matchPoints.end(),
                [](const MatchPoint& a, const MatchPoint& b) { 
                    return std::abs(a.gainDB) > std::abs(b.gainDB); 
                });
            matchPoints.resize(static_cast<size_t>(settings.maxMatchPoints));
        }
        
        // Nach Frequenz sortieren
        std::sort(matchPoints.begin(), matchPoints.end(),
            [](const MatchPoint& a, const MatchPoint& b) { return a.frequency < b.frequency; });
        
#if JUCE_DEBUG
        if (extractDebugCounter % 100 == 0)
        {
            DBG("extractMatchPoints: " + juce::String(matchPoints.size()) + " Punkte generiert");
        }
#endif
    }
    
    float calculateQFromPeak(int peakBinIdx)
    {
        // Q aus der Breite des Peaks bei -3dB bestimmen
        float peakVal = std::abs(correctionCurve[static_cast<size_t>(peakBinIdx)]);
        float threshold = peakVal * 0.707f;  // -3dB
        
        int leftIdx = peakBinIdx;
        int rightIdx = peakBinIdx;
        
        // Links suchen
        while (leftIdx > 0 && std::abs(correctionCurve[static_cast<size_t>(leftIdx)]) > threshold)
            --leftIdx;
        
        // Rechts suchen
        while (rightIdx < static_cast<int>(correctionCurve.size()) - 1 && 
               std::abs(correctionCurve[static_cast<size_t>(rightIdx)]) > threshold)
            ++rightIdx;
        
        // Bandbreite in Hz
        float freqLow = binFrequencies[static_cast<size_t>(leftIdx)];
        float freqHigh = binFrequencies[static_cast<size_t>(rightIdx)];
        float centerFreq = binFrequencies[static_cast<size_t>(peakBinIdx)];
        
        float bandwidth = freqHigh - freqLow;
        
        if (bandwidth > 0.0f && centerFreq > 0.0f)
        {
            // Q = fc / BW
            float q = centerFreq / bandwidth;
            return juce::jlimit(0.3f, 10.0f, q);
        }
        
        return 1.0f;  // Default Q
    }
    
    //==========================================================================
    // Member-Variablen
    //==========================================================================
    double sampleRate = 44100.0;
    int fftSize = 4096;
    int numBins = 2049;
    
    Settings settings;
    
    std::vector<float> binFrequencies;
    std::vector<float> referenceSpectrum;
    std::vector<float> inputSpectrum;
    std::vector<float> correctionCurve;
    std::vector<float> smoothingWeights;
    std::vector<MatchPoint> matchPoints;
    std::vector<float> smoothingBuffer;  // Wiederverwendbar für smoothSpectrum()
    std::vector<float> resampledSpectrum_;  // Pre-allokiert für updateInputSpectrum() (vermeidet Heap-Allok im RT-Pfad)
    
    bool hasReference = false;
    bool needsRecalculation = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralMatcher)
};
