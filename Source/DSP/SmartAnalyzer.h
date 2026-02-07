#pragma once

#include <JuceHeader.h>
#include "FFTAnalyzer.h"
#include "SpectralAnalysis.h"
#include "PsychoAcousticModel.h"
#include "DynamicResonanceSuppressor.h"
#include "InstrumentProfiles.h"

/**
 * SmartAnalyzer: KI-basierte Spektrum-Analyse für automatische Problemerkennung.
 * 
 * Features:
 * - Automatische Erkennung von Resonanzen, Harshness, Mud und Masking
 * - Regelbasierte Analyse (keine externe ML-Bibliothek erforderlich)
 * - Echtzeit-Empfehlungen für EQ-Einstellungen
 * - Farbcodierung nach Problemkategorie
 * - Confidence-Werte für jede Erkennung
 * - Spektralanalyse mit MFCC und Timbral Descriptors
 * - Psychoakustische Gewichtung (ISO 226 Equal-Loudness)
 * - Dynamische Resonanzunterdrückung
 * - Instrumentenprofile für kontextbezogene Analyse
 */
class SmartAnalyzer
{
public:
    //==========================================================================
    // Problem-Kategorien
    //==========================================================================
    enum class ProblemCategory
    {
        None,
        // === CUT-Kategorien (zu viel Energie) ===
        Resonance,      // Schmale Peaks, oft störend (200-500 Hz typisch)
        Harshness,      // Unangenehme hohe Frequenzen (2-5 kHz)
        Mud,            // Matschiger Bereich (100-300 Hz)
        Masking,        // Frequenz-Maskierung (überlappende Elemente)
        Boxiness,       // Box-artiger Klang (300-600 Hz)
        Sibilance,      // S-Laute (5-10 kHz)
        Rumble,         // Tieffrequentes Rumpeln (20-80 Hz)
        
        // === BOOST-Kategorien (fehlende Energie) ===
        LackOfAir,      // Fehlende Höhen/Luftigkeit (>10 kHz) -> Boost
        LackOfPresence, // Fehlende Präsenz (3-6 kHz) -> Boost
        ThinSound,      // Dünner Klang, fehlender Body (80-250 Hz) -> Boost
        LackOfClarity,  // Fehlende Klarheit/Definition (1-3 kHz) -> Boost
        LackOfWarmth    // Fehlende Wärme (200-500 Hz) -> Boost
    };
    
    // Prüft ob eine Kategorie einen Boost empfiehlt (positive Gain)
    static bool isBoostCategory(ProblemCategory category)
    {
        return category == ProblemCategory::LackOfAir ||
               category == ProblemCategory::LackOfPresence ||
               category == ProblemCategory::ThinSound ||
               category == ProblemCategory::LackOfClarity ||
               category == ProblemCategory::LackOfWarmth;
    }
    
    //==========================================================================
    // Schweregrad
    //==========================================================================
    enum class Severity
    {
        Low,            // Geringfügig - optional bearbeiten
        Medium,         // Merklich - Bearbeitung empfohlen
        High            // Stark - dringende Bearbeitung empfohlen
    };
    
    //==========================================================================
    // Erkanntes Problem
    //==========================================================================
    struct FrequencyProblem
    {
        float frequency = 0.0f;         // Zentrale Frequenz des Problems
        float bandwidth = 0.0f;         // Breite in Hz
        float magnitude = 0.0f;         // Aktuelle Magnitude in dB
        float deviation = 0.0f;         // Abweichung vom Durchschnitt in dB
        ProblemCategory category = ProblemCategory::None;
        Severity severity = Severity::Low;
        float confidence = 0.0f;        // 0.0 - 1.0 Konfidenz der Erkennung
        
        // Empfohlene EQ-Einstellungen
        float suggestedGain = 0.0f;     // Empfohlener Gain in dB
        float suggestedQ = 1.0f;        // Empfohlener Q-Faktor
        
        bool isValid() const { return frequency > 0.0f && category != ProblemCategory::None; }
    };
    
    //==========================================================================
    // Analyse-Einstellungen
    //==========================================================================
    struct Settings
    {
        // Empfindlichkeit für verschiedene Problemtypen (0.0 - 1.0)
        float resonanceSensitivity = 0.7f;
        float harshnessSensitivity = 0.6f;
        float mudSensitivity = 0.5f;
        float boxinessSensitivity = 0.5f;
        float sibilanceSensitivity = 0.6f;
        float rumbleSensitivity = 0.5f;
        
        // Minimale Abweichung für Erkennung (in dB) - niedriger = empfindlicher
        float minimumDeviation = 2.0f;
        
        // Maximale Anzahl gleichzeitiger Probleme
        int maxProblems = 12;
        
        // Analyse-Intervall (in ms)
        int analysisIntervalMs = 100;
        
        // Smoothing für stabile Erkennung
        float detectionSmoothing = 0.8f;
    };
    
    //==========================================================================
    // Farben für Kategorien
    //==========================================================================
    static juce::Colour getColourForCategory(ProblemCategory category)
    {
        switch (category)
        {
            // CUT-Kategorien (rötliche/warnende Farben)
            case ProblemCategory::Resonance:    return juce::Colour(0xffff4444);  // Rot
            case ProblemCategory::Harshness:    return juce::Colour(0xffff8800);  // Orange
            case ProblemCategory::Mud:          return juce::Colour(0xff8b4513);  // Braun
            case ProblemCategory::Masking:      return juce::Colour(0xff4488ff);  // Blau
            case ProblemCategory::Boxiness:     return juce::Colour(0xffaa6633);  // Dunkel-Orange
            case ProblemCategory::Sibilance:    return juce::Colour(0xffffff00);  // Gelb
            case ProblemCategory::Rumble:       return juce::Colour(0xff663399);  // Lila
            
            // BOOST-Kategorien (grünliche/positive Farben)
            case ProblemCategory::LackOfAir:      return juce::Colour(0xff00ddff);  // Cyan
            case ProblemCategory::LackOfPresence: return juce::Colour(0xff00ff88);  // Türkis-Grün
            case ProblemCategory::ThinSound:      return juce::Colour(0xff44cc44);  // Grün
            case ProblemCategory::LackOfClarity:  return juce::Colour(0xff88ddff);  // Hellblau
            case ProblemCategory::LackOfWarmth:   return juce::Colour(0xffffaa44);  // Warm-Orange
            
            default:                            return juce::Colours::grey;
        }
    }
    
    static juce::String getCategoryName(ProblemCategory category)
    {
        switch (category)
        {
            case ProblemCategory::Resonance:    return "Resonanz";
            case ProblemCategory::Harshness:    return "Harshness";
            case ProblemCategory::Mud:          return "Mud";
            case ProblemCategory::Masking:      return "Masking";
            case ProblemCategory::Boxiness:     return "Boxiness";
            case ProblemCategory::Sibilance:    return "Sibilance";
            case ProblemCategory::Rumble:       return "Rumble";
            
            // BOOST-Kategorien
            case ProblemCategory::LackOfAir:      return "Fehlende Luft";
            case ProblemCategory::LackOfPresence: return "Fehlende Präsenz";
            case ProblemCategory::ThinSound:      return "Dünn";
            case ProblemCategory::LackOfClarity:  return "Fehlende Klarheit";
            case ProblemCategory::LackOfWarmth:   return "Fehlende Wärme";
            
            default:                            return "Unbekannt";
        }
    }
    
    static juce::String getSeverityName(Severity severity)
    {
        switch (severity)
        {
            case Severity::Low:     return "Gering";
            case Severity::Medium:  return "Mittel";
            case Severity::High:    return "Hoch";
            default:                return "";
        }
    }
    
    //==========================================================================
    // RT-Safe Arrays: Maximale Anzahl erkannter Probleme
    //==========================================================================
    static constexpr int maxDetectedProblems = 16;

    //==========================================================================
    // Konstruktor / Destruktor
    //==========================================================================
    SmartAnalyzer();
    ~SmartAnalyzer() = default;
    
    //==========================================================================
    // Initialisierung
    //==========================================================================
    void prepare(double sampleRate);
    void reset();
    
    //==========================================================================
    // Analyse durchführen
    //==========================================================================
    void analyze(const FFTAnalyzer& fftAnalyzer);
    
    //==========================================================================
    // Ergebnisse abrufen
    // OPTIMIERUNG: Gebe const-Referenz auf Array zurück statt vector-Kopie
    //==========================================================================
    const std::array<FrequencyProblem, maxDetectedProblems>& getDetectedProblemsArray() const { return detectedProblemsArray; }
    int getDetectedProblemsCount() const { return detectedProblemsCount; }
    
    // Kompatibilitätsfunktion - erstellt temporären Vector (nicht im Audio-Thread verwenden!)
    std::vector<FrequencyProblem> getDetectedProblems() const {
        return std::vector<FrequencyProblem>(detectedProblemsArray.begin(), 
                                              detectedProblemsArray.begin() + detectedProblemsCount);
    }
    bool hasProblems() const { return detectedProblemsCount > 0; }
    int getProblemCount() const { return detectedProblemsCount; }
    
    // Probleme in einem Frequenzbereich finden
    std::vector<FrequencyProblem> getProblemsInRange(float minFreq, float maxFreq) const;
    
    // Schwerstes Problem finden
    const FrequencyProblem* getMostSevereProblem() const;
    
    //==========================================================================
    // Einstellungen
    //==========================================================================
    void setSettings(const Settings& newSettings) { settings = newSettings; }
    const Settings& getSettings() const { return settings; }
    
    void setSensitivity(float globalSensitivity);
    void setEnabled(bool enabled) { analysisEnabled = enabled; }
    bool isEnabled() const { return analysisEnabled; }
    
    //==========================================================================
    // Statistiken
    //==========================================================================
    float getAverageMagnitude() const { return averageMagnitude; }
    float getStandardDeviation() const { return standardDeviation; }
    
private:
    //==========================================================================
    // Interne Analyse-Methoden
    //==========================================================================
    void calculateStatistics(const std::vector<float>& magnitudes);
    void detectSignificantPeaks(const std::vector<float>& magnitudes);  // Einfache, garantiert funktionierende Peak-Erkennung
    void detectResonances(const std::vector<float>& magnitudes);
    void detectHarshness(const std::vector<float>& magnitudes);
    void detectMud(const std::vector<float>& magnitudes);
    void detectBoxiness(const std::vector<float>& magnitudes);
    void detectSibilance(const std::vector<float>& magnitudes);
    void detectRumble(const std::vector<float>& magnitudes);
    
    // BOOST-Erkennung: Fehlende Frequenzbereiche
    void detectLackOfAir(const std::vector<float>& magnitudes);
    void detectLackOfPresence(const std::vector<float>& magnitudes);
    void detectThinSound(const std::vector<float>& magnitudes);
    void detectLackOfClarity(const std::vector<float>& magnitudes);
    void detectLackOfWarmth(const std::vector<float>& magnitudes);
    
    void consolidateProblems();
    void smoothDetections();
    
    // OPTIMIERUNG: Hilfsfunktion zum sicheren Hinzufügen von Problemen (RT-safe)
    inline void addProblem(const FrequencyProblem& problem)
    {
        if (detectedProblemsCount < maxDetectedProblems)
        {
            detectedProblemsArray[detectedProblemsCount++] = problem;
        }
    }
    
    float calculateBandwidth(const std::vector<float>& magnitudes, int peakIndex, float threshold);
    float suggestGainReduction(float deviation, Severity severity);
    float suggestQFactor(float bandwidth, float frequency);
    Severity calculateSeverity(float deviation, float sensitivity);
    
    int frequencyToBin(float frequency) const;
    float binToFrequency(int bin) const;
    
    // Erweiterte Analyse mit neuen Modulen
    void analyzeWithSpectralFeatures(const std::vector<float>& magnitudes);
    void applyPsychoAcousticWeighting();
    void detectWithInstrumentProfile();
    
    //==========================================================================
    // Member
    //==========================================================================
    Settings settings;
    bool analysisEnabled = true;
    
    double sampleRate = 44100.0;
    int fftSize = 2048;
    int numBins = 1025;
    
    // OPTIMIERUNG: Fixed-Size Arrays statt std::vector (RT-safe)
    std::array<FrequencyProblem, maxDetectedProblems> detectedProblemsArray;
    std::array<FrequencyProblem, maxDetectedProblems> previousProblemsArray;
    int detectedProblemsCount = 0;
    int previousProblemsCount = 0;
    
    // Statistiken
    float averageMagnitude = -60.0f;
    float standardDeviation = 10.0f;
    std::vector<float> bandAverages;  // Durchschnitt pro Frequenzband (nur 8 Einträge, OK)
    
    // Frequenzbänder für Band-spezifische Analyse
    struct FrequencyBand
    {
        float minFreq;
        float maxFreq;
        ProblemCategory associatedProblem;
        juce::String name;
    };
    
    std::vector<FrequencyBand> frequencyBands;
    
    // Timing (RT-safe: Sample-Counter statt Systemzeit)
    int samplesSinceLastAnalysis = 0;
    
    //==========================================================================
    // Erweiterte DSP-Module
    //==========================================================================
    SpectralAnalysis spectralAnalysis;
    PsychoAcousticModel psychoModel;
    DynamicResonanceSuppressor resonanceSuppressor;
    InstrumentProfiles instrumentProfiles;
    
    // Aktuelles Instrumentenprofil
    juce::String currentProfileName = "Default";
    InstrumentProfiles::Profile currentProfile;
    
    // Spektralmetriken (gecached)
    SpectralAnalysis::SpectralMetrics cachedMetrics;
    
    // Psychoakustisch gewichtete Magnitudes
    std::vector<float> weightedMagnitudes;
    
public:
    //==========================================================================
    // Erweiterte API für neue Features
    //==========================================================================
    
    // Instrumentenprofil setzen
    void setInstrumentProfile(const juce::String& profileName)
    {
        currentProfileName = profileName;
        currentProfile = instrumentProfiles.getProfile(profileName);
        // Übernehme Profile-Settings
        settings.resonanceSensitivity = currentProfile.analysis.resonanceSensitivity;
        settings.harshnessSensitivity = currentProfile.analysis.harshnessSensitivity;
        settings.mudSensitivity = currentProfile.analysis.mudSensitivity;
        settings.boxinessSensitivity = currentProfile.analysis.boxinessSensitivity;
    }
    
    const juce::String& getCurrentProfileName() const { return currentProfileName; }
    const InstrumentProfiles::Profile& getCurrentProfile() const { return currentProfile; }
    
    // Verfügbare Profile abrufen
    std::vector<juce::String> getAvailableProfiles() const { return instrumentProfiles.getProfileNames(); }
    std::vector<juce::String> getCategories() const { return instrumentProfiles.getCategories(); }
    
    // Spektralmetriken abrufen
    const SpectralAnalysis::SpectralMetrics& getSpectralMetrics() const { return cachedMetrics; }
    
    // Psychoakustische Gewichtung aktivieren/deaktivieren
    void setUsePsychoAcousticWeighting(bool use) { usePsychoAcousticWeighting = use; }
    bool getUsePsychoAcousticWeighting() const { return usePsychoAcousticWeighting; }
    
    // Dynamische Resonanzunterdrückung konfigurieren
    DynamicResonanceSuppressor& getResonanceSuppressor() { return resonanceSuppressor; }
    
    // Profilbasierte Empfehlungstexte
    juce::String getProfileTip() const
    {
        return currentProfile.tips.empty() ? juce::String() : currentProfile.tips[0];
    }
    
    std::vector<juce::String> getAllProfileTips() const
    {
        return currentProfile.tips;
    }
    
private:
    bool usePsychoAcousticWeighting = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmartAnalyzer)
};
