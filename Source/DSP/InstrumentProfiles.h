#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

/**
 * InstrumentProfiles: Vordefinierte EQ-Profile für verschiedene Instrumente/Genres
 * 
 * Features:
 * - Instrument-spezifische Analyse-Schwellwerte
 * - Zielkurven für verschiedene Genres
 * - Empfohlene Frequenzbereiche pro Instrument
 * - Automatische Profil-Erkennung (optional)
 */
class InstrumentProfiles
{
public:
    //==========================================================================
    // Profil-Struktur
    //==========================================================================
    struct Profile
    {
        juce::String name;
        juce::String category;  // "Vocals", "Drums", "Bass", "Keys", "Guitar", "Mix", "Master"
        juce::String description;
        
        // Analyse-Einstellungen
        struct AnalysisSettings
        {
            float sensitivity = 1.0f;           // Globale Empfindlichkeit (0.5 - 2.0)
            float resonanceSensitivity = 1.0f;  // Resonanz-Erkennung
            float harshnessSensitivity = 1.0f;  // Harshness-Erkennung
            float mudSensitivity = 1.0f;        // Mud-Erkennung
            float boxinessSensitivity = 1.0f;   // Boxiness-Erkennung
            
            // Frequenzbereich für Analyse
            float lowFreq = 20.0f;
            float highFreq = 20000.0f;
        } analysis;
        
        // Zielkurve (Frequenz -> dB Offset)
        struct TargetCurve
        {
            float sub = 0.0f;       // 20-60 Hz
            float bass = 0.0f;      // 60-200 Hz
            float lowMid = 0.0f;    // 200-500 Hz
            float mid = 0.0f;       // 500-2000 Hz
            float highMid = 0.0f;   // 2-5 kHz
            float presence = 0.0f;  // 5-10 kHz
            float air = 0.0f;       // 10-20 kHz
        } targetCurve;
        
        // Kritische Frequenzbereiche für dieses Instrument
        struct CriticalBands
        {
            float mudLow = 200.0f;
            float mudHigh = 400.0f;
            float boxLow = 300.0f;
            float boxHigh = 800.0f;
            float harshLow = 2000.0f;
            float harshHigh = 5000.0f;
            float sibilanceLow = 5000.0f;
            float sibilanceHigh = 10000.0f;
        } criticalBands;
        
        // Empfohlene EQ-Tipps
        std::vector<juce::String> tips;
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    InstrumentProfiles()
    {
        initializeProfiles();
    }
    
    //==========================================================================
    // Profil-Zugriff
    //==========================================================================
    
    const Profile& getProfile(const juce::String& name) const
    {
        auto it = profiles.find(name);
        if (it != profiles.end())
            return it->second;
        return profiles.at("Default");
    }
    
    std::vector<juce::String> getProfileNames() const
    {
        std::vector<juce::String> names;
        for (const auto& [name, profile] : profiles)
            names.push_back(name);
        return names;
    }
    
    std::vector<juce::String> getProfilesByCategory(const juce::String& category) const
    {
        std::vector<juce::String> names;
        for (const auto& [name, profile] : profiles)
        {
            if (profile.category == category)
                names.push_back(name);
        }
        return names;
    }
    
    std::vector<juce::String> getCategories() const
    {
        return { "Vocals", "Drums", "Bass", "Keys", "Guitar", "Mix", "Master" };
    }
    
    //==========================================================================
    // Zielkurve als Frequenz-Array
    //==========================================================================
    
    std::vector<std::pair<float, float>> getTargetCurvePoints(const Profile& profile) const
    {
        return {
            { 30.0f, profile.targetCurve.sub },
            { 100.0f, profile.targetCurve.bass },
            { 350.0f, profile.targetCurve.lowMid },
            { 1000.0f, profile.targetCurve.mid },
            { 3500.0f, profile.targetCurve.highMid },
            { 7000.0f, profile.targetCurve.presence },
            { 15000.0f, profile.targetCurve.air }
        };
    }
    
    /**
     * Interpoliert Zielkurve für beliebige Frequenz
     */
    float getTargetLevel(const Profile& profile, float frequency) const
    {
        auto points = getTargetCurvePoints(profile);
        
        // Unterhalb erstem Punkt
        if (frequency <= points.front().first)
            return points.front().second;
        
        // Oberhalb letztem Punkt
        if (frequency >= points.back().first)
            return points.back().second;
        
        // Interpolieren
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            if (frequency >= points[i].first && frequency < points[i + 1].first)
            {
                float t = (std::log10(frequency) - std::log10(points[i].first)) /
                         (std::log10(points[i + 1].first) - std::log10(points[i].first));
                return points[i].second + t * (points[i + 1].second - points[i].second);
            }
        }
        
        return 0.0f;
    }
    
private:
    std::map<juce::String, Profile> profiles;
    
    void initializeProfiles()
    {
        // ========== DEFAULT ==========
        {
            Profile p;
            p.name = "Default";
            p.category = "Mix";
            p.description = "Neutrale Einstellungen für alle Quellen";
            profiles["Default"] = p;
        }
        
        // ========== VOCALS ==========
        {
            Profile p;
            p.name = "Lead Vocals";
            p.category = "Vocals";
            p.description = "Optimiert für Hauptstimme - Klarheit und Präsenz";
            
            p.analysis.sensitivity = 1.2f;
            p.analysis.harshnessSensitivity = 1.3f;  // Empfindlicher für Sibilanz
            p.analysis.mudSensitivity = 1.2f;
            p.analysis.lowFreq = 80.0f;
            p.analysis.highFreq = 16000.0f;
            
            p.targetCurve.sub = -6.0f;      // Sub reduzieren
            p.targetCurve.bass = -2.0f;     // Etwas Bass
            p.targetCurve.lowMid = -1.0f;   // Mud-Region leicht reduziert
            p.targetCurve.mid = 0.0f;       // Neutral
            p.targetCurve.highMid = 2.0f;   // Präsenz anheben
            p.targetCurve.presence = 1.0f;  // Brillanz
            p.targetCurve.air = 1.5f;       // Air
            
            p.criticalBands.mudLow = 200.0f;
            p.criticalBands.mudHigh = 350.0f;
            p.criticalBands.harshLow = 2500.0f;
            p.criticalBands.harshHigh = 4500.0f;
            p.criticalBands.sibilanceLow = 5000.0f;
            p.criticalBands.sibilanceHigh = 9000.0f;
            
            p.tips = {
                "High-Pass bei 80-100 Hz",
                "Mud bei 200-350 Hz prüfen",
                "Präsenz bei 3-5 kHz anheben",
                "De-Esser bei 5-8 kHz erwägen"
            };
            
            profiles["Lead Vocals"] = p;
        }
        
        {
            Profile p;
            p.name = "Background Vocals";
            p.category = "Vocals";
            p.description = "Für Backing Vocals - dezenter, weniger präsent";
            
            p.analysis.sensitivity = 1.0f;
            p.analysis.harshnessSensitivity = 1.5f;
            
            p.targetCurve.sub = -12.0f;
            p.targetCurve.bass = -4.0f;
            p.targetCurve.lowMid = -2.0f;
            p.targetCurve.mid = -1.0f;
            p.targetCurve.highMid = 0.0f;
            p.targetCurve.presence = 0.0f;
            p.targetCurve.air = 1.0f;
            
            profiles["Background Vocals"] = p;
        }
        
        // ========== DRUMS ==========
        {
            Profile p;
            p.name = "Kick Drum";
            p.category = "Drums";
            p.description = "Kick Drum - Punch und Sub";
            
            p.analysis.sensitivity = 0.9f;
            p.analysis.mudSensitivity = 1.4f;
            p.analysis.boxinessSensitivity = 1.3f;
            p.analysis.lowFreq = 30.0f;
            p.analysis.highFreq = 10000.0f;
            
            p.targetCurve.sub = 3.0f;       // Sub-Bass betonen
            p.targetCurve.bass = 2.0f;      // Punch
            p.targetCurve.lowMid = -3.0f;   // Boxiness reduzieren
            p.targetCurve.mid = -2.0f;      // Mitten zurück
            p.targetCurve.highMid = 1.0f;   // Click/Attack
            p.targetCurve.presence = 0.0f;
            p.targetCurve.air = -3.0f;
            
            p.criticalBands.mudLow = 250.0f;
            p.criticalBands.mudHigh = 400.0f;
            p.criticalBands.boxLow = 400.0f;
            p.criticalBands.boxHigh = 700.0f;
            
            p.tips = {
                "Sub-Boost bei 50-60 Hz",
                "Boxiness bei 300-500 Hz reduzieren",
                "Click/Attack bei 2.5-4 kHz"
            };
            
            profiles["Kick Drum"] = p;
        }
        
        {
            Profile p;
            p.name = "Snare Drum";
            p.category = "Drums";
            p.description = "Snare - Body und Crack";
            
            p.analysis.sensitivity = 1.1f;
            p.analysis.boxinessSensitivity = 1.2f;
            p.analysis.harshnessSensitivity = 1.1f;
            
            p.targetCurve.sub = -6.0f;
            p.targetCurve.bass = 1.0f;      // Body
            p.targetCurve.lowMid = 0.0f;
            p.targetCurve.mid = 2.0f;       // Snare-Körper
            p.targetCurve.highMid = 1.5f;   // Crack
            p.targetCurve.presence = 0.0f;
            p.targetCurve.air = -2.0f;
            
            p.criticalBands.boxLow = 400.0f;
            p.criticalBands.boxHigh = 600.0f;
            
            profiles["Snare Drum"] = p;
        }
        
        {
            Profile p;
            p.name = "Hi-Hat / Cymbals";
            p.category = "Drums";
            p.description = "Hi-Hat und Becken - Kontrolle der Höhen";
            
            p.analysis.sensitivity = 1.3f;
            p.analysis.harshnessSensitivity = 1.5f;
            p.analysis.lowFreq = 200.0f;
            p.analysis.highFreq = 20000.0f;
            
            p.targetCurve.sub = -12.0f;
            p.targetCurve.bass = -6.0f;
            p.targetCurve.lowMid = -4.0f;
            p.targetCurve.mid = -2.0f;
            p.targetCurve.highMid = 0.0f;
            p.targetCurve.presence = 1.0f;
            p.targetCurve.air = 2.0f;
            
            p.criticalBands.harshLow = 3000.0f;
            p.criticalBands.harshHigh = 6000.0f;
            
            profiles["Hi-Hat / Cymbals"] = p;
        }
        
        // ========== BASS ==========
        {
            Profile p;
            p.name = "Electric Bass";
            p.category = "Bass";
            p.description = "E-Bass - Tiefe und Definition";
            
            p.analysis.sensitivity = 1.0f;
            p.analysis.mudSensitivity = 1.3f;
            p.analysis.lowFreq = 30.0f;
            p.analysis.highFreq = 8000.0f;
            
            p.targetCurve.sub = 2.0f;
            p.targetCurve.bass = 1.0f;
            p.targetCurve.lowMid = -2.0f;
            p.targetCurve.mid = 0.0f;
            p.targetCurve.highMid = 1.0f;   // Definition
            p.targetCurve.presence = -2.0f;
            p.targetCurve.air = -6.0f;
            
            p.criticalBands.mudLow = 200.0f;
            p.criticalBands.mudHigh = 350.0f;
            
            p.tips = {
                "Fundamentale bei 60-80 Hz",
                "Mud bei 200-300 Hz prüfen",
                "Definition bei 700-1000 Hz"
            };
            
            profiles["Electric Bass"] = p;
        }
        
        {
            Profile p;
            p.name = "Synth Bass";
            p.category = "Bass";
            p.description = "Synthesizer Bass - Sub und Punch";
            
            p.analysis.sensitivity = 0.9f;
            p.analysis.resonanceSensitivity = 1.4f;  // Synths haben oft Resonanzen
            
            p.targetCurve.sub = 4.0f;
            p.targetCurve.bass = 1.0f;
            p.targetCurve.lowMid = -3.0f;
            p.targetCurve.mid = -1.0f;
            p.targetCurve.highMid = 1.0f;
            p.targetCurve.presence = -2.0f;
            p.targetCurve.air = -6.0f;
            
            profiles["Synth Bass"] = p;
        }
        
        // ========== GUITAR ==========
        {
            Profile p;
            p.name = "Acoustic Guitar";
            p.category = "Guitar";
            p.description = "Akustikgitarre - natürlich und luftig";
            
            p.analysis.sensitivity = 1.1f;
            p.analysis.boxinessSensitivity = 1.3f;
            
            p.targetCurve.sub = -6.0f;
            p.targetCurve.bass = 0.0f;
            p.targetCurve.lowMid = -1.0f;
            p.targetCurve.mid = 0.0f;
            p.targetCurve.highMid = 1.0f;
            p.targetCurve.presence = 2.0f;
            p.targetCurve.air = 2.0f;
            
            p.criticalBands.boxLow = 200.0f;
            p.criticalBands.boxHigh = 400.0f;
            
            profiles["Acoustic Guitar"] = p;
        }
        
        {
            Profile p;
            p.name = "Electric Guitar (Clean)";
            p.category = "Guitar";
            p.description = "Clean E-Gitarre";
            
            p.analysis.sensitivity = 1.0f;
            
            p.targetCurve.sub = -12.0f;
            p.targetCurve.bass = -2.0f;
            p.targetCurve.lowMid = 0.0f;
            p.targetCurve.mid = 1.0f;
            p.targetCurve.highMid = 1.5f;
            p.targetCurve.presence = 1.0f;
            p.targetCurve.air = 0.0f;
            
            profiles["Electric Guitar (Clean)"] = p;
        }
        
        {
            Profile p;
            p.name = "Electric Guitar (Distorted)";
            p.category = "Guitar";
            p.description = "Verzerrte E-Gitarre - Dichte Mitten";
            
            p.analysis.sensitivity = 1.2f;
            p.analysis.harshnessSensitivity = 1.4f;
            p.analysis.resonanceSensitivity = 1.3f;
            
            p.targetCurve.sub = -12.0f;
            p.targetCurve.bass = -2.0f;
            p.targetCurve.lowMid = 1.0f;
            p.targetCurve.mid = 2.0f;
            p.targetCurve.highMid = 0.0f;
            p.targetCurve.presence = -1.0f;  // Fizz reduzieren
            p.targetCurve.air = -3.0f;
            
            p.criticalBands.harshLow = 2500.0f;
            p.criticalBands.harshHigh = 4000.0f;
            
            p.tips = {
                "Fizz bei 3-5 kHz reduzieren",
                "Low-Cut bei 80-100 Hz",
                "Resonanzen im Amp-Bereich prüfen"
            };
            
            profiles["Electric Guitar (Distorted)"] = p;
        }
        
        // ========== KEYS ==========
        {
            Profile p;
            p.name = "Piano";
            p.category = "Keys";
            p.description = "Akustisches Klavier - voller Frequenzbereich";
            
            p.analysis.sensitivity = 1.0f;
            p.analysis.resonanceSensitivity = 1.2f;
            
            p.targetCurve.sub = 0.0f;
            p.targetCurve.bass = 0.0f;
            p.targetCurve.lowMid = -1.0f;
            p.targetCurve.mid = 0.0f;
            p.targetCurve.highMid = 1.0f;
            p.targetCurve.presence = 1.0f;
            p.targetCurve.air = 0.5f;
            
            profiles["Piano"] = p;
        }
        
        {
            Profile p;
            p.name = "Synth Pad";
            p.category = "Keys";
            p.description = "Synthesizer Pad - weich und breit";
            
            p.analysis.sensitivity = 0.8f;
            p.analysis.resonanceSensitivity = 1.5f;
            
            p.targetCurve.sub = 0.0f;
            p.targetCurve.bass = 0.0f;
            p.targetCurve.lowMid = -2.0f;
            p.targetCurve.mid = -1.0f;
            p.targetCurve.highMid = 0.0f;
            p.targetCurve.presence = 1.0f;
            p.targetCurve.air = 2.0f;
            
            profiles["Synth Pad"] = p;
        }
        
        // ========== MIX ==========
        {
            Profile p;
            p.name = "Mix Bus";
            p.category = "Mix";
            p.description = "Stereo-Mix - subtile Korrekturen";
            
            p.analysis.sensitivity = 0.7f;  // Weniger aggressiv
            p.analysis.resonanceSensitivity = 0.8f;
            p.analysis.harshnessSensitivity = 0.9f;
            
            p.targetCurve.sub = 0.0f;
            p.targetCurve.bass = 0.0f;
            p.targetCurve.lowMid = 0.0f;
            p.targetCurve.mid = 0.0f;
            p.targetCurve.highMid = 0.0f;
            p.targetCurve.presence = 0.0f;
            p.targetCurve.air = 0.0f;
            
            p.tips = {
                "Subtile Korrekturen (max ±2 dB)",
                "Auf Resonanzen achten, nicht überkorrigieren",
                "A/B Vergleich nutzen"
            };
            
            profiles["Mix Bus"] = p;
        }
        
        // ========== MASTER ==========
        {
            Profile p;
            p.name = "Mastering";
            p.category = "Master";
            p.description = "Mastering - sehr subtile Eingriffe";
            
            p.analysis.sensitivity = 0.5f;  // Sehr konservativ
            p.analysis.resonanceSensitivity = 0.6f;
            p.analysis.harshnessSensitivity = 0.7f;
            p.analysis.mudSensitivity = 0.6f;
            
            p.targetCurve.sub = 0.0f;
            p.targetCurve.bass = 0.0f;
            p.targetCurve.lowMid = 0.0f;
            p.targetCurve.mid = 0.0f;
            p.targetCurve.highMid = 0.0f;
            p.targetCurve.presence = 0.0f;
            p.targetCurve.air = 0.0f;
            
            p.tips = {
                "Maximal ±1.5 dB Korrektur",
                "Linear Phase für Mastering",
                "Referenz-Track zum Vergleich nutzen"
            };
            
            profiles["Mastering"] = p;
        }
    }
};
