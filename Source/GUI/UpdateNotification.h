#pragma once

#include <JuceHeader.h>
#include "../Utils/UpdateChecker.h"
#include "../Utils/VersionInfo.h"

/**
 * UpdateNotificationBanner: Dezentes Banner im Header das anzeigt, 
 * dass ein Update verfuegbar ist.
 * 
 * Klick oeffnet den Update-Dialog mit Details.
 */
class UpdateNotificationBanner : public juce::Component
{
public:
    UpdateNotificationBanner()
    {
        setInterceptsMouseClicks(true, false);
        setVisible(false);
    }
    
    void showUpdate(const UpdateChecker::UpdateInfo& info)
    {
        updateInfo = info;
        setVisible(info.updateAvailable);
        repaint();
    }
    
    void hide()
    {
        setVisible(false);
    }
    
    void paint(juce::Graphics& g) override
    {
        if (!updateInfo.updateAvailable)
            return;
        
        auto bounds = getLocalBounds().toFloat();
        
        // Hintergrund: Auffaelliges aber dezentes Gradient
        juce::Colour bannerColor(0xFF2299DD);  // Blau
        g.setColour(bannerColor.withAlpha(0.85f));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Rand
        g.setColour(bannerColor.brighter(0.3f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
        
        // Icon (Pfeil nach oben)
        auto iconArea = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);
        g.setColour(juce::Colours::white);
        
        juce::Path arrow;
        float cx = iconArea.getCentreX();
        float cy = iconArea.getCentreY();
        float s = iconArea.getWidth() * 0.3f;
        arrow.addTriangle(cx, cy - s, cx - s, cy + s * 0.3f, cx + s, cy + s * 0.3f);
        arrow.addRectangle(cx - s * 0.35f, cy + s * 0.3f, s * 0.7f, s * 0.8f);
        g.fillPath(arrow);
        
        // Text
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Update v" + updateInfo.latestVersion + " verfuegbar!",
                   bounds.reduced(4, 0), juce::Justification::centredLeft, true);
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        if (onClicked)
            onClicked();
    }
    
    void mouseEnter(const juce::MouseEvent&) override
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    
    std::function<void()> onClicked;
    
    const UpdateChecker::UpdateInfo& getUpdateInfo() const { return updateInfo; }
    
private:
    UpdateChecker::UpdateInfo updateInfo;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateNotificationBanner)
};

/**
 * UpdateDialog: Fenster mit Update-Details und Aktions-Buttons.
 */
class UpdateDialog : public juce::Component
{
public:
    UpdateDialog(const UpdateChecker::UpdateInfo& info, UpdateChecker& checker)
        : updateInfo(info), updateChecker(checker)
    {
        // Titel
        titleLabel.setText("Update verfuegbar!", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);
        
        // Version-Info
        juce::String versionText = "Aktuelle Version: " + VersionInfo::getCurrentVersion()
                                 + "\nNeue Version: " + info.latestVersion;
        if (info.message.isNotEmpty())
            versionText += "\n\n" + info.message;
        if (info.changelog.isNotEmpty())
            versionText += "\n\nChangelog:\n" + info.changelog;
        
        infoLabel.setText(versionText, juce::dontSendNotification);
        infoLabel.setFont(juce::FontOptions(13.0f));
        infoLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
        infoLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(infoLabel);
        
        // Buttons
        downloadButton.setButtonText("Download");
        downloadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2299DD));
        downloadButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        downloadButton.onClick = [this]()
        {
            if (updateInfo.downloadURL.isNotEmpty())
                juce::URL(updateInfo.downloadURL).launchInDefaultBrowser();
        };
        addAndMakeVisible(downloadButton);
        
        laterButton.setButtonText("Spaeter");
        laterButton.onClick = [this]()
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible(laterButton);
        
        skipButton.setButtonText("Version ueberspringen");
        skipButton.onClick = [this]()
        {
            updateChecker.skipVersion(updateInfo.latestVersion);
            if (onClose)
                onClose();
        };
        addAndMakeVisible(skipButton);
        
        setSize(380, 260);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Dunkler Hintergrund
        g.fillAll(juce::Colour(0xFF1A1A2E));
        
        // Rand
        g.setColour(juce::Colour(0xFF2299DD).withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 1.5f);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(15);
        
        titleLabel.setBounds(bounds.removeFromTop(28));
        bounds.removeFromTop(8);
        
        // Buttons unten
        auto buttonRow = bounds.removeFromBottom(32);
        downloadButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        laterButton.setBounds(buttonRow.removeFromLeft(80));
        buttonRow.removeFromLeft(8);
        skipButton.setBounds(buttonRow.removeFromLeft(150));
        
        bounds.removeFromBottom(10);
        infoLabel.setBounds(bounds);
    }
    
    std::function<void()> onClose;
    
private:
    UpdateChecker::UpdateInfo updateInfo;
    UpdateChecker& updateChecker;
    
    juce::Label titleLabel;
    juce::Label infoLabel;
    juce::TextButton downloadButton;
    juce::TextButton laterButton;
    juce::TextButton skipButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateDialog)
};

/**
 * UpdateDialogWindow: Wrapper um den Dialog als eigenständiges Fenster.
 */
class UpdateDialogWindow : public juce::DocumentWindow
{
public:
    UpdateDialogWindow(const UpdateChecker::UpdateInfo& info, UpdateChecker& checker)
        : juce::DocumentWindow("Aura - Update",
                               juce::Colour(0xFF1A1A2E),
                               juce::DocumentWindow::closeButton)
    {
        auto* dialog = new UpdateDialog(info, checker);
        dialog->onClose = [this]() { closeButtonPressed(); };
        
        setContentOwned(dialog, true);
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        toFront(true);
    }
    
    void closeButtonPressed() override
    {
        setVisible(false);
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateDialogWindow)
};
