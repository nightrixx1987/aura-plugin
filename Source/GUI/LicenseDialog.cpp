#include "LicenseDialog.h"

LicenseDialog::LicensePanel::LicensePanel()
{
    // Titel
    titleLabel.setText("Aura Lizenzaktivierung", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);
    
    // Status-Label
    statusLabel.setJustificationType(juce::Justification::centred);
    updateStatusLabel();
    addAndMakeVisible(statusLabel);
    
    // Info-Text
    infoLabel.setText(
        "Geben Sie Ihren Lizenz-Key ein oder nutzen Sie die 30-Tage Trial-Periode.",
        juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setFont(12.0f);
    addAndMakeVisible(infoLabel);
    
    // Lizenz-Key Editor
    licenseKeyEditor.setMultiLine(false, false);
    licenseKeyEditor.setTextToShowWhenEmpty("AURA-XXXX-XXXX-XXXX", juce::Colours::grey);
    licenseKeyEditor.setReturnKeyStartsNewLine(false);
    licenseKeyEditor.setFont(14.0f);
    addAndMakeVisible(licenseKeyEditor);
    
    // Activate Button
    activateButton.setButtonText("Aktivieren");
    activateButton.onClick = [this]()
    {
        juce::String key = licenseKeyEditor.getText();
        
        if (LicenseManager::getInstance().activateLicense(key))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Erfolg",
                "Lizenz erfolgreich aktiviert!",
                "OK");
            updateStatusLabel();
            licenseKeyEditor.clear();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Fehler",
                "Ungültiger Lizenz-Key. Bitte überprüfen Sie die Eingabe.",
                "OK");
        }
    };
    addAndMakeVisible(activateButton);
    
    // Close Button
    closeButton.setButtonText("Schließen");
    closeButton.onClick = [this]()
    {
        if (auto* window = getParentComponent())
        {
            if (auto* dialog = dynamic_cast<juce::DialogWindow*>(window))
            {
                dialog->closeButtonPressed();
            }
        }
    };
    addAndMakeVisible(closeButton);
}

void LicenseDialog::LicensePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a)); // Dunkler Hintergrund
    
    // Rahmen
    g.setColour(juce::Colour(0xff00ffff).withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void LicenseDialog::LicensePanel::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    
    // Layout vertikal
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);
    
    statusLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(15);
    
    infoLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(15);
    
    licenseKeyEditor.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(15);
    
    auto buttonArea = bounds.removeFromTop(40);
    activateButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2).reduced(5));
    closeButton.setBounds(buttonArea.reduced(5));
}

void LicenseDialog::LicensePanel::updateStatusLabel()
{
    auto& licenseManager = LicenseManager::getInstance();
    statusLabel.setText(licenseManager.getStatusText(), juce::dontSendNotification);
    
    // Farbe basierend auf Status
    auto status = licenseManager.getLicenseStatus();
    if (status == LicenseManager::LicenseStatus::Licensed)
    {
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ff88));
    }
    else if (status == LicenseManager::LicenseStatus::TrialExpired)
    {
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff4444));
    }
    else
    {
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffff99));
    }
}

LicenseDialog::LicenseDialog()
    : juce::DialogWindow("Aura Lizenzaktivierung", juce::Colours::black, true, true)
{
    panel = std::make_unique<LicensePanel>();
    setContentOwned(panel.release(), true);
    
    setSize(500, 350);
    setResizable(false, false);
    
    // Zentriere das Fenster
    centreWithSize(500, 350);
}

void LicenseDialog::closeButtonPressed()
{
    exitModalState(0);
}
