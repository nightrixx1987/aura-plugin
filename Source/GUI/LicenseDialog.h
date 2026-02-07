#pragma once

#include <JuceHeader.h>
#include "../Licensing/LicenseManager.h"

/**
 * LicenseDialog: UI für Lizenz-Aktivierung
 */
class LicenseDialog : public juce::DialogWindow
{
public:
    LicenseDialog();
    
    void closeButtonPressed() override;
    
private:
    class LicensePanel : public juce::Component
    {
    public:
        LicensePanel();
        
        void paint(juce::Graphics& g) override;
        void resized() override;
        
    private:
        juce::Label titleLabel;
        juce::Label statusLabel;
        juce::TextEditor licenseKeyEditor;
        juce::TextButton activateButton;
        juce::TextButton closeButton;
        juce::Label infoLabel;
        
        void updateStatusLabel();
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicensePanel)
    };
    
    std::unique_ptr<LicensePanel> panel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseDialog)
};
