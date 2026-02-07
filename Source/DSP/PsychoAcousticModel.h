#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <array>

/**
 * PsychoAcousticModel: Psychoakustische Gewichtung nach ISO 226:2003
 * 
 * Features:
 * - Equal-Loudness Kurven (Fletcher-Munson / ISO 226)
 * - A-Gewichtung für Frequenzen
 * - Auditory Masking Model
 * - Critical Band (Bark) basierte Analyse
 * - Perceptual Loudness Calculation
 */
class PsychoAcousticModel
{
public:
    //==========================================================================
    // Konstruktor
    //==========================================================================
    PsychoAcousticModel()
    {
        initializeEqualLoudnessContours();
    }
    
    void prepare(double newSampleRate, int newFftSize = 2048)
    {
        sampleRate = newSampleRate;
        fftSize = newFftSize;
        numBins = fftSize / 2 + 1;
    }
    
    //==========================================================================
    // Equal Loudness Gewichtung (ISO 226:2003)
    //==========================================================================
    
    /**
     * Berechnet den Equal-Loudness Offset für eine Frequenz bei einem Referenzpegel
     * Basiert auf ISO 226:2003
     * @param frequency Frequenz in Hz
     * @param phon Lautstärkepegel in Phon (typisch 60-80 für Musik)
     * @return Gewichtung in dB (positiv = verstärken, negativ = abschwächen)
     */
    float getEqualLoudnessWeight(float frequency, float phon = 70.0f) const
    {
        // ISO 226 Formel vereinfacht
        // L_p = (1/a_f) * [4.47e-3 * (10^(0.025*L_N) - 1.15) + (0.4 * 10^(((T_f + L_U)/10) - 9))^a_f]
        
        // Vereinfachte Approximation basierend auf typischen Equal-Loudness Kurven
        float f = juce::jlimit(20.0f, 20000.0f, frequency);
        
        // Tieffrequente Anhebung (unter 100Hz hören wir schlechter)
        float lowWeight = 0.0f;
        if (f < 100.0f)
            lowWeight = -20.0f * std::log10(f / 100.0f);
        
        // Mittenbereich-Empfindlichkeit (2-5kHz ist am empfindlichsten)
        float midWeight = 0.0f;
        float fLog = std::log10(f);
        float centerLog = std::log10(3500.0f);  // Peak bei ~3.5kHz
        float spread = 0.4f;
        midWeight = 3.0f * std::exp(-std::pow((fLog - centerLog) / spread, 2.0f));
        
        // Hochfrequente Absenkung (über 10kHz)
        float highWeight = 0.0f;
        if (f > 10000.0f)
            highWeight = -15.0f * std::log10(f / 10000.0f);
        
        // Phon-abhängige Skalierung (bei höheren Pegeln flachere Kurven)
        float phonScale = 1.0f - (phon - 40.0f) / 120.0f;
        phonScale = juce::jlimit(0.3f, 1.0f, phonScale);
        
        return (lowWeight - midWeight + highWeight) * phonScale;
    }
    
    /**
     * Wendet Equal-Loudness Gewichtung auf FFT-Magnitudes an
     */
    std::vector<float> applyEqualLoudnessWeighting(const std::vector<float>& magnitudesDb, 
                                                    float phon = 70.0f) const
    {
        std::vector<float> weighted(magnitudesDb.size());
        
        for (size_t i = 0; i < magnitudesDb.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            float weight = getEqualLoudnessWeight(freq, phon);
            weighted[i] = magnitudesDb[i] + weight;
        }
        
        return weighted;
    }
    
    //==========================================================================
    // A-Gewichtung (IEC 61672-1)
    //==========================================================================
    
    /**
     * A-Gewichtungskurve für eine Frequenz
     * @param frequency Frequenz in Hz
     * @return Gewichtung in dB
     */
    static float getAWeighting(float frequency)
    {
        float f2 = frequency * frequency;
        float f4 = f2 * f2;
        
        float numerator = 12194.0f * 12194.0f * f4;
        float denominator = (f2 + 20.6f * 20.6f) * 
                           std::sqrt((f2 + 107.7f * 107.7f) * (f2 + 737.9f * 737.9f)) *
                           (f2 + 12194.0f * 12194.0f);
        
        if (denominator < 1e-10f)
            return -100.0f;
        
        float ra = numerator / denominator;
        return 20.0f * std::log10(ra) + 2.0f;  // +2.0 dB Offset bei 1kHz
    }
    
    std::vector<float> applyAWeighting(const std::vector<float>& magnitudesDb) const
    {
        std::vector<float> weighted(magnitudesDb.size());
        
        for (size_t i = 0; i < magnitudesDb.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            if (freq > 10.0f)  // Vermeidet Division durch sehr kleine Werte
            {
                float weight = getAWeighting(freq);
                weighted[i] = magnitudesDb[i] + weight;
            }
            else
            {
                weighted[i] = magnitudesDb[i] - 50.0f;  // Stark dämpfen
            }
        }
        
        return weighted;
    }
    
    //==========================================================================
    // Auditory Masking Model
    //==========================================================================
    
    /**
     * Berechnet Maskierungsschwelle basierend auf Spektrum
     * Optimiert: Arbeitet auf 24 Bark-Bändern statt O(n²) Bin-Vergleich
     */
    std::vector<float> calculateMaskingThreshold(const std::vector<float>& magnitudesDb) const
    {
        std::vector<float> threshold(magnitudesDb.size(), -100.0f);
        
        // Schritt 1: Aggregiere Energie pro Bark-Band (24 Bänder, 0-24 Bark)
        constexpr int numBarkBands = 25;
        std::array<float, numBarkBands> barkEnergy;
        std::array<float, numBarkBands> barkCenterFreq;
        barkEnergy.fill(-100.0f);
        
        for (int b = 0; b < numBarkBands; ++b)
            barkCenterFreq[static_cast<size_t>(b)] = barkToHz(static_cast<float>(b) + 0.5f);
        
        // Bins auf Bark-Bänder zuordnen und max Energie finden
        for (size_t i = 0; i < magnitudesDb.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            int barkBand = juce::jlimit(0, numBarkBands - 1, static_cast<int>(hzToBark(freq)));
            barkEnergy[static_cast<size_t>(barkBand)] = std::max(
                barkEnergy[static_cast<size_t>(barkBand)], magnitudesDb[i]);
        }
        
        // Schritt 2: Fuer jeden Bin die Maskierung aus benachbarten Bark-Bändern berechnen
        for (size_t i = 0; i < magnitudesDb.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            float bark = hzToBark(freq);
            
            float maxMask = -100.0f;
            
            // Nur relevante Bark-Bänder pruefen (±5 Bark reicht fuer Maskierung)
            int centerBark = static_cast<int>(bark);
            int lowBark = std::max(0, centerBark - 5);
            int highBark = std::min(numBarkBands - 1, centerBark + 5);
            
            for (int b = lowBark; b <= highBark; ++b)
            {
                float maskerLevel = barkEnergy[static_cast<size_t>(b)];
                if (maskerLevel < -60.0f) continue;
                
                float dBark = bark - (static_cast<float>(b) + 0.5f);
                float spread = calculateSpreadingFunction(dBark, maskerLevel);
                float maskLevel = maskerLevel - spread;
                
                maxMask = std::max(maxMask, maskLevel);
            }
            
            // Absolute Hoerschwelle hinzufuegen
            float absThreshold = getAbsoluteThreshold(freq);
            threshold[i] = std::max(maxMask, absThreshold);
        }
        
        return threshold;
    }
    
    /**
     * Prüft ob ein Signal hörbar ist (über Maskierungsschwelle)
     */
    bool isAudible(float magnitudeDb, float maskingThresholdDb) const
    {
        return magnitudeDb > maskingThresholdDb + 3.0f;  // 3dB Sicherheitsmarge
    }
    
    //==========================================================================
    // Perceptual Loudness (vereinfachte LUFS-ähnliche Berechnung)
    //==========================================================================
    
    /**
     * Berechnet wahrgenommene Lautheit basierend auf K-Gewichtung
     * Ähnlich ITU-R BS.1770 aber vereinfacht
     */
    float calculatePerceptualLoudness(const std::vector<float>& magnitudesDb) const
    {
        float totalLoudness = 0.0f;
        
        for (size_t i = 1; i < magnitudesDb.size(); ++i)
        {
            float freq = binToFrequency(static_cast<int>(i));
            
            // K-Gewichtung (Hochpass + Hochregal)
            float kWeight = getKWeighting(freq);
            
            float weightedDb = magnitudesDb[i] + kWeight;
            
            // In lineare Leistung umrechnen und summieren
            if (weightedDb > -100.0f)
            {
                float linearPower = std::pow(10.0f, weightedDb / 10.0f);
                totalLoudness += linearPower;
            }
        }
        
        // Zurück in dB (LUFS-ähnlich)
        if (totalLoudness > 0.0f)
            return 10.0f * std::log10(totalLoudness) - 0.691f;  // -0.691 ist LUFS-Offset
        
        return -100.0f;
    }
    
    //==========================================================================
    // Problem-Gewichtung für SmartAnalyzer
    //==========================================================================
    
    /**
     * Gewichtet ein erkanntes Problem nach psychoakustischer Relevanz
     * @param frequency Frequenz des Problems
     * @param magnitude dB-Pegel
     * @param problemType Typ des Problems (für kontextuelle Gewichtung)
     * @return Multiplikator für Relevanz (0.0 - 2.0)
     */
    float getProblemRelevanceWeight(float frequency, float magnitude, int /*problemType*/ = 0) const
    {
        float weight = 1.0f;
        
        // 1. Equal-Loudness Gewichtung (Mitten sind wichtiger)
        float elWeight = -getEqualLoudnessWeight(frequency, 70.0f) / 10.0f + 1.0f;
        elWeight = juce::jlimit(0.5f, 1.5f, elWeight);
        weight *= elWeight;
        
        // 2. Magnitude-basierte Gewichtung (lautere Probleme sind relevanter)
        float magWeight = 1.0f;
        if (magnitude > -20.0f)
            magWeight = 1.2f;
        else if (magnitude < -40.0f)
            magWeight = 0.8f;
        weight *= magWeight;
        
        // 3. Kritische Frequenzbereiche (Sprache, Musikinstrumente)
        // 1-4 kHz: Sprachverständlichkeit
        if (frequency >= 1000.0f && frequency <= 4000.0f)
            weight *= 1.2f;
        
        // 200-500 Hz: Wärme/Mud - kritisch
        if (frequency >= 200.0f && frequency <= 500.0f)
            weight *= 1.1f;
        
        return juce::jlimit(0.5f, 2.0f, weight);
    }
    
    //==========================================================================
    // Critical Band (Bark) Utilities
    //==========================================================================
    
    static float hzToBark(float hz)
    {
        return 13.0f * std::atan(0.00076f * hz) + 3.5f * std::atan(std::pow(hz / 7500.0f, 2.0f));
    }
    
    static float barkToHz(float bark)
    {
        // Approximation der inversen Funktion
        return 600.0f * std::sinh(bark / 6.0f);
    }
    
    /**
     * Gibt die Breite eines kritischen Bandes bei einer Frequenz zurück
     */
    static float getCriticalBandwidth(float frequency)
    {
        return 25.0f + 75.0f * std::pow(1.0f + 1.4f * std::pow(frequency / 1000.0f, 2.0f), 0.69f);
    }

private:
    double sampleRate = 44100.0;
    int fftSize = 2048;
    int numBins = 1025;
    
    // Lookup-Tabellen für Performance
    std::array<float, 1000> equalLoudnessLUT;
    
    //==========================================================================
    // Hilfsfunktionen
    //==========================================================================
    float binToFrequency(int bin) const
    {
        return static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    }
    
    void initializeEqualLoudnessContours()
    {
        // Lookup-Tabelle für schnelleren Zugriff
        for (int i = 0; i < 1000; ++i)
        {
            float freq = 20.0f * std::pow(1000.0f, static_cast<float>(i) / 999.0f);  // 20Hz - 20kHz log
            equalLoudnessLUT[static_cast<size_t>(i)] = getEqualLoudnessWeight(freq, 70.0f);
        }
    }
    
    /**
     * K-Gewichtung für LUFS-Berechnung
     * Kombination aus Hochpass (100Hz) + Hochregal (+4dB über 1.5kHz)
     */
    static float getKWeighting(float frequency)
    {
        // Stage 1: Hochregal bei 1681 Hz
        float shelfGain = 4.0f;  // +4 dB
        float shelfFreq = 1681.0f;
        float shelf = shelfGain / (1.0f + std::pow(shelfFreq / frequency, 2.0f));
        
        // Stage 2: Hochpass bei 38 Hz
        float hpFreq = 38.0f;
        float hp = -12.0f / (1.0f + std::pow(frequency / hpFreq, 2.0f));
        
        return shelf + hp;
    }
    
    /**
     * Absolute Hörschwelle (Threshold in Quiet)
     */
    static float getAbsoluteThreshold(float frequency)
    {
        float f = frequency / 1000.0f;  // in kHz
        
        // ISO 389-7 Approximation
        return 3.64f * std::pow(f, -0.8f) 
             - 6.5f * std::exp(-0.6f * std::pow(f - 3.3f, 2.0f))
             + 0.001f * std::pow(f, 4.0f);
    }
    
    /**
     * Spreading Function für Maskierung
     * Beschreibt wie ein Ton benachbarte Frequenzen maskiert
     */
    static float calculateSpreadingFunction(float barkDistance, float maskerLevel)
    {
        // Asymmetrische Spreading Function
        float spread;
        
        if (barkDistance < 0)
        {
            // Aufwärts-Maskierung (hochfrequente Töne maskieren tiefere weniger stark)
            spread = 27.0f * std::abs(barkDistance);
        }
        else
        {
            // Abwärts-Maskierung (tieffrequente Töne maskieren höhere stärker)
            // Level-abhängig: lautere Töne haben breitere Maskierung
            float slope = 27.0f - 0.37f * std::max(0.0f, maskerLevel + 50.0f);
            slope = juce::jlimit(5.0f, 27.0f, slope);
            spread = slope * barkDistance;
        }
        
        return spread;
    }
};
