#pragma once

#include <JuceHeader.h>
#include <set>
#include "../DSP/SmartAnalyzer.h"
#include "../DSP/EQProcessor.h"
#include "../Parameters/ParameterIDs.h"

/**
 * SmartEQRecommendation: Verwaltet EQ-Empfehlungen basierend auf SmartAnalyzer-Ergebnissen.
 * 
 * Features:
 * - Konvertiert erkannte Probleme in konkrete EQ-Einstellungen
 * - One-Click-Anwendung von Empfehlungen
 * - Filtertyp-Auswahl basierend auf Problemkategorie
 * - Undo-Unterstützung
 */
class SmartEQRecommendation
{
public:
    //==========================================================================
    // Empfehlungs-Struktur
    //==========================================================================
    struct Recommendation
    {
        int bandIndex = -1;                     // -1 = neues Band benötigt
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 1.0f;
        int filterType = 0;                     // BiquadFilter::Type
        SmartAnalyzer::ProblemCategory sourceCategory = SmartAnalyzer::ProblemCategory::None;
        SmartAnalyzer::Severity severity = SmartAnalyzer::Severity::Low;
        float confidence = 0.0f;
        bool applied = false;
        
        juce::String getDescription() const
        {
            juce::String desc;
            desc << SmartAnalyzer::getCategoryName(sourceCategory) << " bei ";
            
            if (frequency >= 1000.0f)
                desc << juce::String(frequency / 1000.0f, 1) << " kHz";
            else
                desc << juce::String(static_cast<int>(frequency)) << " Hz";
            
            desc << " (" << SmartAnalyzer::getSeverityName(severity) << ")";
            return desc;
        }
    };
    
    SmartEQRecommendation() = default;
    ~SmartEQRecommendation() = default;
    
    //==========================================================================
    // Empfehlungen generieren
    //==========================================================================
    void updateRecommendations(const SmartAnalyzer& analyzer, const EQProcessor& processor)
    {
        recommendations.clear();
        
        const auto& problems = analyzer.getDetectedProblems();
        
        for (const auto& problem : problems)
        {
            if (!problem.isValid())
                continue;
            
            Recommendation rec;
            rec.frequency = problem.frequency;
            rec.gain = problem.suggestedGain;
            rec.q = problem.suggestedQ;
            rec.filterType = getFilterTypeForCategory(problem.category, problem.frequency);
            rec.sourceCategory = problem.category;
            rec.severity = problem.severity;
            rec.confidence = problem.confidence;
            rec.bandIndex = findBestBandForRecommendation(processor, rec);
            
            recommendations.push_back(rec);
        }
    }
    
    //==========================================================================
    // Empfehlung anwenden
    //==========================================================================
    bool applyRecommendation(int index, EQProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    {
        if (index < 0 || index >= static_cast<int>(recommendations.size()))
            return false;
        
        auto& rec = recommendations[static_cast<size_t>(index)];
        
        // Freies Band finden falls nötig
        int bandToUse = rec.bandIndex;
        if (bandToUse < 0)
        {
            bandToUse = findInactiveBand(processor);
            if (bandToUse < 0)
                return false;  // Kein freies Band verfügbar
        }
        
        // Parameter setzen (IDs: bandN_freq, bandN_gain, bandN_q, bandN_type, bandN_active)
        if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(bandToUse)))
            param->setValueNotifyingHost(1.0f);
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(bandToUse)))
        {
            auto range = param->getNormalisableRange();
            float normalizedFreq = range.convertTo0to1(rec.frequency);
            param->setValueNotifyingHost(normalizedFreq);
        }
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(bandToUse)))
        {
            auto range = param->getNormalisableRange();
            float normalizedGain = range.convertTo0to1(rec.gain);
            param->setValueNotifyingHost(normalizedGain);
        }
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(bandToUse)))
        {
            auto range = param->getNormalisableRange();
            float normalizedQ = range.convertTo0to1(rec.q);
            param->setValueNotifyingHost(normalizedQ);
        }
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(bandToUse)))
        {
            auto range = param->getNormalisableRange();
            float normalizedType = range.convertTo0to1(static_cast<float>(rec.filterType));
            param->setValueNotifyingHost(normalizedType);
        }
        
        rec.applied = true;
        rec.bandIndex = bandToUse;
        
        return true;
    }
    
    //==========================================================================
    // Alle Empfehlungen anwenden
    //==========================================================================
    int applyAllRecommendations(EQProcessor& processor, juce::AudioProcessorValueTreeState& apvts)
    {
        int appliedCount = 0;
        std::set<int> usedBands;  // Welche Bänder wurden bereits verwendet
        
        // Erst alle bereits verwendeten Bänder sammeln
        for (const auto& rec : recommendations)
        {
            if (rec.applied && rec.bandIndex >= 0)
                usedBands.insert(rec.bandIndex);
        }
        
        // Alle aktiven Bänder auch als "verwendet" markieren
        const int numBands = ParameterIDs::MAX_BANDS;
        for (int i = 0; i < numBands; ++i)
        {
            if (processor.getBand(i).isActive())
                usedBands.insert(i);
        }
        
        for (size_t i = 0; i < recommendations.size(); ++i)
        {
            if (recommendations[i].applied)
                continue;
            
            // Freies Band finden das noch nicht verwendet wurde
            int bandToUse = -1;
            for (int b = 0; b < numBands; ++b)
            {
                if (usedBands.find(b) == usedBands.end())
                {
                    bandToUse = b;
                    break;
                }
            }
            
            if (bandToUse < 0)
                break;  // Keine freien Bänder mehr
            
            // Band als verwendet markieren
            usedBands.insert(bandToUse);
            recommendations[i].bandIndex = bandToUse;
            
            if (applyRecommendation(static_cast<int>(i), processor, apvts))
                ++appliedCount;
        }
        
        return appliedCount;
    }
    
    //==========================================================================
    // Zugriff auf Empfehlungen
    //==========================================================================
    const std::vector<Recommendation>& getRecommendations() const { return recommendations; }
    int getRecommendationCount() const { return static_cast<int>(recommendations.size()); }
    bool hasRecommendations() const { return !recommendations.empty(); }
    
    const Recommendation* getRecommendation(int index) const
    {
        if (index < 0 || index >= static_cast<int>(recommendations.size()))
            return nullptr;
        return &recommendations[static_cast<size_t>(index)];
    }
    
    void clearRecommendations() { recommendations.clear(); }
    
private:
    std::vector<Recommendation> recommendations;
    
    //==========================================================================
    // Filtertyp basierend auf Problemkategorie
    //==========================================================================
    int getFilterTypeForCategory(SmartAnalyzer::ProblemCategory category, float frequency)
    {
        // Filtertypen entsprechend BiquadFilter::Type
        // 0=Bell, 1=LowShelf, 2=HighShelf, 3=LowCut, 4=HighCut, 5=Notch, 6=BandPass, 7=TiltShelf
        
        switch (category)
        {
            // === CUT-Kategorien ===
            case SmartAnalyzer::ProblemCategory::Resonance:
                return 0;  // Bell fuer praezise Cuts
                
            case SmartAnalyzer::ProblemCategory::Harshness:
                return 0;  // Bell mit breitem Q
                
            case SmartAnalyzer::ProblemCategory::Mud:
                return frequency < 150.0f ? 1 : 0;  // LowShelf fuer tiefe Frequenzen, sonst Bell
                
            case SmartAnalyzer::ProblemCategory::Boxiness:
                return 0;  // Bell
                
            case SmartAnalyzer::ProblemCategory::Sibilance:
                return 2;  // HighShelf fuer De-Essing
                
            case SmartAnalyzer::ProblemCategory::Rumble:
                return 3;  // LowCut (High-Pass)
                
            case SmartAnalyzer::ProblemCategory::Masking:
                return 0;  // Bell
                
            // === BOOST-Kategorien ===
            case SmartAnalyzer::ProblemCategory::LackOfAir:
                return 2;  // HighShelf - Luftigkeit >10kHz boosten
                
            case SmartAnalyzer::ProblemCategory::LackOfPresence:
                return 0;  // Bell - gezielte Praesenz 3-6kHz
                
            case SmartAnalyzer::ProblemCategory::ThinSound:
                return frequency < 120.0f ? 1 : 0;  // LowShelf fuer Bass-Body, Bell fuer oberen Body
                
            case SmartAnalyzer::ProblemCategory::LackOfClarity:
                return 0;  // Bell - Klarheit 1-3kHz
                
            case SmartAnalyzer::ProblemCategory::LackOfWarmth:
                return 1;  // LowShelf - Waerme 200-500Hz
                
            default:
                return 0;
        }
    }
    
    //==========================================================================
    // Bestes Band für Empfehlung finden
    //==========================================================================
    int findBestBandForRecommendation(const EQProcessor& processor, const Recommendation& rec)
    {
        // Erst prüfen ob ein inaktives Band in der Nähe der Frequenz ist
        const int numBands = ParameterIDs::MAX_BANDS;
        float closestDistance = std::numeric_limits<float>::max();
        int closestBand = -1;
        
        for (int i = 0; i < numBands; ++i)
        {
            const auto& band = processor.getBand(i);
            if (!band.isActive())
            {
                float distance = std::abs(std::log2(band.getFrequency() / rec.frequency));
                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    closestBand = i;
                }
            }
        }
        
        return closestBand;
    }
    
    //==========================================================================
    // Inaktives Band finden
    //==========================================================================
    int findInactiveBand(const EQProcessor& processor)
    {
        const int numBands = ParameterIDs::MAX_BANDS;
        
        for (int i = 0; i < numBands; ++i)
        {
            const auto& band = processor.getBand(i);
            if (!band.isActive())
                return i;
        }
        
        return -1;  // Kein freies Band
    }
};
