#pragma once

#include <JuceHeader.h>

/**
 * Zentrale Versionsinformationen für Aura.
 * Die Version wird aus CMake (project(Aura VERSION x.y.z)) übernommen.
 */
namespace VersionInfo
{
    // Aktuelle Plugin-Version (wird von JucePlugin_VersionString aus CMakeLists.txt gesetzt)
    static inline juce::String getCurrentVersion()
    {
        return JucePlugin_VersionString;  // z.B. "1.0.0"
    }
    
    // Update-Server URL
    static inline juce::String getUpdateURL()
    {
        // Prüfe ob eine lokale Test-Datei existiert (für Entwicklung)
        auto localTestFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                .getChildFile("Aura")
                                .getChildFile("update_test_url.txt");
        
        if (localTestFile.existsAsFile())
        {
            juce::String customURL = localTestFile.loadFileAsString().trim();
            if (customURL.isNotEmpty())
            {
                DBG("UpdateChecker: Verwende Test-URL: " + customURL);
                return customURL;
            }
        }
        
        return "https://www.unproved-audio.de/update/aura.json";
    }
    
    // Vergleicht zwei Semantic-Version Strings (z.B. "1.0.0" vs "1.1.0")
    // Gibt zurück: -1 wenn a < b, 0 wenn gleich, 1 wenn a > b
    static inline int compareVersions(const juce::String& a, const juce::String& b)
    {
        auto partsA = juce::StringArray::fromTokens(a, ".", "");
        auto partsB = juce::StringArray::fromTokens(b, ".", "");
        
        for (int i = 0; i < 3; ++i)
        {
            int va = (i < partsA.size()) ? partsA[i].getIntValue() : 0;
            int vb = (i < partsB.size()) ? partsB[i].getIntValue() : 0;
            
            if (va < vb) return -1;
            if (va > vb) return 1;
        }
        return 0;
    }
}
