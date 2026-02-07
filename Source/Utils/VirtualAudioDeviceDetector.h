#pragma once

#include <JuceHeader.h>

/**
 * VirtualAudioDeviceDetector: Erkennt virtuelle Audio-Geräte wie VB-Cable,
 * Voicemeeter, Virtual Audio Cable etc.
 * 
 * Ermöglicht das Routing von System-Audio (Browser, Media Player) als Input.
 * 
 * Unterstützte Geräte:
 * - VB-Cable (kostenlos)
 * - Voicemeeter (kostenlos)
 * - Virtual Audio Cable (kommerziell)
 * - BlackHole (macOS, aber zur Vollständigkeit)
 * - JACK Audio Connection Kit
 */
class VirtualAudioDeviceDetector
{
public:
    //==========================================================================
    // Erkanntes virtuelles Gerät
    //==========================================================================
    struct VirtualDevice
    {
        juce::String name;          // Vollständiger Gerätename
        juce::String shortName;     // Kurzname für UI
        juce::String type;          // VB-Cable, Voicemeeter, etc.
        bool isInput = false;       // Als Input verfügbar?
        bool isOutput = false;      // Als Output verfügbar?
        int deviceIndex = -1;       // Index im JUCE AudioDeviceManager
    };
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    VirtualAudioDeviceDetector() = default;
    ~VirtualAudioDeviceDetector() = default;
    
    //==========================================================================
    // Geräte scannen
    //==========================================================================
    void scanForDevices(juce::AudioDeviceManager& deviceManager)
    {
        virtualDevices.clear();
        
        // Alle verfügbaren Device-Typen durchgehen
        auto& deviceTypes = deviceManager.getAvailableDeviceTypes();
        
        for (auto* deviceType : deviceTypes)
        {
            deviceType->scanForDevices();
            
            // Input-Geräte
            auto inputNames = deviceType->getDeviceNames(true);
            for (int i = 0; i < inputNames.size(); ++i)
            {
                auto& name = inputNames[i];
                if (isVirtualDevice(name))
                {
                    addOrUpdateDevice(name, true, false, i);
                }
            }
            
            // Output-Geräte
            auto outputNames = deviceType->getDeviceNames(false);
            for (int i = 0; i < outputNames.size(); ++i)
            {
                auto& name = outputNames[i];
                if (isVirtualDevice(name))
                {
                    addOrUpdateDevice(name, false, true, i);
                }
            }
        }
        
        // Nach Typ sortieren
        std::sort(virtualDevices.begin(), virtualDevices.end(),
            [](const VirtualDevice& a, const VirtualDevice& b) {
                return a.type < b.type;
            });
    }
    
    //==========================================================================
    // Getter
    //==========================================================================
    const std::vector<VirtualDevice>& getVirtualDevices() const
    {
        return virtualDevices;
    }
    
    // Nur Input-Geräte (für System-Audio-Capture)
    std::vector<VirtualDevice> getInputDevices() const
    {
        std::vector<VirtualDevice> inputs;
        for (const auto& device : virtualDevices)
        {
            if (device.isInput)
                inputs.push_back(device);
        }
        return inputs;
    }
    
    // Nur Output-Geräte
    std::vector<VirtualDevice> getOutputDevices() const
    {
        std::vector<VirtualDevice> outputs;
        for (const auto& device : virtualDevices)
        {
            if (device.isOutput)
                outputs.push_back(device);
        }
        return outputs;
    }
    
    bool hasVirtualDevices() const
    {
        return !virtualDevices.empty();
    }
    
    bool hasVirtualInputs() const
    {
        for (const auto& device : virtualDevices)
        {
            if (device.isInput)
                return true;
        }
        return false;
    }
    
    //==========================================================================
    // Setup-Hilfe für Benutzer
    //==========================================================================
    static juce::String getSetupInstructions()
    {
        return "Um System-Audio (Browser, Musik-Player) aufzunehmen:\n\n"
               "1. Installiere VB-Cable (kostenlos): https://vb-audio.com/Cable/\n"
               "2. Setze 'CABLE Input' als Windows Standard-Ausgabegerät\n"
               "3. Wähle 'CABLE Output' als Aura Input\n"
               "4. Dein System-Audio wird jetzt durch Aura geroutet!\n\n"
               "Alternative: Voicemeeter Banana für mehr Flexibilität.";
    }
    
    static juce::URL getVBCableDownloadURL()
    {
        return juce::URL("https://vb-audio.com/Cable/");
    }
    
    static juce::URL getVoicemeeterDownloadURL()
    {
        return juce::URL("https://vb-audio.com/Voicemeeter/");
    }
    
private:
    //==========================================================================
    // Hilfsfunktionen
    //==========================================================================
    
    bool isVirtualDevice(const juce::String& name) const
    {
        juce::String lowerName = name.toLowerCase();
        
        // VB-Audio Produkte
        if (lowerName.contains("cable") && lowerName.contains("vb"))
            return true;
        if (lowerName.contains("voicemeeter"))
            return true;
        if (lowerName.contains("vb-audio"))
            return true;
            
        // Virtual Audio Cable (kommerziell)
        if (lowerName.contains("virtual audio cable"))
            return true;
        if (lowerName.contains("line 1") && lowerName.contains("virtual"))
            return true;
            
        // BlackHole (macOS)
        if (lowerName.contains("blackhole"))
            return true;
            
        // JACK
        if (lowerName.contains("jack"))
            return true;
            
        // Andere virtuelle Geräte
        if (lowerName.contains("virtual") && 
            (lowerName.contains("audio") || lowerName.contains("sound")))
            return true;
            
        // Loopback-Geräte
        if (lowerName.contains("loopback"))
            return true;
        if (lowerName.contains("stereo mix"))
            return true;
        if (lowerName.contains("what u hear"))
            return true;
            
        return false;
    }
    
    juce::String detectDeviceType(const juce::String& name) const
    {
        juce::String lowerName = name.toLowerCase();
        
        if (lowerName.contains("voicemeeter"))
        {
            if (lowerName.contains("banana"))
                return "Voicemeeter Banana";
            if (lowerName.contains("potato"))
                return "Voicemeeter Potato";
            return "Voicemeeter";
        }
        
        if (lowerName.contains("cable") && lowerName.contains("vb"))
            return "VB-Cable";
            
        if (lowerName.contains("virtual audio cable"))
            return "Virtual Audio Cable";
            
        if (lowerName.contains("blackhole"))
            return "BlackHole";
            
        if (lowerName.contains("jack"))
            return "JACK";
            
        if (lowerName.contains("stereo mix") || lowerName.contains("what u hear"))
            return "System Loopback";
            
        return "Virtual Device";
    }
    
    juce::String createShortName(const juce::String& fullName, const juce::String& type) const
    {
        // Kurznamen für bessere UI-Darstellung
        if (type == "VB-Cable")
        {
            if (fullName.containsIgnoreCase("output"))
                return "VB-Cable (Input)";  // Verwirrend aber korrekt: Output = Input für Capture
            if (fullName.containsIgnoreCase("input"))
                return "VB-Cable (Output)";
            return "VB-Cable";
        }
        
        if (type.contains("Voicemeeter"))
        {
            // Versuche Bus-Nummer zu extrahieren
            for (int i = 1; i <= 5; ++i)
            {
                if (fullName.contains("B" + juce::String(i)) || 
                    fullName.contains("Bus " + juce::String(i)))
                {
                    return type + " Bus " + juce::String(i);
                }
            }
            return type;
        }
        
        // Fallback: Ersten 20 Zeichen
        if (fullName.length() > 25)
            return fullName.substring(0, 22) + "...";
            
        return fullName;
    }
    
    void addOrUpdateDevice(const juce::String& name, bool asInput, bool asOutput, int index)
    {
        // Prüfen ob Gerät bereits existiert
        for (auto& device : virtualDevices)
        {
            if (device.name == name)
            {
                device.isInput |= asInput;
                device.isOutput |= asOutput;
                return;
            }
        }
        
        // Neues Gerät hinzufügen
        VirtualDevice device;
        device.name = name;
        device.type = detectDeviceType(name);
        device.shortName = createShortName(name, device.type);
        device.isInput = asInput;
        device.isOutput = asOutput;
        device.deviceIndex = index;
        
        virtualDevices.push_back(device);
    }
    
    std::vector<VirtualDevice> virtualDevices;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VirtualAudioDeviceDetector)
};
