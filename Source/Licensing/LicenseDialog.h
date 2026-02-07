#pragma once

#include <JuceHeader.h>
#include "LicenseManager.h"
#include "../GUI/CustomLookAndFeel.h"

/**
 * LicenseDialog v3: Modales Lizenz-Fenster mit Online-Aktivierung
 *
 * - Zeigt Machine-ID prominent an
 * - Online-Aktivierung: Key wird beim Server validiert
 * - Deaktivierung: Seat wird freigegeben (Rechnerwechsel)
 * - Offline-Status-Anzeige mit Grace-Period Info
 * - Loading-Spinner waehrend Server-Kommunikation
 * - Farbcodiert: Gruen=Licensed, Gelb=Trial, Rot=Expired
 */
class LicenseDialog : public juce::Component,
                      private juce::Timer
{
public:
    std::function<void()> onClose;
    std::function<void()> onLicenseActivated;

    LicenseDialog()
    {
        // === Titel ===
        titleLabel.setText("Aura - Lizenzierung", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(juce::FontOptions(22.0f).withStyle("Bold")));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        // === Status-Label ===
        statusLabel.setFont(juce::Font(juce::FontOptions(15.0f)));
        statusLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(statusLabel);

        // === Machine-ID Bereich ===
        machineIDTitleLabel.setText("Ihre Machine-ID:", juce::dontSendNotification);
        machineIDTitleLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        machineIDTitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
        machineIDTitleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(machineIDTitleLabel);

        juce::String machineID = LicenseManager::getInstance().getMachineID();
        machineIDLabel.setText(machineID, juce::dontSendNotification);
        machineIDLabel.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
        machineIDLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ccff));
        machineIDLabel.setJustificationType(juce::Justification::centred);
        machineIDLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1e1e2e));
        machineIDLabel.setColour(juce::Label::outlineColourId, juce::Colour(0xff00ccff).withAlpha(0.3f));
        addAndMakeVisible(machineIDLabel);

        // Kopieren-Button
        copyMachineIDButton.setButtonText("Kopieren");
        copyMachineIDButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3a50));
        copyMachineIDButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff00ccff));
        copyMachineIDButton.onClick = [this, machineID]()
        {
            juce::SystemClipboard::copyTextToClipboard(machineID);
            copyMachineIDButton.setButtonText("Kopiert!");
            juce::Timer::callAfterDelay(1500, [this]() {
                if (this != nullptr)
                    copyMachineIDButton.setButtonText("Kopieren");
            });
        };
        addAndMakeVisible(copyMachineIDButton);

        // === Info-Text ===
        infoLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
        infoLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(infoLabel);

        // === Key-Eingabefeld ===
        keyInput.setMultiLine(false);
        keyInput.setReturnKeyStartsNewLine(false);
        keyInput.setFont(juce::Font(juce::FontOptions(16.0f)));
        keyInput.setTextToShowWhenEmpty("AURA-XXXX-XXXX-XXXXXXXX", juce::Colour(0xff505050));
        keyInput.setJustification(juce::Justification::centred);
        keyInput.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff252525));
        keyInput.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe0e0e0));
        keyInput.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff404040));
        keyInput.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff00aaff));
        keyInput.setInputRestrictions(30, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
        addAndMakeVisible(keyInput);

        // === Aktivieren-Button ===
        activateButton.setButtonText("Lizenz aktivieren");
        activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff00aa55));
        activateButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        activateButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        activateButton.onClick = [this]() { attemptOnlineActivation(); };
        addAndMakeVisible(activateButton);

        // === Deaktivieren-Button ===
        deactivateButton.setButtonText("Lizenz deaktivieren");
        deactivateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff884422));
        deactivateButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        deactivateButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        deactivateButton.onClick = [this]() { attemptDeactivation(); };
        addAndMakeVisible(deactivateButton);

        // === Schliessen-Button ===
        closeButton.setButtonText("Schliessen");
        closeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
        closeButton.onClick = [this]()
        {
            if (onClose)
                onClose();
        };
        addAndMakeVisible(closeButton);

        // === Feedback-Label ===
        feedbackLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        feedbackLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(feedbackLabel);

        // === Loading-Label (als Spinner-Ersatz) ===
        loadingLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
        loadingLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00aaff));
        loadingLabel.setJustificationType(juce::Justification::centred);
        loadingLabel.setVisible(false);
        addAndMakeVisible(loadingLabel);

        // === Online-Status Label ===
        onlineStatusLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        onlineStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff808080));
        onlineStatusLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(onlineStatusLabel);

        // === Webseite-Link ===
        websiteButton.setButtonText("www.unproved-audio.de");
        websiteButton.setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xff5599ff));
        websiteButton.setURL(juce::URL("https://www.unproved-audio.de"));
        addAndMakeVisible(websiteButton);

        updateStatusDisplay();
        setSize(500, 520);
    }

    void paint(juce::Graphics& g) override
    {
        // Hintergrund
        g.fillAll(juce::Colour(0xff1a1a1a));

        // Rahmen
        g.setColour(juce::Colour(0xff333333));
        g.drawRect(getLocalBounds(), 1);

        // Trennlinie unter Titel
        g.setColour(juce::Colour(0xff333333));
        g.fillRect(20, 52, getWidth() - 40, 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);

        titleLabel.setBounds(bounds.removeFromTop(40));
        bounds.removeFromTop(12);

        statusLabel.setBounds(bounds.removeFromTop(26));
        bounds.removeFromTop(8);

        // Online-Status (letzte Pruefung etc.)
        onlineStatusLabel.setBounds(bounds.removeFromTop(16));
        bounds.removeFromTop(8);

        // Machine-ID Bereich
        machineIDTitleLabel.setBounds(bounds.removeFromTop(18));
        bounds.removeFromTop(4);

        auto machineRow = bounds.removeFromTop(34);
        auto machineIDArea = machineRow.reduced(60, 0);
        auto copyArea = machineIDArea.removeFromRight(70);
        machineIDLabel.setBounds(machineIDArea);
        copyMachineIDButton.setBounds(copyArea.reduced(2, 2));
        bounds.removeFromTop(10);

        // Info-Text
        infoLabel.setBounds(bounds.removeFromTop(36));
        bounds.removeFromTop(10);

        // Key-Eingabe
        keyInput.setBounds(bounds.removeFromTop(35).reduced(30, 0));
        bounds.removeFromTop(8);

        // Feedback / Loading
        feedbackLabel.setBounds(bounds.removeFromTop(22));
        loadingLabel.setBounds(feedbackLabel.getBounds());
        bounds.removeFromTop(10);

        // Buttons: Aktivieren | Deaktivieren | Schliessen
        auto buttonArea = bounds.removeFromTop(36);
        int btnWidth = buttonArea.getWidth() / 3;
        activateButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(8, 0));
        deactivateButton.setBounds(buttonArea.removeFromLeft(btnWidth).reduced(8, 0));
        closeButton.setBounds(buttonArea.reduced(8, 0));

        bounds.removeFromTop(10);
        websiteButton.setBounds(bounds.removeFromTop(22).reduced(120, 0));
    }

    void updateStatusDisplay()
    {
        auto& lm = LicenseManager::getInstance();
        auto status = lm.getLicenseStatus();

        switch (status)
        {
            case LicenseManager::LicenseStatus::Licensed:
            {
                statusLabel.setText("Status: Lizenziert", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00cc66));
                infoLabel.setText("Vielen Dank fuer den Kauf von Aura!\nAlle Funktionen sind freigeschaltet.",
                                  juce::dontSendNotification);
                keyInput.setEnabled(false);
                keyInput.setText(lm.getLicenseKey(), false);
                keyInput.setAlpha(0.5f);
                activateButton.setEnabled(false);
                activateButton.setAlpha(0.4f);
                deactivateButton.setEnabled(true);
                deactivateButton.setAlpha(1.0f);
                deactivateButton.setVisible(true);

                // Online-Status Info
                if (lm.isOnlineActivated())
                {
                    int daysSince = lm.getDaysSinceLastOnlineCheck();
                    int graceDays = lm.getOfflineGraceDaysRemaining();
                    if (daysSince < 9999)
                    {
                        onlineStatusLabel.setText(
                            juce::String::formatted("Letzter Online-Check: vor %d Tag%s | Offline noch %d Tage",
                                                    daysSince, daysSince == 1 ? "" : "en", graceDays),
                            juce::dontSendNotification);
                    }
                    else
                    {
                        onlineStatusLabel.setText("Online-Aktivierung (kein Re-Check erfolgt)",
                                                  juce::dontSendNotification);
                    }
                }
                else
                {
                    onlineStatusLabel.setText("Offline-Lizenz (Legacy)", juce::dontSendNotification);
                    deactivateButton.setVisible(false);
                }
                break;
            }

            case LicenseManager::LicenseStatus::Trial:
            {
                int days = lm.getTrialDaysRemaining();
                statusLabel.setText(juce::String::formatted("Testversion: %d Tag%s verbleibend",
                                    days, days == 1 ? "" : "e"), juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffaa00));
                infoLabel.setText("Geben Sie Ihren Lizenz-Key ein und klicken Sie\n\"Lizenz aktivieren\" (Internetverbindung erforderlich).",
                                  juce::dontSendNotification);
                keyInput.setEnabled(true);
                keyInput.setAlpha(1.0f);
                activateButton.setEnabled(true);
                activateButton.setAlpha(1.0f);
                deactivateButton.setVisible(false);
                onlineStatusLabel.setText("", juce::dontSendNotification);
                break;
            }

            case LicenseManager::LicenseStatus::TrialExpired:
                statusLabel.setText("Testversion abgelaufen!", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff4444));
                infoLabel.setText("Bitte geben Sie einen gueltigen Lizenz-Key ein.\nOhne Lizenz wird das Audio-Signal eingeschraenkt.",
                                  juce::dontSendNotification);
                keyInput.setEnabled(true);
                keyInput.setAlpha(1.0f);
                activateButton.setEnabled(true);
                activateButton.setAlpha(1.0f);
                deactivateButton.setVisible(false);
                onlineStatusLabel.setText("", juce::dontSendNotification);
                break;

            case LicenseManager::LicenseStatus::Unlicensed:
            default:
            {
                // Pruefe ob es eine abgelaufene Online-Grace-Period ist
                if (lm.isOnlineActivated() && lm.getOfflineGraceDaysRemaining() <= 0)
                {
                    statusLabel.setText("Offline-Zeitraum abgelaufen!", juce::dontSendNotification);
                    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff8800));
                    infoLabel.setText("Bitte einmal mit dem Internet verbinden,\ndamit die Lizenz erneut geprueft werden kann.",
                                      juce::dontSendNotification);
                    onlineStatusLabel.setText("Lizenz vorhanden - Online-Re-Check erforderlich",
                                              juce::dontSendNotification);
                }
                else
                {
                    statusLabel.setText("Nicht lizenziert", juce::dontSendNotification);
                    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff4444));
                    infoLabel.setText("Bitte geben Sie einen gueltigen Lizenz-Key ein.",
                                      juce::dontSendNotification);
                    onlineStatusLabel.setText("", juce::dontSendNotification);
                }
                keyInput.setEnabled(true);
                keyInput.setAlpha(1.0f);
                activateButton.setEnabled(true);
                activateButton.setAlpha(1.0f);
                deactivateButton.setVisible(false);
                break;
            }
        }

        feedbackLabel.setText("", juce::dontSendNotification);
    }

private:
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label machineIDTitleLabel;
    juce::Label machineIDLabel;
    juce::TextButton copyMachineIDButton;
    juce::Label infoLabel;
    juce::TextEditor keyInput;
    juce::TextButton activateButton;
    juce::TextButton deactivateButton;
    juce::TextButton closeButton;
    juce::Label feedbackLabel;
    juce::Label loadingLabel;
    juce::Label onlineStatusLabel;
    juce::HyperlinkButton websiteButton;

    int loadingDots = 0;

    // === Timer fuer Loading-Animation ===
    void timerCallback() override
    {
        loadingDots = (loadingDots + 1) % 4;
        juce::String dots;
        for (int i = 0; i < loadingDots; ++i)
            dots += ".";
        loadingLabel.setText("Verbinde mit Lizenz-Server" + dots, juce::dontSendNotification);
    }

    void showLoading(bool show)
    {
        loadingLabel.setVisible(show);
        feedbackLabel.setVisible(!show);
        activateButton.setEnabled(!show);
        keyInput.setEnabled(!show);

        if (show)
        {
            loadingDots = 0;
            loadingLabel.setText("Verbinde mit Lizenz-Server...", juce::dontSendNotification);
            startTimer(400);
        }
        else
        {
            stopTimer();
        }
    }

    // === Online-Aktivierung ===
    void attemptOnlineActivation()
    {
        juce::String key = keyInput.getText().trim().toUpperCase();

        if (key.isEmpty())
        {
            feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6644));
            feedbackLabel.setText("Bitte einen Lizenz-Key eingeben.", juce::dontSendNotification);
            return;
        }

        showLoading(true);

        auto& lm = LicenseManager::getInstance();
        lm.activateOnline(key, [this](bool success, const juce::String& message)
        {
            showLoading(false);

            if (success)
            {
                feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00cc66));
                feedbackLabel.setText("Lizenz erfolgreich aktiviert!", juce::dontSendNotification);
                updateStatusDisplay();

                if (onLicenseActivated)
                    onLicenseActivated();
            }
            else
            {
                feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff4444));
                feedbackLabel.setText(message, juce::dontSendNotification);

                // Shake-Effekt
                shakeInput();
            }
        });
    }

    // === Deaktivierung ===
    void attemptDeactivation()
    {
        // Sicherheitsabfrage
        bool result = juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "Lizenz deaktivieren",
            "Moechten Sie die Lizenz auf diesem Rechner wirklich deaktivieren?\n\n"
            "Der Aktivierungs-Slot wird freigegeben und kann auf\n"
            "einem anderen Rechner verwendet werden.",
            "Ja, deaktivieren",
            "Abbrechen",
            nullptr,
            nullptr);

        if (!result) return;

        showLoading(true);
        loadingLabel.setText("Deaktiviere Lizenz...", juce::dontSendNotification);

        auto& lm = LicenseManager::getInstance();
        lm.deactivateOnline([this](bool success, const juce::String& message)
        {
            showLoading(false);

            if (success)
            {
                feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffaa00));
                feedbackLabel.setText("Lizenz deaktiviert. Slot freigegeben.", juce::dontSendNotification);
                keyInput.clear();
                updateStatusDisplay();
            }
            else
            {
                feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff4444));
                feedbackLabel.setText(message, juce::dontSendNotification);
            }
        });
    }

    void shakeInput()
    {
        auto originalBounds = keyInput.getBounds();
        auto& animator = juce::Desktop::getInstance().getAnimator();
        auto shifted = originalBounds.translated(6, 0);
        animator.animateComponent(&keyInput, shifted, 1.0f, 60, false, 1.0, 0.0);

        juce::Timer::callAfterDelay(80, [this, originalBounds]() {
            auto& anim = juce::Desktop::getInstance().getAnimator();
            anim.animateComponent(&keyInput, originalBounds, 1.0f, 60, false, 1.0, 0.0);
        });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseDialog)
};

/**
 * LicenseDialogWindow: DocumentWindow-Wrapper fuer modalen Dialog
 */
class LicenseDialogWindow : public juce::DocumentWindow
{
public:
    LicenseDialogWindow()
        : juce::DocumentWindow("Aura - Lizenz",
                               juce::Colour(0xff1a1a1a),
                               juce::DocumentWindow::closeButton)
    {
        dialog = std::make_unique<LicenseDialog>();
        dialog->onClose = [this]() { closeWindow(); };
        dialog->onLicenseActivated = [this]()
        {
            if (onLicenseActivated)
                onLicenseActivated();
        };

        setContentOwned(dialog.release(), true);
        setUsingNativeTitleBar(true);
        centreWithSize(500, 520);
        setResizable(false, false);
        setVisible(true);
        toFront(true);
    }

    void closeButtonPressed() override
    {
        closeWindow();
    }

    std::function<void()> onLicenseActivated;
    std::function<void()> onDialogClosed;

private:
    std::unique_ptr<LicenseDialog> dialog;

    void closeWindow()
    {
        setVisible(false);
        if (onDialogClosed)
            onDialogClosed();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseDialogWindow)
};
