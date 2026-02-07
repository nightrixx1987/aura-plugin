#pragma once

#include <JuceHeader.h>
#include "../Utils/VirtualAudioDeviceDetector.h"
#include "../Utils/WASAPILoopbackCapture.h"

/**
 * AudioSourceSelector: UI-Komponente für Audio-Input-Auswahl im Standalone-Modus.
 * 
 * Features:
 * - Liste aller verfügbaren Audio-Inputs
 * - Automatische Erkennung virtueller Geräte (VB-Cable, Voicemeeter)
 * - NEU: Native WASAPI Loopback Capture (ohne externe Software!)
 * - Virtuelle Geräte werden markiert für System-Audio-Capture
 * - Refresh-Button für Geräte-Neusuche
 * - Setup-Hilfe wenn keine virtuellen Geräte gefunden
 */
class AudioSourceSelector : public juce::Component,
                             public juce::ComboBox::Listener
{
public:
    //==========================================================================
    // Callback für Änderungen
    //==========================================================================
    std::function<void(const juce::String& deviceName)> onDeviceSelected;
    std::function<void(bool useNativeCapture)> onNativeCaptureChanged;
    
    // Referenz zu SystemAudioCapture (wird vom PluginProcessor übergeben)
    SystemAudioCapture* systemAudioCapture = nullptr;
    
    //==========================================================================
    // Konstruktor
    //==========================================================================
    AudioSourceSelector(juce::AudioDeviceManager& deviceMgr)
        : deviceManager(deviceMgr)
    {
        // Titel
        titleLabel.setText("Audio-Quelle", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);
        
        // Input-Auswahl
        inputCombo.addListener(this);
        addAndMakeVisible(inputCombo);
        
        // Refresh-Button
        refreshButton.setButtonText(juce::CharPointer_UTF8("\xe2\x9f\xb3"));  // ⟳ Unicode
        refreshButton.onClick = [this]() { refreshDevices(); };
        refreshButton.setTooltip("Geräte neu scannen");
        addAndMakeVisible(refreshButton);
        
        // Hilfe-Button
        helpButton.setButtonText("?");
        helpButton.onClick = [this]() { showSetupHelp(); };
        helpButton.setTooltip("Setup-Hilfe für System-Audio");
        addAndMakeVisible(helpButton);
        
        // NEU: Native WASAPI Loopback Toggle
        nativeCaptureButton.setButtonText("[*] System Audio (Native)");
        nativeCaptureButton.setClickingTogglesState(true);
        nativeCaptureButton.setTooltip("Aktiviert native Windows-System-Audio Capture via WASAPI Loopback.\nKein VB-Cable oder Voicemeeter nötig!");
        nativeCaptureButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2a4e));
        nativeCaptureButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00aa88));
        nativeCaptureButton.onClick = [this]()
        {
            bool enabled = nativeCaptureButton.getToggleState();
            
            if (enabled && systemAudioCapture != nullptr)
            {
                // Native Capture aktivieren
                if (systemAudioCapture->startCapture())
                {
                    updateNativeCaptureStatus(true);
                    // ComboBox deaktivieren wenn Native aktiv
                    inputCombo.setEnabled(false);
                }
                else
                {
                    // Fehler beim Starten
                    nativeCaptureButton.setToggleState(false, juce::dontSendNotification);
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "System Audio Capture",
                        "Konnte WASAPI Loopback nicht starten.\n\nStellen Sie sicher, dass Audio-Ausgabe aktiv ist.",
                        "OK");
                }
            }
            else if (!enabled && systemAudioCapture != nullptr)
            {
                // Native Capture deaktivieren
                systemAudioCapture->stopCapture();
                updateNativeCaptureStatus(false);
                inputCombo.setEnabled(true);
            }
            
            if (onNativeCaptureChanged)
                onNativeCaptureChanged(enabled);
        };
        addAndMakeVisible(nativeCaptureButton);
        
        // Status-Label
        statusLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(statusLabel);
        
        // Virtuelle Geräte Info
        virtualDeviceInfo.setFont(juce::Font(juce::FontOptions(10.0f).withStyle("Italic")));
        virtualDeviceInfo.setColour(juce::Label::textColourId, juce::Colour(0xff00ff88));
        virtualDeviceInfo.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(virtualDeviceInfo);
        
        // Initiales Scannen
        refreshDevices();
    }
    
    ~AudioSourceSelector() override = default;
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Hintergrund
        g.setColour(juce::Colour(0xff1a1a2e));
        g.fillRoundedRectangle(bounds, 6.0f);
        
        // Rand
        g.setColour(juce::Colour(0xff3a3a5e));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 8);
        
        // Titel-Zeile
        auto titleRow = bounds.removeFromTop(20);
        titleLabel.setBounds(titleRow);
        
        bounds.removeFromTop(5);
        
        // Input-Zeile
        auto inputRow = bounds.removeFromTop(25);
        refreshButton.setBounds(inputRow.removeFromRight(25));
        helpButton.setBounds(inputRow.removeFromRight(25).reduced(2, 0));
        inputCombo.setBounds(inputRow.reduced(0, 2));
        
        bounds.removeFromTop(5);
        
        // NEU: Native Capture Button (volle Breite)
        auto nativeRow = bounds.removeFromTop(26);
        nativeCaptureButton.setBounds(nativeRow.reduced(0, 1));
        
        bounds.removeFromTop(3);
        
        // Status
        statusLabel.setBounds(bounds.removeFromTop(15));
        virtualDeviceInfo.setBounds(bounds.removeFromTop(15));
    }
    
    void comboBoxChanged(juce::ComboBox* combo) override
    {
        if (combo == &inputCombo)
        {
            int selectedId = inputCombo.getSelectedId();
            if (selectedId > 0 && onDeviceSelected)
            {
                juce::String deviceName = inputCombo.getText();
                onDeviceSelected(deviceName);
                
                // Prüfen ob virtuelles Gerät
                updateStatusForDevice(deviceName);
            }
        }
    }
    
    //==========================================================================
    // Geräte-Aktualisierung
    //==========================================================================
    void refreshDevices()
    {
        // Virtuelle Geräte scannen
        virtualDetector.scanForDevices(deviceManager);
        
        // ComboBox aktualisieren
        inputCombo.clear();
        
        int itemId = 1;
        
        // Zuerst: Standard-Input-Geräte (nicht-virtuell)
        inputCombo.addSectionHeading("Standard-Eingänge");
        
        auto* currentType = deviceManager.getCurrentDeviceTypeObject();
        if (currentType != nullptr)
        {
            auto inputNames = currentType->getDeviceNames(true);
            
            for (auto& name : inputNames)
            {
                if (!isVirtualDeviceName(name))
                {
                    inputCombo.addItem(name, itemId++);
                }
            }
        }
        
        // Dann: Virtuelle Geräte
        auto virtualInputs = virtualDetector.getInputDevices();
        if (!virtualInputs.empty())
        {
            inputCombo.addSeparator();
            inputCombo.addSectionHeading("System-Audio (Virtuelle Geraete)");
            
            for (const auto& device : virtualInputs)
            {
                juce::String displayName = juce::String("* ") + device.shortName;
                inputCombo.addItem(displayName, itemId++);
            }
        }
        
        // Status aktualisieren
        updateStatus();
        
        // Aktuelles Gerät auswählen (falls verfügbar)
        auto* currentDevice = deviceManager.getCurrentAudioDevice();
        if (currentDevice != nullptr)
        {
            auto currentInputs = currentDevice->getActiveInputChannels();
            // TODO: Aktuelles Gerät in ComboBox auswählen
        }
        
        if (inputCombo.getNumItems() > 0)
        {
            inputCombo.setSelectedId(1, juce::dontSendNotification);
        }
    }
    
    //==========================================================================
    // Info über aktuelle Auswahl
    //==========================================================================
    bool isVirtualDeviceSelected() const
    {
        // Native Capture hat Priorität
        if (nativeCaptureButton.getToggleState())
            return true;
            
        juce::String selected = inputCombo.getText();
        return selected.startsWith("* ") || 
               isVirtualDeviceName(selected);
    }
    
    bool isNativeCaptureActive() const
    {
        return nativeCaptureButton.getToggleState() && 
               systemAudioCapture != nullptr && 
               systemAudioCapture->isCapturing();
    }
    
    juce::String getSelectedDeviceName() const
    {
        juce::String text = inputCombo.getText();
        // Marker entfernen
        if (text.startsWith("* "))
            text = text.substring(2);
        return text;
    }
    
private:
    void updateStatus()
    {
        if (virtualDetector.hasVirtualInputs())
        {
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff88ff88));
            statusLabel.setText("[OK] Virtuelle Geraete gefunden", 
                              juce::dontSendNotification);
            
            auto inputs = virtualDetector.getInputDevices();
            juce::String types;
            for (const auto& device : inputs)
            {
                if (types.isNotEmpty()) types += ", ";
                types += device.type;
            }
            virtualDeviceInfo.setText("Verfügbar: " + types, juce::dontSendNotification);
        }
        else
        {
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffaa00));
            statusLabel.setText("Keine virtuellen Geräte gefunden", juce::dontSendNotification);
            virtualDeviceInfo.setText("Klicke '?' für Setup-Hilfe", juce::dontSendNotification);
        }
    }
    
    void updateStatusForDevice(const juce::String& deviceName)
    {
        if (isVirtualDeviceName(deviceName) || deviceName.startsWith("* "))
        {
            virtualDeviceInfo.setText("[*] System-Audio wird verwendet", 
                                     juce::dontSendNotification);
        }
        else
        {
            virtualDeviceInfo.setText("Standard-Audio-Eingang", juce::dontSendNotification);
        }
    }
    
    bool isVirtualDeviceName(const juce::String& name) const
    {
        juce::String lower = name.toLowerCase();
        return lower.contains("cable") || 
               lower.contains("voicemeeter") ||
               lower.contains("virtual") ||
               lower.contains("loopback");
    }
    
    void showSetupHelp()
    {
        juce::String message = 
            "=== System-Audio Capture Optionen ===\n\n"
            "1. NATIVE (Empfohlen)\n"
            "   Klicke auf '[*] System Audio (Native)'\n"
            "   Verwendet Windows WASAPI Loopback direkt.\n"
            "   Keine externe Software noetig!\n\n"
            "2. Virtuelle Audio-Geraete\n"
            "   - VB-Audio Cable (kostenlos)\n"
            "   - Voicemeeter\n"
            "   Wenn du diese bereits hast, erscheinen sie in der Dropdown.\n\n"
            + VirtualAudioDeviceDetector::getSetupInstructions();
        
        auto options = juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::InfoIcon)
            .withTitle("System-Audio Setup")
            .withMessage(message)
            .withButton("OK")
            .withButton("VB-Cable herunterladen");
            
        juce::AlertWindow::showAsync(options, [](int result) {
            if (result == 1)  // Zweiter Button
            {
                VirtualAudioDeviceDetector::getVBCableDownloadURL().launchInDefaultBrowser();
            }
        });
    }
    
    void updateNativeCaptureStatus(bool active)
    {
        if (active)
        {
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ff88));
            statusLabel.setText("[OK] Native System-Audio Capture aktiv", 
                              juce::dontSendNotification);
            virtualDeviceInfo.setText("[*] WASAPI Loopback laeuft", 
                                     juce::dontSendNotification);
        }
        else
        {
            updateStatus();
        }
    }
    
    // Members
    juce::AudioDeviceManager& deviceManager;
    VirtualAudioDeviceDetector virtualDetector;
    
    juce::Label titleLabel;
    juce::ComboBox inputCombo;
    juce::TextButton refreshButton;
    juce::TextButton helpButton;
    juce::ToggleButton nativeCaptureButton;  // NEU: Native WASAPI Loopback
    juce::Label statusLabel;
    juce::Label virtualDeviceInfo;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSourceSelector)
};
