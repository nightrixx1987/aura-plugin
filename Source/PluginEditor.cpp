#include "PluginProcessor.h"
#include "PluginEditor.h"

AuraAudioProcessorEditor::AuraAudioProcessorEditor(AuraAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      updateTimer(*this),
      spectrumGrabTool(audioProcessor.getEQProcessor())
{
    // GPU-beschleunigtes Rendering aktivieren (beschleunigt Spektrum-Darstellung erheblich)
    // Graceful Fallback: Falls OpenGL nicht verfügbar, Software-Rendering verwenden
    try
    {
        openGLContext.setComponentPaintingEnabled(true);
        openGLContext.setContinuousRepainting(false);
        openGLContext.attachTo(*this);
    }
    catch (...)
    {
        // OpenGL nicht verfügbar - Software-Rendering wird automatisch genutzt
        DBG("OpenGL nicht verfügbar - verwende Software-Rendering");
    }
    
    // LookAndFeel setzen
    setLookAndFeel(&customLookAndFeel);
    
    // Fenstergröße ZUERST, damit resized() ein gültiges Bounds hat
    // Standard-Größe: 1250x650 (breiter für Smart EQ Panel)
    int savedWidth = 1400;
    int savedHeight = 770;
    
    // Versuche, gespeicherte Größe zu laden
    if (!loadWindowSize(savedWidth, savedHeight))
    {
        // Falls Laden fehlschlägt, nutze Standard
        savedWidth = 1400;
        savedHeight = 770;
    }
    
    setSize(savedWidth, savedHeight);
    
    try
    {
        // Spektrum-Analyzer
        spectrumAnalyzer.setAnalyzer(&audioProcessor.getPreAnalyzer(), &audioProcessor.getPostAnalyzer());
        // Obere Frequenzgrenze an Samplerate anpassen (max 20kHz, aber nie über Nyquist)
        float nyquist = static_cast<float>(audioProcessor.getSampleRate()) * 0.5f;
        float displayMax = std::min(20000.0f, nyquist * 0.95f); // 95% Nyquist vermeidet Aliasing-Artefakte am Rand
        spectrumAnalyzer.setFrequencyRange(20.0f, displayMax);
        addAndMakeVisible(spectrumAnalyzer);
        
        // EQ-Kurve
        eqCurve.setEQProcessor(&audioProcessor.getEQProcessor());
        eqCurve.addListener(this);
        eqCurve.setSketchTool(&audioProcessor.getEQSketchTool());
        addAndMakeVisible(eqCurve);
        
        // Grab Tool
        addAndMakeVisible(spectrumGrabTool);
        spectrumGrabTool.setVisible(false);
        
        // Level Meter (rechte Seite) - Pro-Q4 Style
        addAndMakeVisible(levelMeter);
        
        // Theme Selector (oben rechts)
        addAndMakeVisible(themeSelector);
        
        // Lizenz-Button (neben Theme Selector)
        licenseButton.setButtonText("Lizenz");
        licenseButton.setTooltip("Lizenz verwalten\nZeigt den aktuellen Lizenzstatus an und ermoeglicht\ndie Eingabe eines Lizenzschluessels zur Aktivierung der Vollversion.");
        licenseButton.onClick = [this]()
        {
            showLicenseDialog();
        };
        addAndMakeVisible(licenseButton);
        
        // NEU: Einstellungen-Button
        settingsButton.setButtonText("Settings");
        settingsButton.setTooltip("Einstellungen\nAudio/MIDI-Einstellungen, Darstellung und Plugin-Defaults.");
        settingsButton.onClick = [this]()
        {
            AuraSettingsPanel::showAsDialog();
        };
        addAndMakeVisible(settingsButton);
        
        // NEU: System Audio Button (nur Standalone)
        systemAudioButton.setButtonText("Sys Audio");
        systemAudioButton.setClickingTogglesState(true);
        systemAudioButton.setTooltip("System Audio Capture (WASAPI Loopback)\nNimmt den gesamten Windows Audio Output auf und zeigt ihn im Analyzer an.\nSo kannst du z.B. Spotify oder YouTube analysieren und EQ-Einstellungen vornehmen.\n\nWaehle ein Ausgabegeraet um das bearbeitete Signal zu hoeren!");
        systemAudioButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00aa88));
        systemAudioButton.onClick = [this]()
        {
            bool enabled = systemAudioButton.getToggleState();
            auto& capture = audioProcessor.getSystemAudioCapture();
            
            if (enabled)
            {
                if (capture.startCapture())
                {
                    // Output-Device ComboBox befüllen und anzeigen
                    refreshSysAudioOutputDevices();
                    sysAudioOutputCombo.setVisible(true);
                    sysAudioOutputLabel.setVisible(true);
                    
                    auto options = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::InfoIcon)
                        .withTitle("System Audio Capture")
                        .withMessage("Windows Audio wird jetzt analysiert!\n\n"
                                     "Waehle unter 'Ausgabe' ein Geraet (z.B. TR-8S, M-Audio),\n"
                                     "um das bearbeitete Signal zu hoeren.\n\n"
                                     "Ohne Ausgabegeraet ist der Output stumm (Anti-Feedback).")
                        .withButton("OK");
                    juce::AlertWindow::showAsync(options, nullptr);
                }
                else
                {
                    systemAudioButton.setToggleState(false, juce::dontSendNotification);
                    auto options = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("System Audio Capture")
                        .withMessage("Konnte WASAPI Loopback nicht starten.\n\nBitte stelle sicher, dass Audioausgabe aktiv ist.")
                        .withButton("OK");
                    juce::AlertWindow::showAsync(options, nullptr);
                }
            }
            else
            {
                capture.stopCapture();
                sysAudioOutputCombo.setVisible(false);
                sysAudioOutputLabel.setVisible(false);
            }
            resized();
        };
        addAndMakeVisible(systemAudioButton);
        
        // NEU: Output-Device Routing ComboBox (nur sichtbar wenn Sys Audio aktiv)
        sysAudioOutputLabel.setText("Ausgabe:", juce::dontSendNotification);
        sysAudioOutputLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        sysAudioOutputLabel.setJustificationType(juce::Justification::centredRight);
        sysAudioOutputLabel.setVisible(false);
        addAndMakeVisible(sysAudioOutputLabel);
        
        sysAudioOutputCombo.setTooltip("Ausgabegeraet fuer das bearbeitete Signal.\n"
                                        "Waehle ein anderes Geraet als den Windows-Standard-Ausgang,\n"
                                        "um Feedback zu vermeiden.\n\n"
                                        "Beispiele: TR-8S, M-Audio M-Track Solo, Kopfhoerer etc.");
        sysAudioOutputCombo.addItem("-- Kein Output (Stumm) --", 1);
        sysAudioOutputCombo.setSelectedId(1, juce::dontSendNotification);
        sysAudioOutputCombo.setVisible(false);
        sysAudioOutputCombo.onChange = [this]()
        {
            auto& capture = audioProcessor.getSystemAudioCapture();
            int selectedId = sysAudioOutputCombo.getSelectedId();
            
            if (selectedId <= 1)
            {
                // Kein Output → Routing stoppen
                capture.stopOutputRouting();
            }
            else
            {
                // Device-ID aus der Item-Text extrahieren (gespeichert in Properties)
                juce::String deviceId = sysAudioOutputCombo.getProperties()["deviceId_" + juce::String(selectedId)].toString();
                if (deviceId.isNotEmpty())
                {
                    if (!capture.startOutputRouting(deviceId))
                    {
                        auto options = juce::MessageBoxOptions()
                            .withIconType(juce::MessageBoxIconType::WarningIcon)
                            .withTitle("Output-Routing")
                            .withMessage("Konnte das Ausgabegeraet nicht oeffnen.\n\nBitte waehle ein anderes Geraet.")
                            .withButton("OK");
                        juce::AlertWindow::showAsync(options, nullptr);
                        sysAudioOutputCombo.setSelectedId(1, juce::dontSendNotification);
                    }
                }
            }
        };
        addAndMakeVisible(sysAudioOutputCombo);
        
        // Theme-Listener registrieren (thread-safe)
        ThemeManager::getInstance().setOnThemeChanged([this](ThemeManager::ThemeID /*id*/)
        {
            customLookAndFeel.updateColors();
            repaint();
            
            // Trigger repaint für alle sichtbaren Komponenten
            for (int i = 0; i < getNumChildComponents(); ++i)
            {
                if (auto* child = getChildComponent(i))
                    child->repaint();
            }
        });
    }
    catch (const std::exception&)
    {
        // Fehler beim Initialisieren von GUI-Komponenten
        throw;
    }

    // Band-Controls
    bandControls.addListener(this);
    addAndMakeVisible(bandControls);

    // Band-Popup
    bandPopup.addListener(this);
    addChildComponent(bandPopup);
    bandPopup.setVisible(false);

    // Reset-Button
    resetButton.setButtonText("Reset");
    resetButton.setTooltip("Reset All EQ Bands\nResets frequency, gain, Q-factor and filter type\nof all bands to default values.");
    resetButton.onClick = [this]()
    {
        // Bestätigungsdialog mit moderner JUCE 8 API
        juce::MessageBoxOptions options;
        options = options.withIconType(juce::MessageBoxIconType::QuestionIcon)
                         .withTitle("Reset EQ")
                         .withMessage("Alle EQ-Bänder auf Standardwerte zurücksetzen?")
                         .withButton("Reset")
                         .withButton("Abbrechen");
        
        juce::AlertWindow::showAsync(options, [this](int result)
        {
            if (result == 1)  // "Reset" wurde geklickt
            {
                audioProcessor.resetAllBands();
            }
        });
    };
    addAndMakeVisible(resetButton);
    
    // NEU: Undo/Redo Buttons
    undoButton.setButtonText(juce::CharPointer_UTF8("\xe2\x86\xa9"));  // ↩ Undo Symbol
    undoButton.setTooltip("Undo (Ctrl+Z)\nReverts the last parameter change.");
    undoButton.onClick = [this]() { audioProcessor.getUndoManager().undo(); };
    addAndMakeVisible(undoButton);
    
    redoButton.setButtonText(juce::CharPointer_UTF8("\xe2\x86\xaa"));  // ↪ Redo Symbol
    redoButton.setTooltip("Redo (Ctrl+Y)\nReapplies a previously undone change.");
    redoButton.onClick = [this]() { audioProcessor.getUndoManager().redo(); };
    addAndMakeVisible(redoButton);

    // ===== Toolbar Tabs (Processing | Smart | Analyzer) =====
    processingTabButton.setButtonText("Processing");
    processingTabButton.setClickingTogglesState(true);
    processingTabButton.onClick = [this]()
    {
        currentToolbarTab_ = ToolbarTab::Processing;
        applyToolbarTabVisibility();
        resized();
    };
    addAndMakeVisible(processingTabButton);

    smartTabButton.setButtonText("Smart");
    smartTabButton.setClickingTogglesState(true);
    smartTabButton.onClick = [this]()
    {
        currentToolbarTab_ = ToolbarTab::Smart;
        applyToolbarTabVisibility();
        resized();
    };
    addAndMakeVisible(smartTabButton);

    analyzerTabButton.setButtonText("Analyzer");
    analyzerTabButton.setClickingTogglesState(true);
    analyzerTabButton.onClick = [this]()
    {
        currentToolbarTab_ = ToolbarTab::Analyzer;
        applyToolbarTabVisibility();
        resized();
    };
    addAndMakeVisible(analyzerTabButton);

    // Preset-Component
    presetComponent.addListener(this);
    addAndMakeVisible(presetComponent);
    
    // Output-Controls
    setupOutputControls();
    
    // Analyzer-Controls
    setupAnalyzerControls();
    
    // Smart EQ Setup
    setupSmartEQ();

    // Toolbar Tabs initialisieren
    processingTabButton.setToggleState(true, juce::dontSendNotification);
    smartTabButton.setToggleState(false, juce::dontSendNotification);
    analyzerTabButton.setToggleState(false, juce::dontSendNotification);
    applyToolbarTabVisibility();
    
    // Initiale Band-Daten setzen
    updateFromProcessor();
    
    // Resizing aktivieren
    setResizable(true, true);
    setResizeLimits(1000, 550, 1800, 1000);  // Mindestbreite erhöht für Smart EQ Panel
    
    // Timer NACH vollständiger Initialisierung starten!
    spectrumAnalyzer.startAnalyzer();
    eqCurve.startCurveUpdates();
    updateTimer.startTimerHz(25);  // 25 FPS - reduziert für bessere Performance
    
    // ===== NEU: Trial-Banner am unteren Rand =====
    trialBannerLabel.setJustificationType(juce::Justification::centred);
    trialBannerLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    addAndMakeVisible(trialBannerLabel);
    
    // Klick auf Banner oeffnet Lizenz-Dialog
    trialBannerLabel.setInterceptsMouseClicks(true, false);
    trialBannerLabel.addMouseListener(this, false);
    updateTrialBanner();
    
    // ===== NEU: Startup-Dialog bei abgelaufener Trial =====
    {
        auto& lm = LicenseManager::getInstance();
        lm.refreshCachedStatus();
        auto licStatus = lm.getCachedLicenseStatus();
        if (licStatus == LicenseManager::LicenseStatus::TrialExpired)
        {
            // Dialog nach kurzem Delay oeffnen (damit GUI erst fertig aufgebaut ist)
            auto safePtrForTrial = juce::Component::SafePointer<AuraAudioProcessorEditor>(this);
            juce::Timer::callAfterDelay(500, [safePtrForTrial]() {
                if (safePtrForTrial != nullptr)
                    safePtrForTrial->showLicenseDialog();
            });
        }
    }

    // ===== Online-Lizenz-Check bei jedem Start =====
    {
        auto& lm = LicenseManager::getInstance();
        if (lm.isOnlineActivated())
        {
            auto safeThis = juce::Component::SafePointer<AuraAudioProcessorEditor>(this);
            juce::Timer::callAfterDelay(1500, [safeThis]() {
                if (safeThis == nullptr) return;
                auto& licMgr = LicenseManager::getInstance();
                auto safeInner = juce::Component::SafePointer<AuraAudioProcessorEditor>(safeThis.getComponent());
                licMgr.validateOnStartup([safeInner](bool success, const juce::String& /*message*/) {
                    if (!success && safeInner != nullptr)
                    {
                        // Lizenz wurde serverseitig gesperrt/deaktiviert
                        safeInner->updateTrialBanner();
                        safeInner->showLicenseDialog();
                    }
                });
            });
        }
    }

    // ===== Update-Check beim Start =====
    updateChecker.addListener(this);
    updateBanner.onClicked = [this]() {
        showUpdateDialog(updateChecker.getLastResult());
    };
    addAndMakeVisible(updateBanner);
    updateBanner.setVisible(false);
    // Verzögerter Check damit GUI erst fertig ist
    auto safePtrForUpdate = juce::Component::SafePointer<AuraAudioProcessorEditor>(this);
    juce::Timer::callAfterDelay(3000, [safePtrForUpdate]() {
        if (safePtrForUpdate != nullptr)
            safePtrForUpdate->updateChecker.checkForUpdates();
    });
}

AuraAudioProcessorEditor::~AuraAudioProcessorEditor()
{
    // Update-Checker Listener entfernen
    updateChecker.removeListener(this);
    // Timer zuerst stoppen!
    updateTimer.stopTimer();
    spectrumAnalyzer.stopAnalyzer();
    eqCurve.stopCurveUpdates();
    
    // OpenGL-Context VOR allen Komponenten detachen (wichtig!)
    if (openGLContext.isAttached())
        openGLContext.detach();
    
    // Lizenz-Dialog schliessen falls offen
    licenseDialogWindow.reset();
    
    // Listener entfernen
    eqCurve.removeListener(this);
    bandControls.removeListener(this);
    bandPopup.removeListener(this);
    setLookAndFeel(nullptr);
}

//==============================================================================
// Sys Audio Output-Routing: Verfügbare Ausgabegeräte laden
//==============================================================================
void AuraAudioProcessorEditor::refreshSysAudioOutputDevices()
{
    auto& capture = audioProcessor.getSystemAudioCapture();
    auto devices = capture.getAvailableOutputDevices();
    
    sysAudioOutputCombo.clear(juce::dontSendNotification);
    sysAudioOutputCombo.addItem("-- Kein Output (Stumm) --", 1);
    
    int itemId = 2;
    for (const auto& device : devices)
    {
        // Standard-Ausgabegerät markieren (Feedback-Warnung)
        juce::String label = device.name;
        if (device.isDefault)
            label += " [Standard - Feedback!]";
        
        sysAudioOutputCombo.addItem(label, itemId);
        
        // Device-ID als Property speichern für späteren Zugriff
        sysAudioOutputCombo.getProperties().set("deviceId_" + juce::String(itemId), device.id);
        
        ++itemId;
    }
    
    sysAudioOutputCombo.setSelectedId(1, juce::dontSendNotification);
}

void AuraAudioProcessorEditor::applyToolbarTabVisibility()
{
    const bool showProcessing = (currentToolbarTab_ == ToolbarTab::Processing);
    const bool showSmart = (currentToolbarTab_ == ToolbarTab::Smart);
    const bool showAnalyzer = (currentToolbarTab_ == ToolbarTab::Analyzer);

    processingTabButton.setToggleState(showProcessing, juce::dontSendNotification);
    smartTabButton.setToggleState(showSmart, juce::dontSendNotification);
    analyzerTabButton.setToggleState(showAnalyzer, juce::dontSendNotification);

    // Processing Tab: Alle Buttons direkt sichtbar
    undoButton.setVisible(showProcessing);
    redoButton.setVisible(showProcessing);
    oversamplingCombo.setVisible(showProcessing);
    eqQualityCombo.setVisible(showProcessing);
    characterModeCombo.setVisible(showProcessing);
    phaseModeCombo.setVisible(showProcessing);
    const bool showMixedPhaseCrossover = showProcessing && (phaseModeCombo.getSelectedId() == 3);
    phaseCrossoverSlider.setVisible(showMixedPhaseCrossover);
    deltaButton.setVisible(showProcessing);
    suppressorButton.setVisible(showProcessing);

    // Smart Tab: Alle Buttons direkt sichtbar
    smartModeButton.setVisible(showSmart);
    referenceButton.setVisible(showSmart);
    grabModeButton.setVisible(showSmart);
    eqSketchButton.setVisible(showSmart);
    genreMorphButton.setVisible(showSmart);
    multiRefButton.setVisible(showSmart);
    tutorialButton.setVisible(showSmart);
    spectralDynButton.setVisible(showSmart);
    crossChannelButton.setVisible(showSmart);

    // Analyzer Tab: Alle Buttons direkt sichtbar
    analyzerButton.setVisible(showAnalyzer);
    phaseButton.setVisible(showAnalyzer);
    analyzerModeCombo.setVisible(showAnalyzer);
    midSideButton.setVisible(showAnalyzer);

    // Legacy-Proxy bleibt unsichtbar, wird nur für alte States/Automationen synchron gehalten
    linearPhaseButton.setVisible(false);
    
    // Accordion Buttons ausblenden (nicht mehr benötigt)
    processingExpandButton.setVisible(false);
    smartExpandButton.setVisible(false);
    analyzerExpandButton.setVisible(false);
}

void AuraAudioProcessorEditor::setupOutputControls()
{
    // Input Gain Slider
    inputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    inputGainSlider.setRange(-24.0f, 24.0f, 0.1f);
    inputGainSlider.setValue(0.0f);
    inputGainSlider.setDoubleClickReturnValue(true, 0.0f);
    inputGainSlider.setTooltip("Input Gain (-24 to +24 dB)\nAdjusts the input volume before the EQ.\nUseful for creating headroom or boosting quiet signals.\nDouble-click to reset to 0 dB.");
    addAndMakeVisible(inputGainSlider);

    inputGainLabel.setText("Input", juce::dontSendNotification);
    inputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputGainLabel);

    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::INPUT_GAIN, inputGainSlider);

    // Output Gain Slider
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    outputGainSlider.setRange(-24.0f, 24.0f, 0.1f);
    outputGainSlider.setValue(0.0f);
    outputGainSlider.setDoubleClickReturnValue(true, 0.0f);
    outputGainSlider.setTooltip("Output Gain (-24 to +24 dB)\nAdjusts the output volume after the EQ.\nUse this to compensate for level changes caused by EQ.\nDouble-click to reset to 0 dB.");
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("Output", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::OUTPUT_GAIN, outputGainSlider);

    // Phase Mode Combo (Minimum / Linear / Mixed)
    phaseModeCombo.addItem("Min", 1);
    phaseModeCombo.addItem("Linear", 2);
    phaseModeCombo.addItem("Mixed", 3);
    phaseModeCombo.setTooltip("Phase Mode\nMinimum = klassischer IIR-EQ mit geringster Latenz\nLinear = phasenlinear für transparentes Mastering\nMixed = tiefe Frequenzen minimum-phase, Höhen linear-phase\n\nÄltere Presets mit dem Legacy-Linear-Phase-Flag werden automatisch übernommen.");
    phaseModeCombo.onChange = [this]()
    {
        if (phaseModeControlSyncInProgress_)
            return;

        auto& apvts = audioProcessor.getAPVTS();
        const int phaseModeIndex = juce::jmax(0, phaseModeCombo.getSelectedId() - 1);

        if (auto* phaseModeParam = apvts.getParameter(ParameterIDs::PHASE_MODE))
            phaseModeParam->setValueNotifyingHost(phaseModeParam->convertTo0to1(static_cast<float>(phaseModeIndex)));

        if (auto* legacyLinearParam = apvts.getParameter(ParameterIDs::LINEAR_PHASE_MODE))
            legacyLinearParam->setValueNotifyingHost(phaseModeIndex == 1 ? 1.0f : 0.0f);

        applyToolbarTabVisibility();
        resized();
    };
    addAndMakeVisible(phaseModeCombo);

    // Legacy-Linear-Phase-Proxy für alte Presets/Automationen unsichtbar weiterführen
    linearPhaseButton.setButtonText("Lin Phase");
    linearPhaseButton.setClickingTogglesState(true);
    linearPhaseButton.setVisible(false);
    addChildComponent(linearPhaseButton);

    linearPhaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::LINEAR_PHASE_MODE, linearPhaseButton);

    // Mid/Side Button
    midSideButton.setButtonText("M/S");
    midSideButton.setClickingTogglesState(true);
    midSideButton.setTooltip("Mid/Side Mode\nEnables Mid/Side processing instead of Stereo (L/R).\nMid = center of stereo field, Side = sides.\nAllows targeted EQ of mono and stereo content.");
    addAndMakeVisible(midSideButton);

    auto& apvts = audioProcessor.getAPVTS();
    if (auto* msModeParam = apvts.getParameter(ParameterIDs::MID_SIDE_MODE))
    {
        midSideButton.onStateChange = [this, msModeParam]()
        {
            msModeParam->setValueNotifyingHost(midSideButton.getToggleState() ? 1.0f : 0.0f);
        };
    }

    // Analyzer On/Off Button
    analyzerButton.setButtonText("Analyzer");
    analyzerButton.setTooltip("Spectrum Analyzer on/off\nDisplays the audio signal's frequency spectrum in real-time.");
    addAndMakeVisible(analyzerButton);

    analyzerOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::ANALYZER_ON, analyzerButton);

    // Phase-Anzeige Button
    phaseButton.setButtonText("Phase");
    phaseButton.setTooltip("Phase Display on/off\nShows the phase shift of the EQ curve.");
    addAndMakeVisible(phaseButton);
    phaseButton.onClick = [this]() {
        eqCurve.setShowPhase(phaseButton.getToggleState());
    };

    // Analyzer Mode ComboBox
    analyzerModeCombo.addItem("Pre", 1);
    analyzerModeCombo.addItem("Post", 2);
    analyzerModeCombo.addItem("Both", 3);
    analyzerModeCombo.setTooltip("Analyzer Mode\nPre = Signal before EQ\nPost = Signal after EQ\nBoth = Show both simultaneously");
    addAndMakeVisible(analyzerModeCombo);

    analyzerModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::ANALYZER_PRE_POST, analyzerModeCombo);

    // Analyzer-Modus-Änderung verbinden
    analyzerModeCombo.onChange = [this]()
    {
        int mode = analyzerModeCombo.getSelectedId();
        spectrumAnalyzer.setShowPre(mode == 1 || mode == 3);
        spectrumAnalyzer.setShowPost(mode == 2 || mode == 3);
    };
    
    // Grab Mode Button
    grabModeButton.setButtonText("Grab");
    grabModeButton.setClickingTogglesState(true);
    grabModeButton.setTooltip("Spectrum Grab Mode\nClick on peaks in the spectrum to automatically create\nan EQ band at that frequency with matching gain and Q.");
    addAndMakeVisible(grabModeButton);
    
    grabModeButton.onClick = [this]()
    {
        bool isActive = grabModeButton.getToggleState();
        spectrumGrabTool.setGrabMode(isActive);
        spectrumGrabTool.setVisible(isActive);
    };
    
    // Grab-Tool Callback: Band ueber APVTS erstellen (nicht direkt auf EQProcessor)
    spectrumGrabTool.onBandGrabbed = [this](int bandIndex, float frequency, float gain, float q, int filterType)
    {
        auto& apvts = audioProcessor.getAPVTS();
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(bandIndex)))
            param->setValueNotifyingHost(1.0f);
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(bandIndex)))
            param->setValueNotifyingHost(param->convertTo0to1(frequency));
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(bandIndex)))
            param->setValueNotifyingHost(param->convertTo0to1(gain));
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(bandIndex)))
            param->setValueNotifyingHost(param->convertTo0to1(q));
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(bandIndex)))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(filterType)));
        
        if (auto* param = apvts.getParameter(ParameterIDs::getBandBypassID(bandIndex)))
            param->setValueNotifyingHost(0.0f);
        
        eqCurve.setSelectedBand(bandIndex);
        updateBandControlsDisplay();
    };
    
    // NEU: Wet/Dry Mix Slider
    wetDrySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    wetDrySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 18);
    wetDrySlider.setRange(0.0f, 100.0f, 1.0f);
    wetDrySlider.setValue(100.0f);
    wetDrySlider.setDoubleClickReturnValue(true, 100.0f);
    wetDrySlider.setTooltip("Wet/Dry Mix (Parallel Processing)\n100% = EQ signal only\n0% = Original signal only (bypass)\nValues in between blend original and EQ signal.\nDouble-click to reset to 100%.");
    addAndMakeVisible(wetDrySlider);
    
    wetDryLabel.setText("Mix", juce::dontSendNotification);
    wetDryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wetDryLabel);
    
    wetDryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::WET_DRY_MIX, wetDrySlider);
    
    // NEU: Delta-Modus Button
    deltaButton.setButtonText("Delta");
    deltaButton.setClickingTogglesState(true);
    deltaButton.setTooltip("Delta Mode (Difference Signal)\nPlays only the difference between original and EQ signal.\nLets you hear exactly what the EQ is changing.\nPerfect for finding resonances and problems.");
    deltaButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffdd6633));
    addAndMakeVisible(deltaButton);
    
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::DELTA_MODE, deltaButton);
    
    // NEU: Oversampling ComboBox
    oversamplingCombo.addItem("OS: Off", 1);
    oversamplingCombo.addItem("OS: 2x", 2);
    oversamplingCombo.addItem("OS: 4x", 3);
    oversamplingCombo.setTooltip("Oversampling Factor\nOff = No oversampling (lowest CPU)\n2x = Double sample rate (good quality)\n4x = Quadruple sample rate (best quality)\n\nReduces aliasing artifacts at high frequencies.\nHigher values = better quality, but more CPU.");
    addAndMakeVisible(oversamplingCombo);
    
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::OVERSAMPLING_FACTOR, oversamplingCombo);

    // EQ Quality Combo (Nyquist-Matched / Standard)
    eqQualityCombo.addItem("Std", 1);
    eqQualityCombo.addItem("HQ", 2);
    eqQualityCombo.setTooltip("EQ Quality\nStd = klassische IIR-Koeffizienten\nHQ = Nyquist-Matched-Transform für präzisere Höhen und saubere Top-End-Formen\n\nVor allem bei aggressiven High-Shelf- und Cut-Settings hörbar sinnvoll.");
    addAndMakeVisible(eqQualityCombo);

    eqQualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::EQ_QUALITY, eqQualityCombo);

    // Character Mode Combo (harmonic coloration)
    characterModeCombo.addItem("Char: Off", 1);
    characterModeCombo.addItem("Char: Subtle", 2);
    characterModeCombo.addItem("Char: Warm", 3);
    characterModeCombo.setTooltip("Character Mode\nOff = neutral\nSubtle = leichte harmonische Verdichtung\nWarm = staerkere, musikalische Saettigung\n\nEignet sich gut fuer Tone-Shaping nach der EQ-Kurve.");
    addAndMakeVisible(characterModeCombo);

    characterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::CHARACTER_MODE, characterModeCombo);

    // Mixed-Phase Crossover (nur relevant im Mixed-Phase-Modus)
    phaseCrossoverSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    phaseCrossoverSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 18);
    phaseCrossoverSlider.setRange(200.0f, 5000.0f, 1.0f);
    phaseCrossoverSlider.setSkewFactorFromMidPoint(700.0f);
    phaseCrossoverSlider.setTooltip("Mixed-Phase Crossover\nSetzt die Trennfrequenz fuer den Mixed-Mode:\nunterhalb eher Minimum-Phase, oberhalb eher Linear-Phase.\n\nNur im Mixed-Phase-Modus relevant.");
    phaseCrossoverSlider.setTextValueSuffix(" Hz");
    addAndMakeVisible(phaseCrossoverSlider);

    phaseCrossoverAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::PHASE_CROSSOVER, phaseCrossoverSlider);
    
    // NEU: Resonance Suppressor Button
    suppressorButton.setButtonText("Suppress");
    suppressorButton.setClickingTogglesState(true);
    suppressorButton.setTooltip("Resonance Suppressor\nAutomatically detects and suppresses resonances\nin the frequency spectrum using 16 dynamic bands.\n\nIdeal for harsh vocals, room resonances,\nor annoying peaks in instruments.");
    suppressorButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8844cc));
    addAndMakeVisible(suppressorButton);
    
    suppressorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::SUPPRESSOR_ENABLED, suppressorButton);
    
    // NEU: Piano Roll Overlay
    addAndMakeVisible(pianoRollOverlay);
    pianoRollOverlay.setEnabled(false);
    pianoRollOverlay.setVisible(false);
    
    pianoRollButton.setButtonText("Notes");
    pianoRollButton.setClickingTogglesState(true);
    pianoRollButton.setTooltip("Piano Roll / Note Overlay\nDisplays musical notes (C0-C10) over the analyzer.\nHelps align EQ settings to specific notes and instruments.");
    pianoRollButton.onClick = [this]()
    {
        bool enabled = pianoRollButton.getToggleState();
        pianoRollOverlay.setEnabled(enabled);
        pianoRollOverlay.setVisible(enabled);
    };
    addAndMakeVisible(pianoRollButton);
    
    // ===== v2.0: Neue Panel-Buttons =====
    spectralDynButton.setButtonText("Spectral");
    spectralDynButton.setClickingTogglesState(true);
    spectralDynButton.setTooltip("Spectral Dynamics\nPer-frequency dynamic processing\n(Compress/Expand/Gate per band)");
    spectralDynButton.onClick = [this]()
    {
        showSpectralDynPanel = spectralDynButton.getToggleState();
        if (showSpectralDynPanel && spectralDynamicsPanel == nullptr)
        {
            spectralDynamicsPanel = std::make_unique<SpectralDynamicsPanel>(
                audioProcessor.getSpectralDynamics());
            addAndMakeVisible(*spectralDynamicsPanel);
        }
        if (spectralDynamicsPanel)
            spectralDynamicsPanel->setVisible(showSpectralDynPanel);
        resized();
    };
    addAndMakeVisible(spectralDynButton);
    
    crossChannelButton.setButtonText("Cross-Ch");
    crossChannelButton.setClickingTogglesState(true);
    crossChannelButton.setTooltip("Cross-Channel Analysis\nShows frequency collisions with other Aura instances\nand suggests automatic corrections.");
    crossChannelButton.onClick = [this]()
    {
        showCrossChannelPanel = crossChannelButton.getToggleState();
        if (showCrossChannelPanel && crossChannelPanel == nullptr)
        {
            crossChannelPanel = std::make_unique<CrossChannelPanel>(
                audioProcessor.getCrossChannel());
            addAndMakeVisible(*crossChannelPanel);
        }
        if (crossChannelPanel)
            crossChannelPanel->setVisible(showCrossChannelPanel);
        resized();
    };
    addAndMakeVisible(crossChannelButton);
    
    eqSketchButton.setButtonText("Sketch");
    eqSketchButton.setClickingTogglesState(true);
    eqSketchButton.setTooltip("EQ Sketch\nDraw a freehand EQ curve that automatically\ngets converted into EQ bands.");
    eqSketchButton.onClick = [this]()
    {
        bool active = eqSketchButton.getToggleState();
        eqCurve.setSketchMode(active);
        if (!active)
            audioProcessor.getEQSketchTool().clearSketch();
        repaint();
    };
    addAndMakeVisible(eqSketchButton);
    
    // v2.0: Genre Morph Button
    genreMorphButton.setButtonText("Morph");
    genreMorphButton.setClickingTogglesState(true);
    genreMorphButton.setTooltip("Genre Morph\n2D XY-Pad for morphing between genre profiles.\nDrag the point to interpolate the EQ character\nbetween different genres.");
    genreMorphButton.onClick = [this]()
    {
        showGenreMorphPanel = genreMorphButton.getToggleState();
        if (showGenreMorphPanel && genreMorphWidget == nullptr)
        {
            genreMorphWidget = std::make_unique<GenreMorphWidget>(
                audioProcessor.getGenreMorphSlider());
            addAndMakeVisible(*genreMorphWidget);
        }
        if (genreMorphWidget)
            genreMorphWidget->setVisible(showGenreMorphPanel);
        resized();
    };
    addAndMakeVisible(genreMorphButton);
    
    // v2.0: Multi-Reference Button
    multiRefButton.setButtonText("Multi-Ref");
    multiRefButton.setClickingTogglesState(true);
    multiRefButton.setTooltip("Multi-Reference\nManage up to 8 reference tracks simultaneously.\nWeight and combine multiple references\nfor more precise spectral matching.");
    multiRefButton.onClick = [this]()
    {
        showMultiRefPanel = multiRefButton.getToggleState();
        if (showMultiRefPanel && multiRefPanel == nullptr)
        {
            multiRefPanel = std::make_unique<MultiReferencePanel>(
                audioProcessor.getMultiReferenceManager());
            addAndMakeVisible(*multiRefPanel);
        }
        if (multiRefPanel)
            multiRefPanel->setVisible(showMultiRefPanel);
        resized();
    };
    addAndMakeVisible(multiRefButton);
    
    // v2.0: Tutorial / Lernmodus Button
    tutorialButton.setButtonText("Learn");
    tutorialButton.setClickingTogglesState(true);
    tutorialButton.setTooltip("Tutorial / Learning Mode\nInteractive guide that walks you through\nthe most important features step by step.\n3 paths: Basics, Smart EQ, Mastering.");
    tutorialButton.onClick = [this]()
    {
        showTutorialOverlay = tutorialButton.getToggleState();
        if (showTutorialOverlay)
        {
            tutorialOverlayPanel.setVisible(true);
            tutorialOverlayPanel.startTutorial(0);  // Basics
            tutorialOverlayPanel.toFront(false);
        }
        else
        {
            tutorialOverlayPanel.endTutorial();
            tutorialOverlayPanel.setVisible(false);
        }
    };
    addAndMakeVisible(tutorialButton);
    
    // Tutorial Overlay (liegt über allem)
    addChildComponent(tutorialOverlayPanel);
    tutorialOverlayPanel.onTutorialEnded = [this]()
    {
        showTutorialOverlay = false;
        tutorialButton.setToggleState(false, juce::dontSendNotification);
        tutorialOverlayPanel.setVisible(false);
    };
}

void AuraAudioProcessorEditor::paint(juce::Graphics& g)
{
    // ===== Subtiler Hintergrund-Gradient (Premium-Effekt) =====
    {
        auto bgDark = CustomLookAndFeel::getBackgroundDark();
        juce::ColourGradient bgGradient(
            bgDark.brighter(0.06f), 0, 0,
            bgDark.darker(0.05f), 0, static_cast<float>(getHeight()),
            false);
        g.setGradientFill(bgGradient);
        g.fillRect(getLocalBounds());
    }

    // Header-Bereich: 2 Zeilen (90 Pixel gesamt) mit Gradient
    {
        auto headerTop = CustomLookAndFeel::getBackgroundMid().brighter(0.04f);
        auto headerBot = CustomLookAndFeel::getBackgroundMid().darker(0.03f);
        juce::ColourGradient headerGrad(headerTop, 0, 0, headerBot, 0, 90.0f, false);
        g.setGradientFill(headerGrad);
        g.fillRect(0, 0, getWidth(), 90);
    }
    
    // Trennlinie zwischen Zeile 1 und 2 (subtiler Schatten-Effekt)
    g.setColour(CustomLookAndFeel::getBackgroundDark().withAlpha(0.6f));
    g.fillRect(0, 55, getWidth(), 1);
    g.setColour(CustomLookAndFeel::getBackgroundLight().withAlpha(0.08f));
    g.fillRect(0, 56, getWidth(), 1);

    // Untere Kante des Headers: Highlight-Linie
    g.setColour(CustomLookAndFeel::getBackgroundDark().withAlpha(0.7f));
    g.fillRect(0, 89, getWidth(), 1);
    g.setColour(CustomLookAndFeel::getBackgroundLight().withAlpha(0.06f));
    g.fillRect(0, 90, getWidth(), 1);

    // Plugin-Name mit Glow
    {
        auto nameColor = CustomLookAndFeel::getTextColor();
        g.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
        // Subtiler Glow hinter dem Text
        g.setColour(CustomLookAndFeel::getAccentColor().withAlpha(0.08f));
        g.drawText("Aura", 14, 7, 102, 26, juce::Justification::left);
        g.setColour(nameColor);
        g.drawText("Aura", 15, 8, 100, 24, juce::Justification::left);
    }

    // Version
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.45f));
    g.drawText("v2.0", 75, 14, 35, 14, juce::Justification::left);

    // ===== Aktiver Tab-Unterstrich (Accent-Color Glow-Linie) =====
    {
        juce::Component* activeTab = nullptr;
        if (processingTabButton.getToggleState()) activeTab = &processingTabButton;
        else if (smartTabButton.getToggleState()) activeTab = &smartTabButton;
        else if (analyzerTabButton.getToggleState()) activeTab = &analyzerTabButton;
        
        if (activeTab != nullptr)
        {
            auto tabBounds = activeTab->getBounds();
            auto accent = CustomLookAndFeel::getAccentColor();
            int lineY = tabBounds.getBottom() + 1;
            int lineX = tabBounds.getX() + 4;
            int lineW = tabBounds.getWidth() - 8;
            
            // Äußerer Glow (breiter, transparent)
            g.setColour(accent.withAlpha(0.15f));
            g.fillRoundedRectangle(static_cast<float>(lineX - 2), static_cast<float>(lineY - 1),
                                    static_cast<float>(lineW + 4), 4.0f, 2.0f);
            // Innere Linie (scharf)
            g.setColour(accent.withAlpha(0.8f));
            g.fillRoundedRectangle(static_cast<float>(lineX), static_cast<float>(lineY),
                                    static_cast<float>(lineW), 2.0f, 1.0f);
        }
    }

    // ===== Toolbar-Separatoren zwischen Button-Gruppen =====
    {
        auto sepColor = CustomLookAndFeel::getTextColor().withAlpha(0.1f);
        g.setColour(sepColor);
        
        // Separator nach Tabs (vor den Buttons)
        auto tabEnd = analyzerTabButton.getBounds().getRight() + 4;
        if (tabEnd > 0)
            g.fillRect(tabEnd, 60, 1, 26);
    }

    // ===== Unterer Settings-Bereich: 2-zeilig (modern & kompakt) =====
    const int dynBandH = juce::jlimit(72, 108, getHeight() * 12 / 100);
    const int settingsAreaH = 50;
    const int meterWidth = 55;
    const int bottomY = getHeight() - settingsAreaH - dynBandH - 24;

    // Hintergrund für den Settings-Bereich mit Gradient
    {
        auto settingsTop = CustomLookAndFeel::getBackgroundMid().withAlpha(0.55f);
        auto settingsBot = CustomLookAndFeel::getBackgroundMid().withAlpha(0.4f);
        juce::ColourGradient settingsGrad(settingsTop, 0, static_cast<float>(bottomY),
                                           settingsBot, 0, static_cast<float>(bottomY + settingsAreaH), false);
        g.setGradientFill(settingsGrad);
        g.fillRect(0, bottomY, getWidth() - meterWidth, settingsAreaH);
    }

    // Obere Kante: Highlight-Linie
    g.setColour(CustomLookAndFeel::getBackgroundLight().withAlpha(0.08f));
    g.fillRect(0, bottomY, getWidth() - meterWidth, 1);

    // Trennlinie zwischen Zeile 1 und 2 (kompakter)
    g.setColour(CustomLookAndFeel::getBackgroundDark().withAlpha(0.3f));
    g.fillRect(8, bottomY + 24, getWidth() - meterWidth - 16, 1);

    // ===== Section Labels im unteren Bereich =====
    {
        auto labelColor = CustomLookAndFeel::getTextColor().withAlpha(0.45f);
        g.setColour(labelColor);
        g.setFont(juce::Font(juce::FontOptions(9.5f).withStyle("Bold")));

        // Zeile 1 rechts: "ANALYZER"
        int labelRightX = getWidth() - meterWidth - 140;
        g.drawText("ANALYZER", labelRightX, bottomY + 2, 60, 10, juce::Justification::centred);

        // Zeile 2: Separator zwischen Display- und Panel-Toggles
        int settRow2Left = 5;
        int sepPanelX = settRow2Left + 72 + 4 + 68 + 3;
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.1f));
        g.fillRect(sepPanelX, bottomY + 28, 1, 18);
    }

    // ===== Moderne Card-Layer für bessere visuelle Hierarchie =====
    {
        const int cardDynBandH = juce::jlimit(72, 108, getHeight() * 12 / 100);
        const int cardSettingsAreaH = 50;
        const int cardMeterWidth = 55;
        const int mainTop = 95;
        const int mainBottom = getHeight() - cardSettingsAreaH - cardDynBandH - 28;
        const int mainHeight = juce::jmax(120, mainBottom - mainTop);

        auto contentCard = juce::Rectangle<float>(6.0f, static_cast<float>(mainTop),
                                                  static_cast<float>(getWidth() - cardMeterWidth - 14),
                                                  static_cast<float>(mainHeight));

        // Card-Schatten
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillRoundedRectangle(contentCard.translated(0.0f, 1.6f), 9.0f);

        // Card-Fläche mit leichtem Vertical-Gradient
        juce::ColourGradient cardGrad(
            CustomLookAndFeel::getBackgroundMid().withAlpha(0.17f), contentCard.getX(), contentCard.getY(),
            CustomLookAndFeel::getBackgroundDark().withAlpha(0.11f), contentCard.getX(), contentCard.getBottom(),
            false);
        g.setGradientFill(cardGrad);
        g.fillRoundedRectangle(contentCard, 9.0f);

        // Card-Border
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.07f));
        g.drawRoundedRectangle(contentCard.reduced(0.5f), 9.0f, 0.9f);

        // Analyzer-Settings als eigene „Toolbar Card“
        auto settingsCard = juce::Rectangle<float>(6.0f,
                                                   static_cast<float>(getHeight() - cardSettingsAreaH - cardDynBandH - 24),
                                                   static_cast<float>(getWidth() - cardMeterWidth - 14),
                                                   static_cast<float>(cardSettingsAreaH));
        g.setColour(CustomLookAndFeel::getBackgroundLight().withAlpha(0.07f));
        g.fillRoundedRectangle(settingsCard, 8.0f);
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.08f));
        g.drawRoundedRectangle(settingsCard.reduced(0.5f), 8.0f, 0.8f);

        // Band-Control-Bereich subtil abheben
        auto bandCard = juce::Rectangle<float>(6.0f,
                                               static_cast<float>(getHeight() - cardDynBandH - 24),
                                               static_cast<float>(getWidth() - cardMeterWidth - 14),
                                               static_cast<float>(cardDynBandH));
        g.setColour(CustomLookAndFeel::getBackgroundLight().withAlpha(0.05f));
        g.fillRoundedRectangle(bandCard, 10.0f);
        g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.06f));
        g.drawRoundedRectangle(bandCard.reduced(0.5f), 10.0f, 0.8f);
    }
}

void AuraAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    // Klick auf Trial-Banner -> Lizenz-Dialog oeffnen
    if (event.eventComponent == &trialBannerLabel)
    {
        showLicenseDialog();
        return;
    }
    
    juce::AudioProcessorEditor::mouseDown(event);
}

bool AuraAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    auto& um = audioProcessor.getUndoManager();

    // Undo: Ctrl+Z
    if (key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        um.undo();
        return true;
    }
    // Redo: Ctrl+Y oder Ctrl+Shift+Z
    if (key == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0) ||
        key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        um.redo();
        return true;
    }
    // Delete/Backspace: Ausgewähltes Band löschen
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        int sel = eqCurve.getSelectedBand();
        if (sel >= 0)
        {
            eqCurve.deleteBand(sel);
            return true;
        }
    }
    // Space: Bypass des ausgewählten Bands umschalten
    if (key == juce::KeyPress::spaceKey)
    {
        int sel = eqCurve.getSelectedBand();
        if (sel >= 0)
        {
            auto* bypassParam = audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandBypassID(sel));
            if (bypassParam != nullptr)
            {
                bool current = bypassParam->getValue() > 0.5f;
                bypassParam->setValueNotifyingHost(current ? 0.0f : 1.0f);
                return true;
            }
        }
    }
    // Ctrl+S: User-Preset speichern
    if (key == juce::KeyPress('s', juce::ModifierKeys::ctrlModifier, 0))
    {
        presetComponent.triggerSavePreset();
        return true;
    }
    // Ctrl+C: Ausgewähltes Band kopieren
    if (key == juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0))
    {
        int sel = eqCurve.getSelectedBand();
        if (sel >= 0)
        {
            audioProcessor.getEQProcessor().copyBandSettings(sel);
            return true;
        }
    }
    // Ctrl+V: Band-Settings einfügen
    if (key == juce::KeyPress('v', juce::ModifierKeys::ctrlModifier, 0))
    {
        int sel = eqCurve.getSelectedBand();
        if (sel >= 0)
        {
            audioProcessor.getEQProcessor().pasteBandSettings(sel);
            // APVTS-Parameter synchronisieren
            const auto& band = audioProcessor.getEQProcessor().getBand(sel);
            auto& apvts = audioProcessor.getAPVTS();
            if (auto* p = apvts.getParameter(ParameterIDs::getBandFreqID(sel)))
                p->setValueNotifyingHost(p->convertTo0to1(band.getFrequency()));
            if (auto* p = apvts.getParameter(ParameterIDs::getBandGainID(sel)))
                p->setValueNotifyingHost(p->convertTo0to1(band.getGain()));
            if (auto* p = apvts.getParameter(ParameterIDs::getBandQID(sel)))
                p->setValueNotifyingHost(p->convertTo0to1(band.getQ()));
            return true;
        }
    }
    // Escape: Band-Selektion aufheben
    if (key == juce::KeyPress::escapeKey)
    {
        if (eqCurve.getSelectedBand() >= 0)
        {
            eqCurve.setSelectedBand(-1);
            return true;
        }
    }
    // Tab / Shift+Tab: Nächstes / Vorheriges aktives Band selektieren
    if (key == juce::KeyPress::tabKey)
    {
        int sel = eqCurve.getSelectedBand();
        int dir = key.getModifiers().isShiftDown() ? -1 : 1;
        // Suche nächstes aktives Band
        for (int attempt = 0; attempt < ParameterIDs::MAX_BANDS; ++attempt)
        {
            sel = ((sel + dir) % ParameterIDs::MAX_BANDS + ParameterIDs::MAX_BANDS) % ParameterIDs::MAX_BANDS;
            auto* activeParam = audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandActiveID(sel));
            if (activeParam != nullptr && activeParam->getValue() > 0.5f)
            {
                eqCurve.setSelectedBand(sel);
                return true;
            }
        }
        return true;
    }
    // Pfeiltasten: Gain / Frequenz des selektierten Bands ändern
    {
        int sel = eqCurve.getSelectedBand();
        if (sel >= 0)
        {
            float gainStep = key.getModifiers().isShiftDown() ? 0.1f : 0.5f;
            float freqFactor = key.getModifiers().isShiftDown() ? 1.01f : 1.05f;

            if (key == juce::KeyPress::upKey || key == juce::KeyPress(juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0))
            {
                auto* gainParam = audioProcessor.getAPVTS().getRawParameterValue(ParameterIDs::getBandGainID(sel));
                if (gainParam != nullptr)
                {
                    float newGain = juce::jlimit(-30.0f, 30.0f, gainParam->load() + gainStep);
                    audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandGainID(sel))->setValueNotifyingHost(
                        audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandGainID(sel))->convertTo0to1(newGain));
                }
                return true;
            }
            if (key == juce::KeyPress::downKey || key == juce::KeyPress(juce::KeyPress::downKey, juce::ModifierKeys::shiftModifier, 0))
            {
                auto* gainParam = audioProcessor.getAPVTS().getRawParameterValue(ParameterIDs::getBandGainID(sel));
                if (gainParam != nullptr)
                {
                    float newGain = juce::jlimit(-30.0f, 30.0f, gainParam->load() - gainStep);
                    audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandGainID(sel))->setValueNotifyingHost(
                        audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandGainID(sel))->convertTo0to1(newGain));
                }
                return true;
            }
            if (key == juce::KeyPress::rightKey || key == juce::KeyPress(juce::KeyPress::rightKey, juce::ModifierKeys::shiftModifier, 0))
            {
                auto* freqParam = audioProcessor.getAPVTS().getRawParameterValue(ParameterIDs::getBandFreqID(sel));
                if (freqParam != nullptr)
                {
                    float newFreq = juce::jlimit(20.0f, 20000.0f, freqParam->load() * freqFactor);
                    audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandFreqID(sel))->setValueNotifyingHost(
                        audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandFreqID(sel))->convertTo0to1(newFreq));
                }
                return true;
            }
            if (key == juce::KeyPress::leftKey || key == juce::KeyPress(juce::KeyPress::leftKey, juce::ModifierKeys::shiftModifier, 0))
            {
                auto* freqParam = audioProcessor.getAPVTS().getRawParameterValue(ParameterIDs::getBandFreqID(sel));
                if (freqParam != nullptr)
                {
                    float newFreq = juce::jlimit(20.0f, 20000.0f, freqParam->load() / freqFactor);
                    audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandFreqID(sel))->setValueNotifyingHost(
                        audioProcessor.getAPVTS().getParameter(ParameterIDs::getBandFreqID(sel))->convertTo0to1(newFreq));
                }
                return true;
            }
        }
    }
    return false;
}

void AuraAudioProcessorEditor::resized()
{
    // Undo/Redo mit Ctrl+Z / Ctrl+Y
    setWantsKeyboardFocus(true);

    applyToolbarTabVisibility();
    
    // Speichere aktuelle Fenstergröße (mit Debounce um Disk-I/O zu reduzieren)
    pendingSaveWidth = getWidth();
    pendingSaveHeight = getHeight();
    if (!saveWindowSizeTimer.isTimerRunning())
        saveWindowSizeTimer.startTimer(500);  // 500ms Debounce
    
    auto bounds = getLocalBounds();

    // Adaptive Density: kompakter bei schmalen Fenstern, komfortabel bei breiten Layouts
    const int windowWidth = bounds.getWidth();
    const bool compactDensity = (windowWidth < 1320);
    const auto densityValue = [compactDensity](int compactValue, int comfortableValue)
    {
        return compactDensity ? compactValue : comfortableValue;
    };
    
    // Schutz: Wenn Bounds zu klein, nicht layouten
    if (bounds.getWidth() < 100 || bounds.getHeight() < 100)
        return;

    // Level Meter auf der rechten Seite (55 Pixel breit - moderne Breite)
    levelMeter.setBounds(bounds.removeFromRight(55).reduced(2, 5));

    // ============================================================
    // HEADER: 2 Zeilen für bessere Sichtbarkeit (90px gesamt)
    // ============================================================
    auto fullHeaderArea = bounds.removeFromTop(90);
    
    // === ZEILE 1 (55px): Presets | Gain-Knobs | Lizenz/Theme ===
    auto row1 = fullHeaderArea.removeFromTop(55);
    
    // Preset-Component links + Reset-Button
    presetComponent.setBounds(row1.removeFromLeft(250).reduced(5));
    resetButton.setBounds(row1.removeFromLeft(58).reduced(2, 12));
    
    // Lizenz-Button + SysAudio + Output-Routing + Update-Banner + Theme Selector (oben rechts)
    bool sysOutputVisible = sysAudioOutputCombo.isVisible();
    int rightWidth = 415 + (updateBanner.isVisible() ? 180 : 0) + (sysOutputVisible ? 230 : 0);
    auto rightRow1 = row1.removeFromRight(rightWidth).reduced(5, 8);
    licenseButton.setBounds(rightRow1.removeFromLeft(55));
    rightRow1.removeFromLeft(5);
    settingsButton.setBounds(rightRow1.removeFromLeft(60));
    rightRow1.removeFromLeft(5);
    systemAudioButton.setBounds(rightRow1.removeFromLeft(70));
    rightRow1.removeFromLeft(4);
    
    // Output-Routing ComboBox (nur sichtbar wenn Sys Audio aktiv)
    if (sysOutputVisible)
    {
        sysAudioOutputLabel.setBounds(rightRow1.removeFromLeft(50));
        rightRow1.removeFromLeft(2);
        sysAudioOutputCombo.setBounds(rightRow1.removeFromLeft(170));
        rightRow1.removeFromLeft(8);
    }
    
    // Update-Banner neben SysAudio (wenn sichtbar)
    if (updateBanner.isVisible())
    {
        updateBanner.setBounds(rightRow1.removeFromLeft(170).reduced(0, 3));
        rightRow1.removeFromLeft(8);
    }
    
    themeSelector.setBounds(rightRow1);

    // Input + Output + Mix Gain Knobs (Mitte-Rechts)
    auto gainArea = row1.removeFromRight(220).reduced(2, 2);
    auto inputArea = gainArea.removeFromLeft(65);
    inputGainLabel.setBounds(inputArea.removeFromTop(14));
    inputGainSlider.setBounds(inputArea);

    gainArea.removeFromLeft(5);
    auto outputArea = gainArea.removeFromLeft(65);
    outputGainLabel.setBounds(outputArea.removeFromTop(14));
    outputGainSlider.setBounds(outputArea);
    
    gainArea.removeFromLeft(5);
    auto wetDryArea = gainArea.removeFromLeft(65);
    wetDryLabel.setBounds(wetDryArea.removeFromTop(14));
    wetDrySlider.setBounds(wetDryArea);
    
    // === ZEILE 2 (35px): Toolbar mit allen Buttons ===
    auto row2 = fullHeaderArea.reduced(densityValue(6, 8), 2);
    const int gap = densityValue(4, 6);

    // Tabs links
    auto tabArea = row2.removeFromLeft(densityValue(232, 268));
    processingTabButton.setBounds(tabArea.removeFromLeft(densityValue(82, 94)).reduced(0, 2));
    tabArea.removeFromLeft(densityValue(4, 6));
    smartTabButton.setBounds(tabArea.removeFromLeft(densityValue(64, 74)).reduced(0, 2));
    tabArea.removeFromLeft(densityValue(4, 6));
    analyzerTabButton.setBounds(tabArea.removeFromLeft(densityValue(74, 84)).reduced(0, 2));
    row2.removeFromLeft(densityValue(6, 9));

    auto placeLeft = [&](juce::Component& c, int width)
    {
        if (!c.isVisible())
            return;
        const int placeWidth = juce::jmin(width, row2.getWidth());
        if (placeWidth <= 0)
            return;
        c.setBounds(row2.removeFromLeft(placeWidth).reduced(0, 2));
        row2.removeFromLeft(juce::jmin(gap, row2.getWidth()));
    };

    // Processing Tab
    placeLeft(undoButton, densityValue(28, 32));
    placeLeft(redoButton, densityValue(28, 32));
    placeLeft(oversamplingCombo, densityValue(84, 96));
    placeLeft(eqQualityCombo, densityValue(58, 66));
    placeLeft(characterModeCombo, densityValue(96, 116));
    placeLeft(phaseModeCombo, densityValue(84, 96));
    placeLeft(phaseCrossoverSlider, densityValue(128, 148));
    placeLeft(deltaButton, densityValue(62, 72));
    placeLeft(suppressorButton, densityValue(78, 90));

    // Smart Tab
    placeLeft(smartModeButton, densityValue(86, 96));
    placeLeft(referenceButton, densityValue(62, 72));
    placeLeft(grabModeButton, densityValue(62, 72));
    placeLeft(eqSketchButton, densityValue(66, 76));
    placeLeft(spectralDynButton, densityValue(74, 86));
    placeLeft(crossChannelButton, densityValue(74, 86));
    placeLeft(genreMorphButton, densityValue(66, 76));
    placeLeft(multiRefButton, densityValue(82, 94));
    placeLeft(tutorialButton, densityValue(66, 76));
    placeLeft(webSearchButton, densityValue(62, 72));

    // Analyzer Tab
    placeLeft(analyzerModeCombo, densityValue(78, 90));
    placeLeft(analyzerButton, densityValue(76, 88));
    placeLeft(phaseButton, densityValue(62, 72));
    placeLeft(midSideButton, densityValue(62, 72));

    // ==========================================
    // Analyzer-Settings Panel — 2 schlanke Zeilen (modern)
    // ==========================================
    auto analyzerSettingsArea = bounds.removeFromBottom(50);  // 2 Zeilen à 22px + 6px Padding

    // --- Zeile 1: Analyzer-Controls ---
    auto settingsRow1 = analyzerSettingsArea.removeFromTop(22).reduced(5, 1);
    const int gap1 = 5;

    analyzerResolutionCombo.setBounds(settingsRow1.removeFromLeft(105));
    settingsRow1.removeFromLeft(gap1);

    analyzerRangeCombo.setBounds(settingsRow1.removeFromLeft(75));
    settingsRow1.removeFromLeft(gap1);

    analyzerSpeedCombo.setBounds(settingsRow1.removeFromLeft(85));
    settingsRow1.removeFromLeft(gap1);

    eqScaleCombo.setBounds(settingsRow1.removeFromLeft(80));
    settingsRow1.removeFromLeft(gap1 + 6);  // Trenner zwischen Combos und Buttons

    // Tilt entfernt (v2.0) - Controls versteckt
    analyzerTiltButton.setVisible(false);
    analyzerTiltSlider.setVisible(false);

    analyzerFreezeButton.setBounds(settingsRow1.removeFromLeft(65));
    settingsRow1.removeFromLeft(gap1);

    analyzerPeaksButton.setBounds(settingsRow1.removeFromLeft(62));
    settingsRow1.removeFromLeft(gap1);

    // Spektrum-Farbschema (rechts in Zeile 1)
    spectrumColorCombo.setBounds(settingsRow1.removeFromRight(130));

    // --- Zeile 2: Feature-Toggles & Display ---
    auto settingsRow2 = analyzerSettingsArea.reduced(5, 1);
    const int gap2 = 4;

    showLabelsButton.setBounds(settingsRow2.removeFromLeft(72));
    settingsRow2.removeFromLeft(gap2);
    
    pianoRollButton.setBounds(settingsRow2.removeFromLeft(68));
    settingsRow2.removeFromLeft(gap2 + 6);  // Trenner: Display → Panels
    
    spectralDynButton.setBounds(settingsRow2.removeFromLeft(65));
    settingsRow2.removeFromLeft(gap2);
    
    crossChannelButton.setBounds(settingsRow2.removeFromLeft(68));
    settingsRow2.removeFromLeft(gap2);
    
    eqSketchButton.setBounds(settingsRow2.removeFromLeft(65));
    settingsRow2.removeFromLeft(gap2);
    
    genreMorphButton.setBounds(settingsRow2.removeFromLeft(65));
    settingsRow2.removeFromLeft(gap2);
    
    multiRefButton.setBounds(settingsRow2.removeFromLeft(78));
    settingsRow2.removeFromLeft(gap2);
    
    tutorialButton.setBounds(settingsRow2.removeFromLeft(65));

    // Band-Controls unten (kompaktes Hybrid-Layout: min 72, max 108, ~12% der Fensterhöhe)
    const int bandControlsHeight = juce::jlimit(72, 108, getHeight() * 12 / 100);
    auto bandControlsArea = bounds.removeFromBottom(bandControlsHeight);

    // ===== Trial-Banner ganz unten (unter BandControls) =====
    {
        auto bannerArea = bandControlsArea.removeFromBottom(24);
        trialBannerLabel.setBounds(bannerArea);
        trialBannerLabel.toFront(false);
    }
    bandControls.setBounds(bandControlsArea.reduced(5));
    
    // Reference Track Panel (unterhalb Band-Controls, wenn aktiv)
    if (referenceTrackPanel != nullptr && showReferencePanel)
    {
        // Variable Höhe für das Reference-Panel
        auto refArea = bounds.removeFromBottom(referencePanelHeight);
        referenceTrackPanel->setBounds(refArea.reduced(5));
    }

    // Spektrum und EQ-Kurve übereinander (Rest des Platzes)
    auto mainArea = bounds.reduced(5);
    
    // Smart Recommendation Panel (rechts, wenn Smart Mode aktiv)
    if (smartModeButton.getToggleState())
    {
        smartRecommendationPanel.setVisible(true);
        int panelWidth = smartRecommendationPanel.getPreferredWidth();
        smartRecommendationPanel.setBounds(mainArea.removeFromRight(panelWidth));
        if (!smartRecommendationPanel.isCollapsed())
            mainArea.removeFromRight(5);
    }
    else
    {
        smartRecommendationPanel.setVisible(false);
    }
    
    // Live Smart EQ Panel (nur wenn sichtbar)
    if (liveSmartEQPanel != nullptr && liveSmartEQPanel->isVisible())
    {
        int liveEQWidth = liveSmartEQPanel->getPreferredWidth();
        auto liveEQArea = mainArea.removeFromRight(liveEQWidth);
        liveSmartEQPanel->setBounds(liveEQArea);
        if (!liveSmartEQPanel->isCollapsed())
            mainArea.removeFromRight(5);
    }
    
    // v2.0: Spectral Dynamics Panel (unter Band-Controls, links)
    if (spectralDynamicsPanel != nullptr && showSpectralDynPanel)
    {
        auto dynArea = mainArea.removeFromBottom(180);
        spectralDynamicsPanel->setBounds(dynArea);
        mainArea.removeFromBottom(3);
    }
    
    // v2.0: Cross-Channel Panel (rechts, wenn aktiv)
    if (crossChannelPanel != nullptr && showCrossChannelPanel)
    {
        auto ccArea = mainArea.removeFromRight(260);
        crossChannelPanel->setBounds(ccArea);
        mainArea.removeFromRight(3);
    }
    
    // v2.0: Multi-Reference Panel (rechts, wenn aktiv)
    if (multiRefPanel != nullptr && showMultiRefPanel)
    {
        int mrWidth = multiRefPanel->getPreferredWidth();
        auto mrArea = mainArea.removeFromRight(mrWidth);
        multiRefPanel->setBounds(mrArea);
        if (!multiRefPanel->isCollapsed())
            mainArea.removeFromRight(3);
    }
    
    // v2.0: Genre Morph Widget (unten, wenn aktiv)
    if (genreMorphWidget != nullptr && showGenreMorphPanel)
    {
        int gmHeight = genreMorphWidget->getPreferredHeight();
        auto gmArea = mainArea.removeFromBottom(gmHeight);
        genreMorphWidget->setBounds(gmArea);
        mainArea.removeFromBottom(3);
    }
    
    // WebSearch Panel (rechts, wenn aktiv)
    if (webSearchPanel != nullptr && showWebSearchPanel)
    {
        int wsWidth = juce::jlimit(260, 400, getWidth() / 4);
        auto wsArea = mainArea.removeFromRight(wsWidth);
        webSearchPanel->setBounds(wsArea);
        mainArea.removeFromRight(3);
    }
    
    spectrumAnalyzer.setBounds(mainArea);
    eqCurve.setBounds(mainArea);
    spectrumGrabTool.setBounds(mainArea);
    smartHighlightOverlay.setBounds(mainArea);
    pianoRollOverlay.setBounds(mainArea);  // NEU: Piano Roll über Analyzer
    
    // v2.0: Tutorial Overlay (über alles)
    if (tutorialOverlayPanel.isVisible())
        tutorialOverlayPanel.setBounds(getLocalBounds());
}

void AuraAudioProcessorEditor::updateFromProcessor()
{
    auto& apvts = audioProcessor.getAPVTS();

    syncPhaseModeControlsFromState();
    
    // NEU: Trial-Banner regelmaessig aktualisieren (~alle 5 Sekunden bei 25 FPS)
    {
        if (++bannerUpdateCounter >= 125)  // 25 FPS * 5s
        {
            bannerUpdateCounter = 0;
            updateTrialBanner();
        }
    }
    
    // Watchdog: Output-Renderer Auto-Restart (~alle 2 Sekunden prüfen)
    {
        if (++outputRoutingWatchdogCounter >= 50)  // 25 FPS * 2s
        {
            outputRoutingWatchdogCounter = 0;
            auto& capture = audioProcessor.getSystemAudioCapture();
            if (capture.isCapturing() && capture.needsOutputRoutingRestart())
            {
                if (capture.restartOutputRouting())
                {
                    DBG("Watchdog: Output-Renderer erfolgreich neu gestartet.");
                }
                else
                {
                    DBG("Watchdog: Output-Renderer Restart fehlgeschlagen.");
                    // ComboBox auf "Stumm" zurücksetzen
                    sysAudioOutputCombo.setSelectedId(1, juce::dontSendNotification);
                }
            }
        }
    }
    
    // Live Smart EQ: Pending Parameter-Änderungen im Message-Thread anwenden (RT-safe)
    // Nicht anwenden auf das Band das der User gerade manuell zieht
    auto& liveSmartEQ = audioProcessor.getLiveSmartEQ();
    {
        int draggedBand = (eqCurve.isDraggingBand()) ? eqCurve.getSelectedBand() : -1;
        liveSmartEQ.setDraggedBand(draggedBand);
    }
    liveSmartEQ.applyPendingParameterChanges(apvts);
    
    // v2.1 FIX: updateMagnitudeResponse() wird NUR noch vom Audio-Thread aufgerufen
    // (via eqParamsDirty_ Flag in processLinearPhaseEQ/processMixedPhaseEQ).
    // Der GUI-Thread-Aufruf hier verursachte eine Data Race auf workingResponse.
    // Entfernt: linearPhaseEQ.updateMagnitudeResponse(audioProcessor.getEQProcessor());
    
    // Live Smart EQ Reset im Message-Thread ausführen (falls angefordert)
    if (liveSmartEQ.shouldReset())
    {
        liveSmartEQ.resetEQBands(apvts);
        liveSmartEQ.clearResetFlag();
    }

    // Band-Daten zur EQ-Kurve synchronisieren
    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        // Überspringe das aktuell gezogene Band um Konflikte zu vermeiden
        if (eqCurve.isDraggingBand() && i == eqCurve.getSelectedBand())
            continue;
        
        // Sicherheitsprüfung für alle Parameter-Zugriffe
        auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(i));
        auto* gainParam = apvts.getRawParameterValue(ParameterIDs::getBandGainID(i));
        auto* qParam = apvts.getRawParameterValue(ParameterIDs::getBandQID(i));
        auto* slopeParam = apvts.getRawParameterValue(ParameterIDs::getBandSlopeID(i));
        auto* typeParam = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(i));
        auto* bypassParam = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(i));
        auto* activeParam = apvts.getRawParameterValue(ParameterIDs::getBandActiveID(i));
        
        if (freqParam == nullptr || gainParam == nullptr || qParam == nullptr ||
            slopeParam == nullptr || typeParam == nullptr || bypassParam == nullptr || activeParam == nullptr)
            continue;
            
        float freq = freqParam->load();
        float gain = gainParam->load();
        float q = qParam->load();
        int slope = static_cast<int>(slopeParam->load());
        int type = static_cast<int>(typeParam->load());
        bool bypass = bypassParam->load() > 0.5f;
        bool active = activeParam->load() > 0.5f;

        eqCurve.setBandParameters(i, freq, gain, q, slope,
                                   static_cast<ParameterIDs::FilterType>(type),
                                   bypass, active);
    }

    // Analyzer-Status aktualisieren
    auto* analyzerOnParam = apvts.getRawParameterValue(ParameterIDs::ANALYZER_ON);
    if (analyzerOnParam != nullptr)
    {
        bool analyzerOn = analyzerOnParam->load() > 0.5f;
        spectrumAnalyzer.setEnabled(analyzerOn);
    }
    
    // Obere Frequenzgrenze dynamisch an Samplerate anpassen
    // (Samplerate kann sich zur Laufzeit ändern, z.B. bei DAW-Wechsel)
    {
        float nyquist = static_cast<float>(audioProcessor.getSampleRate()) * 0.5f;
        float displayMax = std::min(20000.0f, nyquist * 0.95f);
        if (std::abs(spectrumAnalyzer.getMaxFrequency() - displayMax) > 100.0f)
            spectrumAnalyzer.setFrequencyRange(20.0f, displayMax);
    }
    
    // Grab-Tool: Spektrum-Daten vom PostAnalyzer fuettern
    if (spectrumGrabTool.isGrabModeActive())
    {
        const auto& magnitudes = audioProcessor.getPostAnalyzer().getMagnitudes();
        if (!magnitudes.empty())
        {
            spectrumGrabTool.updateSpectrumData(magnitudes, 20.0f, 20000.0f);
        }
    }
    
    // Level Meter aktualisieren (Stereo)
    float outputLevelLeft = audioProcessor.getOutputLevelLeft();
    float outputLevelRight = audioProcessor.getOutputLevelRight();
    levelMeter.setLevel(outputLevelLeft, outputLevelRight);
    
    // Soothe/Suppressor Visualization
    {
        auto* suppressorParam = apvts.getRawParameterValue(ParameterIDs::SUPPRESSOR_ENABLED);
        bool suppressorOn = (suppressorParam != nullptr && suppressorParam->load() > 0.5f);
        
        // Nur anzeigen wenn Suppressor aktiv UND tatsächlich Audio anliegt
        // outputLevelLeft/Right liefert dB-Werte (z.B. -20.0 dB), daher dB-Schwellwert verwenden
        float audioLevelDBSuppressor = std::max(outputLevelLeft, outputLevelRight);
        bool hasAudio = (audioLevelDBSuppressor > -80.0f);  // -80 dB Gate
        
        spectrumAnalyzer.setSootheCurveEnabled(suppressorOn && hasAudio);
        
        if (suppressorOn && hasAudio)
        {
            auto& suppressor = audioProcessor.getResonanceSuppressor();
            int numBins = suppressor.getNumBins();
            if (numBins > 0)
            {
                spectrumAnalyzer.setSootheCurveData(
                    suppressor.getGainReductions(),
                    numBins,
                    audioProcessor.getSampleRate(),
                    audioProcessor.getPostAnalyzer().getCurrentFFTSize());
            }
        }
    }
    
    // Smart Analysis aktualisieren
    updateSmartAnalysis();
    
    // v2.0: Genre Morph — Morph berechnen und Band-Offsets anwenden
    // Nur auf Bänder 0-3 anwenden (Bänder 4-11 sind für Live Smart EQ reserviert)
    // Nicht anwenden während der User ein Band manuell zieht
    // Nicht anwenden wenn Sketch-Mode aktive Bänder gesetzt hat
    if (showGenreMorphPanel && !eqCurve.isDraggingBand() && !sketchActive_)
    {
        auto& morphSlider = audioProcessor.getGenreMorphSlider();
        const auto& morphResult = morphSlider.calculateMorph();
        
        // Band-Frequenzen sammeln und Gain-Offsets berechnen
        float bandFreqs[12] = {};
        for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
        {
            auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(i));
            bandFreqs[i] = (freqParam != nullptr) ? freqParam->load() : 1000.0f;
        }
        morphSlider.calculateBandGainOffsets(bandFreqs, ParameterIDs::MAX_BANDS);
        
        // Nur Bänder 0-3 verwenden (4-11 für Live Smart EQ reserviert)
        constexpr int kMorphLastBand = 4;
        for (int i = 0; i < kMorphLastBand; ++i)
        {
            // Nicht das aktuell selektierte/gezogene Band überschreiben
            if (i == eqCurve.getSelectedBand()) continue;
            
            float offset = morphResult.bandGainOffsets[i];
            
            auto* gainParam = apvts.getParameter(ParameterIDs::getBandGainID(i));
            if (gainParam != nullptr)
            {
                float currentGain = gainParam->convertFrom0to1(gainParam->getValue());
                float targetGain = offset;
                
                // Skip if already at target (within 0.01 dB)
                if (std::abs(targetGain - currentGain) < 0.01f) continue;
                
                // Band aktivieren falls nötig
                if (std::abs(targetGain) > 0.1f)
                {
                    auto* activeParam = apvts.getParameter(ParameterIDs::getBandActiveID(i));
                    if (activeParam != nullptr && activeParam->getValue() < 0.5f)
                        activeParam->setValueNotifyingHost(1.0f);
                }
                
                // Exponentielles Smoothing zum Zielwert (konvergiert!)
                float newGain = currentGain + (targetGain - currentGain) * 0.08f;
                gainParam->setValueNotifyingHost(gainParam->convertTo0to1(
                    juce::jlimit(-24.0f, 24.0f, newGain)));
            }
        }
    }
}

void AuraAudioProcessorEditor::syncPhaseModeControlsFromState()
{
    auto& apvts = audioProcessor.getAPVTS();
    auto* phaseModeParam = apvts.getRawParameterValue(ParameterIDs::PHASE_MODE);
    auto* legacyLinearPhaseParam = apvts.getRawParameterValue(ParameterIDs::LINEAR_PHASE_MODE);

    int effectivePhaseMode = (phaseModeParam != nullptr) ? static_cast<int>(phaseModeParam->load()) : 0;

    if (effectivePhaseMode == 0 && legacyLinearPhaseParam != nullptr && legacyLinearPhaseParam->load() > 0.5f)
        effectivePhaseMode = 1;

    const int targetId = juce::jlimit(1, 3, effectivePhaseMode + 1);
    if (phaseModeCombo.getSelectedId() != targetId)
    {
        juce::ScopedValueSetter<bool> syncGuard(phaseModeControlSyncInProgress_, true);
        phaseModeCombo.setSelectedId(targetId, juce::dontSendNotification);
    }

    applyToolbarTabVisibility();
}

void AuraAudioProcessorEditor::updateBandControlsDisplay()
{
    int selectedBand = eqCurve.getSelectedBand();

    if (selectedBand >= 0)
    {
        auto& apvts = audioProcessor.getAPVTS();

        auto* activeParam = apvts.getRawParameterValue(ParameterIDs::getBandActiveID(selectedBand));
        if (activeParam == nullptr || activeParam->load() <= 0.5f)
        {
            if (eqCurve.getSelectedBand() == selectedBand)
                eqCurve.setSelectedBand(-1);
            else
                bandControls.clearSelection();

            return;
        }

        // Sichere Parameter-Zugriffe
        auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(selectedBand));
        auto* gainParam = apvts.getRawParameterValue(ParameterIDs::getBandGainID(selectedBand));
        auto* qParam = apvts.getRawParameterValue(ParameterIDs::getBandQID(selectedBand));
        auto* typeParam = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(selectedBand));
        auto* channelParam = apvts.getRawParameterValue(ParameterIDs::getBandChannelID(selectedBand));
        auto* bypassParam = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(selectedBand));
        
        if (freqParam == nullptr || gainParam == nullptr || qParam == nullptr ||
            typeParam == nullptr || channelParam == nullptr || bypassParam == nullptr)
        {
            bandControls.clearSelection();
            bandControls.clearAttachments();
            return;
        }

        float freq = freqParam->load();
        float gain = gainParam->load();
        float q = qParam->load();
        int type = static_cast<int>(typeParam->load());
        int channel = static_cast<int>(channelParam->load());
        bool bypass = bypassParam->load() > 0.5f;

        bandControls.setBandData(selectedBand, freq, gain, q,
                                  static_cast<ParameterIDs::FilterType>(type),
                                  static_cast<ParameterIDs::ChannelMode>(channel),
                                  bypass);

        // APVTS-Attachments für das ausgewählte Band setzen
        bandControls.setAttachments(audioProcessor.getAPVTS(), selectedBand);
    }
    else
    {
        bandControls.clearSelection();
        bandControls.clearAttachments();
    }
}

// EQCurveComponent::Listener Implementierung
void AuraAudioProcessorEditor::bandParameterChanged(int bandIndex, float frequency, float gain, float q)
{
    // v2.0 FIX: Manuelle Band-Änderung hebt Sketch-Schutz auf (Bänder 0-3)
    if (bandIndex >= 0 && bandIndex < 4 && sketchActive_)
        sketchActive_ = false;
    
    auto& apvts = audioProcessor.getAPVTS();

    // Parameter über APVTS setzen
    if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(frequency));
    
    if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(gain));
    
    if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(q));
}

void AuraAudioProcessorEditor::bandSelected(int bandIndex)
{
    if (bandIndex >= 0)
    {
        // Update BandControls (immer sichtbar)
        updateBandControlsDisplay();
        
        // Popup wird NICHT automatisch gezeigt!
        // Es öffnet nur bei Rechtsklick (siehe EQCurveComponent::mouseDown)
    }
    else
    {
        bandPopup.setVisible(false);
        bandPopup.clearAttachments();
        bandControls.clearSelection();
        bandControls.clearAttachments();
    }
}

void AuraAudioProcessorEditor::bandCreated(int bandIndex, float frequency)
{
    auto& apvts = audioProcessor.getAPVTS();

    // Neues Band mit vollständigen Standard-Werten initialisieren
    if (auto* param = apvts.getParameter(ParameterIDs::getBandFreqID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(frequency));
    
    if (auto* param = apvts.getParameter(ParameterIDs::getBandGainID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
    
    if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(ParameterIDs::FilterType::Bell)));
    
    // Q-Wert auf Standard setzen
    if (auto* param = apvts.getParameter(ParameterIDs::getBandQID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(ParameterIDs::DEFAULT_Q));
    
    // Bypass auf false setzen
    if (auto* param = apvts.getParameter(ParameterIDs::getBandBypassID(bandIndex)))
        param->setValueNotifyingHost(0.0f);
    
    // KRITISCH: Aktiv-Flag setzen damit Band sichtbar bleibt!
    if (auto* param = apvts.getParameter(ParameterIDs::getBandActiveID(bandIndex)))
        param->setValueNotifyingHost(1.0f);

    updateBandControlsDisplay();
}

// Filter-Typ geändert (über Kontextmenü)
void AuraAudioProcessorEditor::filterTypeChanged(int bandIndex, ParameterIDs::FilterType type)
{
    auto& apvts = audioProcessor.getAPVTS();
    
    if (auto* param = apvts.getParameter(ParameterIDs::getBandTypeID(bandIndex)))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(type)));
    
    // Bei Cut-Filtern Gain auf 0 setzen (falls nicht bereits)
    if (type == ParameterIDs::FilterType::LowCut || type == ParameterIDs::FilterType::HighCut)
    {
        if (auto* gainParam = apvts.getParameter(ParameterIDs::getBandGainID(bandIndex)))
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(0.0f));
    }
    
    updateBandControlsDisplay();
}

void AuraAudioProcessorEditor::bandDeleted(int bandIndex)
{
    auto& apvts = audioProcessor.getAPVTS();

    // Alle Parameter des gelöschten Bandes auf Standard zurücksetzen.
    // WICHTIG: Active-Flag zuletzt zurücknehmen, damit der Processor während
    // der Zwischenstände kein "unsichtbares aber noch wirksames" Band behält.
    if (auto* freqParam = apvts.getParameter(ParameterIDs::getBandFreqID(bandIndex)))
        freqParam->setValueNotifyingHost(freqParam->convertTo0to1(1000.0f));

    if (auto* gainParam = apvts.getParameter(ParameterIDs::getBandGainID(bandIndex)))
        gainParam->setValueNotifyingHost(gainParam->convertTo0to1(0.0f));

    if (auto* qParam = apvts.getParameter(ParameterIDs::getBandQID(bandIndex)))
        qParam->setValueNotifyingHost(qParam->convertTo0to1(ParameterIDs::DEFAULT_Q));

    if (auto* typeParam = apvts.getParameter(ParameterIDs::getBandTypeID(bandIndex)))
        typeParam->setValueNotifyingHost(typeParam->convertTo0to1(0.0f));

    if (auto* bypassParam = apvts.getParameter(ParameterIDs::getBandBypassID(bandIndex)))
        bypassParam->setValueNotifyingHost(0.0f);

    if (auto* channelParam = apvts.getParameter(ParameterIDs::getBandChannelID(bandIndex)))
        channelParam->setValueNotifyingHost(channelParam->convertTo0to1(static_cast<float>(ParameterIDs::ChannelMode::Stereo)));

    if (auto* slopeParam = apvts.getParameter(ParameterIDs::getBandSlopeID(bandIndex)))
        slopeParam->setValueNotifyingHost(slopeParam->convertTo0to1(ParameterIDs::DEFAULT_SLOPE));

    if (auto* dynEnabledParam = apvts.getParameter(ParameterIDs::getBandDynEnabledID(bandIndex)))
        dynEnabledParam->setValueNotifyingHost(0.0f);

    if (auto* dynThresholdParam = apvts.getParameter(ParameterIDs::getBandDynThresholdID(bandIndex)))
        dynThresholdParam->setValueNotifyingHost(dynThresholdParam->convertTo0to1(-20.0f));

    if (auto* dynRatioParam = apvts.getParameter(ParameterIDs::getBandDynRatioID(bandIndex)))
        dynRatioParam->setValueNotifyingHost(dynRatioParam->convertTo0to1(2.0f));

    if (auto* dynAttackParam = apvts.getParameter(ParameterIDs::getBandDynAttackID(bandIndex)))
        dynAttackParam->setValueNotifyingHost(dynAttackParam->convertTo0to1(10.0f));

    if (auto* dynReleaseParam = apvts.getParameter(ParameterIDs::getBandDynReleaseID(bandIndex)))
        dynReleaseParam->setValueNotifyingHost(dynReleaseParam->convertTo0to1(100.0f));

    if (auto* soloParam = apvts.getParameter(ParameterIDs::getBandSoloID(bandIndex)))
        soloParam->setValueNotifyingHost(0.0f);

    if (auto* activeParam = apvts.getParameter(ParameterIDs::getBandActiveID(bandIndex)))
        activeParam->setValueNotifyingHost(0.0f);

    // Zusätzliche harte Synchronisation: Wenn der GUI-Delete schneller ist als
    // nachlaufende Parameter-/Attachment-Updates, darf im Processor keine
    // unsichtbare Restkurve aktiv bleiben.
    auto& band = audioProcessor.getEQProcessor().getBand(bandIndex);
    band.setParameters(ParameterIDs::DEFAULT_FREQUENCY,
                       ParameterIDs::DEFAULT_GAIN,
                       ParameterIDs::DEFAULT_Q,
                       ParameterIDs::FilterType::Bell,
                       ParameterIDs::ChannelMode::Stereo,
                       false);
    band.setSlope(static_cast<int>(ParameterIDs::DEFAULT_SLOPE));
    band.setDynamicMode(false);
    band.setThreshold(-20.0f);
    band.setRatio(2.0f);
    band.setAttack(10.0f);
    band.setRelease(100.0f);
    band.setActive(false);

    eqCurve.setBandParameters(bandIndex,
                              ParameterIDs::DEFAULT_FREQUENCY,
                              ParameterIDs::DEFAULT_GAIN,
                              ParameterIDs::DEFAULT_Q,
                              static_cast<int>(ParameterIDs::DEFAULT_SLOPE),
                              ParameterIDs::FilterType::Bell,
                              false,
                              false);

    bandPopup.setVisible(false);
    bandPopup.clearAttachments();

    // Selection aufräumen wenn nötig
    if (eqCurve.getSelectedBand() == bandIndex)
        eqCurve.setSelectedBand(-1);

    bandControls.clearAttachments();
    bandControls.clearSelection();

    updateFromProcessor();
    updateBandControlsDisplay();

    auto safeThis = juce::Component::SafePointer<AuraAudioProcessorEditor>(this);
    juce::Timer::callAfterDelay(30, [safeThis]()
    {
        if (safeThis != nullptr && safeThis->eqCurve.getSelectedBand() < 0)
        {
            safeThis->bandControls.clearAttachments();
            safeThis->bandControls.clearSelection();
        }
    });
}

// BandControls::Listener Implementierung
void AuraAudioProcessorEditor::bandControlChanged(int bandIndex, const juce::String& parameterName, float value)
{
    auto& apvts = audioProcessor.getAPVTS();
    juce::String paramID;

    if (parameterName == "frequency")
        paramID = ParameterIDs::getBandFreqID(bandIndex);
    else if (parameterName == "gain")
        paramID = ParameterIDs::getBandGainID(bandIndex);
    else if (parameterName == "q")
        paramID = ParameterIDs::getBandQID(bandIndex);
    else if (parameterName == "type")
        paramID = ParameterIDs::getBandTypeID(bandIndex);
    else if (parameterName == "channel")
        paramID = ParameterIDs::getBandChannelID(bandIndex);
    else if (parameterName == "slope")
        paramID = ParameterIDs::getBandSlopeID(bandIndex);
    else if (parameterName == "bypass")
        paramID = ParameterIDs::getBandBypassID(bandIndex);

    if (paramID.isNotEmpty())
    {
        if (auto* param = apvts.getParameter(paramID))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));

            // Slope ist diskret und soll visuell sofort reagieren.
            if (parameterName == "slope")
                updateFromProcessor();
        }
    }
}

// BandPopup::Listener Implementierung
void AuraAudioProcessorEditor::bandPopupValueChanged(int bandIndex, const juce::String& parameterName, float value)
{
    // Gleiche Logik wie bandControlChanged
    bandControlChanged(bandIndex, parameterName, value);
}

void AuraAudioProcessorEditor::bandPopupDeleteRequested(int bandIndex)
{
    eqCurve.deleteBand(bandIndex);
    bandPopup.clearAttachments();
    bandPopup.setVisible(false);
}

void AuraAudioProcessorEditor::bandPopupBypassChanged(int bandIndex, bool bypassed)
{
    auto& apvts = audioProcessor.getAPVTS();
    if (auto* param = apvts.getParameter(ParameterIDs::getBandBypassID(bandIndex)))
        param->setValueNotifyingHost(bypassed ? 1.0f : 0.0f);
}

void AuraAudioProcessorEditor::bandRightClicked(int bandIndex)
{
    // Rechtsklick auf Band -> Zeige Popup
    showBandPopup(bandIndex);
}

void AuraAudioProcessorEditor::bandAutoListenUpdate(int /*bandIndex*/, float freq, float q)
{
    audioProcessor.setAutoListen(true, freq, q);
}

void AuraAudioProcessorEditor::bandAutoListenStop()
{
    audioProcessor.setAutoListen(false);
}

void AuraAudioProcessorEditor::sketchCompleted(const std::vector<EQSketchTool::GeneratedBand>& bands)
{
    if (bands.empty()) return;
    
    auto& apvts = audioProcessor.getAPVTS();
    
    // v2.0 FIX: Sketch nur auf Bänder 0-3 anwenden (4-11 sind für Live Smart EQ reserviert)
    constexpr int kSketchMaxBands = 4;
    int numToApply = std::min(static_cast<int>(bands.size()), kSketchMaxBands);
    
    for (int i = 0; i < numToApply; ++i)
    {
        const auto& band = bands[static_cast<size_t>(i)];
        
        // Band aktivieren
        if (auto* activeParam = apvts.getParameter(ParameterIDs::getBandActiveID(i)))
            activeParam->setValueNotifyingHost(1.0f);
        
        // Frequenz setzen
        if (auto* freqParam = apvts.getParameter(ParameterIDs::getBandFreqID(i)))
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1(band.frequency));
        
        // Gain setzen
        if (auto* gainParam = apvts.getParameter(ParameterIDs::getBandGainID(i)))
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(band.gainDB));
        
        // Q setzen
        if (auto* qParam = apvts.getParameter(ParameterIDs::getBandQID(i)))
            qParam->setValueNotifyingHost(qParam->convertTo0to1(band.q));
        
        // Filter-Typ setzen (Sketch GeneratedBand::Type -> ParameterIDs::FilterType)
        if (auto* typeParam = apvts.getParameter(ParameterIDs::getBandTypeID(i)))
        {
            int typeValue = 0;  // Peak/Bell default
            switch (band.type)
            {
                case EQSketchTool::GeneratedBand::Peak:      typeValue = static_cast<int>(ParameterIDs::FilterType::Bell); break;
                case EQSketchTool::GeneratedBand::LowShelf:   typeValue = static_cast<int>(ParameterIDs::FilterType::LowShelf); break;
                case EQSketchTool::GeneratedBand::HighShelf:  typeValue = static_cast<int>(ParameterIDs::FilterType::HighShelf); break;
                case EQSketchTool::GeneratedBand::LowCut:     typeValue = static_cast<int>(ParameterIDs::FilterType::LowCut); break;
                case EQSketchTool::GeneratedBand::HighCut:    typeValue = static_cast<int>(ParameterIDs::FilterType::HighCut); break;
                default: typeValue = static_cast<int>(ParameterIDs::FilterType::Bell); break;
            }
            typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(typeValue)));
        }
    }
    
    // Sketch Mode automatisch beenden
    eqSketchButton.setToggleState(false, juce::sendNotification);
    
    // v2.0 FIX: Sketch-Schutz aktivieren — verhindert dass Genre Morph sofort überschreibt
    sketchActive_ = true;
    
    // EQ-Kurve aktualisieren
    for (int i = 0; i < numToApply; ++i)
    {
        auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(i));
        auto* gainParam = apvts.getRawParameterValue(ParameterIDs::getBandGainID(i));
        auto* qParam = apvts.getRawParameterValue(ParameterIDs::getBandQID(i));
        if (freqParam && gainParam && qParam)
        {
            auto* slopeParam = apvts.getRawParameterValue(ParameterIDs::getBandSlopeID(i));
            const int slope = (slopeParam != nullptr)
                ? static_cast<int>(slopeParam->load())
                : static_cast<int>(ParameterIDs::DEFAULT_SLOPE);

            eqCurve.setBandParameters(i, *freqParam, *gainParam, *qParam, slope,
                ParameterIDs::FilterType::Bell, false, true);
        }
    }
    
    repaint();
}

void AuraAudioProcessorEditor::applyPreset(const PresetManager::PresetData& preset)
{
    auto& apvts = audioProcessor.getAPVTS();
    
    // Smooth Crossfade starten bevor Parameter geändert werden
    audioProcessor.beginPresetCrossfade();

    for (int i = 0; i < ParameterIDs::MAX_BANDS; ++i)
    {
        const auto& bandSettings = preset.bands[i];

        // Alle Parameter setzen
        if (auto* freqParam = apvts.getParameter(ParameterIDs::getBandFreqID(i)))
            freqParam->setValueNotifyingHost(freqParam->convertTo0to1(bandSettings.frequency));

        if (auto* gainParam = apvts.getParameter(ParameterIDs::getBandGainID(i)))
            gainParam->setValueNotifyingHost(gainParam->convertTo0to1(bandSettings.gain));

        if (auto* qParam = apvts.getParameter(ParameterIDs::getBandQID(i)))
            qParam->setValueNotifyingHost(qParam->convertTo0to1(bandSettings.q));

        if (auto* slopeParam = apvts.getParameter(ParameterIDs::getBandSlopeID(i)))
            slopeParam->setValueNotifyingHost(slopeParam->convertTo0to1(bandSettings.slope));

        if (auto* typeParam = apvts.getParameter(ParameterIDs::getBandTypeID(i)))
            typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(bandSettings.type)));

        if (auto* activeParam = apvts.getParameter(ParameterIDs::getBandActiveID(i)))
            activeParam->setValueNotifyingHost(bandSettings.active ? 1.0f : 0.0f);

        if (auto* bypassParam = apvts.getParameter(ParameterIDs::getBandBypassID(i)))
            bypassParam->setValueNotifyingHost(bandSettings.bypass ? 1.0f : 0.0f);
    }

    updateFromProcessor();
}

void AuraAudioProcessorEditor::presetSelected(const PresetManager::PresetData& preset)
{
    applyPreset(preset);
}

void AuraAudioProcessorEditor::showBandPopup(int bandIndex)
{
    if (bandIndex < 0)
    {
        bandPopup.setVisible(false);
        return;
    }

    // Hole alle Parameter aus APVTS - mit Sicherheitsprüfung
    auto& apvts = audioProcessor.getAPVTS();

    auto* freqParam = apvts.getRawParameterValue(ParameterIDs::getBandFreqID(bandIndex));
    auto* gainParam = apvts.getRawParameterValue(ParameterIDs::getBandGainID(bandIndex));
    auto* typeParam = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(bandIndex));
    auto* channelParam = apvts.getRawParameterValue(ParameterIDs::getBandChannelID(bandIndex));
    auto* slopeParam = apvts.getRawParameterValue(ParameterIDs::getBandSlopeID(bandIndex));
    auto* bypassParam = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(bandIndex));
    
    if (freqParam == nullptr || gainParam == nullptr || typeParam == nullptr ||
        channelParam == nullptr || slopeParam == nullptr || bypassParam == nullptr)
    {
        bandPopup.setVisible(false);
        return;
    }

    float freq = freqParam->load();
    float gain = gainParam->load();
    int type = static_cast<int>(typeParam->load());
    int channel = static_cast<int>(channelParam->load());
    int slope = static_cast<int>(slopeParam->load());
    bool bypass = bypassParam->load() > 0.5f;

    bandPopup.setBandData(bandIndex, freq, gain,
                          static_cast<ParameterIDs::FilterType>(type),
                          static_cast<ParameterIDs::ChannelMode>(channel),
                          slope, bypass);
    
    // EQ-Processor für Auto-Threshold bereitstellen
    bandPopup.setEQProcessor(&audioProcessor.getEQProcessor());

    // Position am Band-Point
    auto pointPos = eqCurve.getBandScreenPosition(bandIndex);
    bandPopup.showAtPoint(pointPos, this);

    // APVTS-Attachments setzen
    bandPopup.setAttachments(audioProcessor.getAPVTS(), bandIndex);
}

//==============================================================================
// Analyzer-Controls Setup (Pro-Q Style)
//==============================================================================

void AuraAudioProcessorEditor::setupAnalyzerControls()
{
    // Resolution ComboBox
    analyzerResolutionCombo.addItemList(ParameterIDs::getAnalyzerResolutionNames(), 1);
    analyzerResolutionCombo.setTooltip("FFT Resolution\nHigher values = more accurate representation of low frequencies,\nbut slower reaction time.\nLower values = faster response, less detail.");
    addAndMakeVisible(analyzerResolutionCombo);

    analyzerResolutionCombo.onChange = [this]()
    {
        int idx = analyzerResolutionCombo.getSelectedId() - 1;
        FFTAnalyzer::FFTResolution res;
        switch (idx)
        {
            case 0: res = FFTAnalyzer::FFTResolution::Low; break;
            case 1: res = FFTAnalyzer::FFTResolution::Medium; break;
            case 2: res = FFTAnalyzer::FFTResolution::High; break;
            case 3: res = FFTAnalyzer::FFTResolution::Maximum; break;
            default: res = FFTAnalyzer::FFTResolution::Medium; break;
        }
        audioProcessor.getPreAnalyzer().setResolution(res);
        audioProcessor.getPostAnalyzer().setResolution(res);
    };
    
    // Setze nach dem onChange Handler
    analyzerResolutionCombo.setSelectedId(2, juce::dontSendNotification);

    // Range ComboBox
    analyzerRangeCombo.addItemList(ParameterIDs::getAnalyzerRangeNames(), 1);
    analyzerRangeCombo.setTooltip("Display Range (dB)\nDetermines the visible dynamic range of the analyzer.\nLarger ranges show more detail in quiet signals.");
    addAndMakeVisible(analyzerRangeCombo);

    analyzerRangeCombo.onChange = [this]()
    {
        int idx = analyzerRangeCombo.getSelectedId() - 1;
        SpectrumAnalyzer::DBRange range;
        switch (idx)
        {
            case 0: range = SpectrumAnalyzer::DBRange::Range60dB; break;
            case 1: range = SpectrumAnalyzer::DBRange::Range90dB; break;
            case 2: range = SpectrumAnalyzer::DBRange::Range120dB; break;
            default: range = SpectrumAnalyzer::DBRange::Range90dB; break;
        }
        spectrumAnalyzer.setDBRange(range);
    };
    
    // Setze nach dem onChange Handler
    analyzerRangeCombo.setSelectedId(2, juce::dontSendNotification);

    // Speed ComboBox
    analyzerSpeedCombo.addItemList(ParameterIDs::getAnalyzerSpeedNames(), 1);
    analyzerSpeedCombo.setTooltip("Analyzer Speed\nSlow = Smoother display, good for overview\nFast = More responsive, good for detail");
    addAndMakeVisible(analyzerSpeedCombo);

    analyzerSpeedCombo.onChange = [this]()
    {
        int idx = analyzerSpeedCombo.getSelectedId() - 1;
        FFTAnalyzer::AnalyzerSpeed speed;
        switch (idx)
        {
            case 0: speed = FFTAnalyzer::AnalyzerSpeed::VerySlow; break;
            case 1: speed = FFTAnalyzer::AnalyzerSpeed::Slow; break;
            case 2: speed = FFTAnalyzer::AnalyzerSpeed::Medium; break;
            case 3: speed = FFTAnalyzer::AnalyzerSpeed::Fast; break;
            case 4: speed = FFTAnalyzer::AnalyzerSpeed::VeryFast; break;
            default: speed = FFTAnalyzer::AnalyzerSpeed::Medium; break;
        }
        audioProcessor.getPreAnalyzer().setSpeed(speed);
        audioProcessor.getPostAnalyzer().setSpeed(speed);
    };
    
    // Setze nach dem onChange Handler
    analyzerSpeedCombo.setSelectedId(3, juce::dontSendNotification);

    // EQ Scale ComboBox (±6, ±12, ±24, ±36 dB)
    eqScaleCombo.addItem(juce::CharPointer_UTF8("+/-6 dB"), 1);
    eqScaleCombo.addItem(juce::CharPointer_UTF8("+/-12 dB"), 2);
    eqScaleCombo.addItem(juce::CharPointer_UTF8("+/-24 dB"), 3);
    eqScaleCombo.addItem(juce::CharPointer_UTF8("+/-36 dB"), 4);
    eqScaleCombo.setTooltip("EQ Gain Scale\nDetermines the maximum gain range of the EQ curve.\nSmaller ranges (e.g. +/-12 dB) show finer detail,\nlarger ranges (e.g. +/-36 dB) allow stronger adjustments.");
    addAndMakeVisible(eqScaleCombo);

    eqScaleCombo.onChange = [this]()
    {
        int idx = eqScaleCombo.getSelectedId() - 1;
        float range = 6.0f;
        switch (idx)
        {
            case 0: range = 6.0f; break;
            case 1: range = 12.0f; break;
            case 2: range = 24.0f; break;
            case 3: range = 36.0f; break;
            default: range = 36.0f; break;
        }
        // Beide Komponenten synchronisieren
        spectrumAnalyzer.setEQDecibelRange(-range, range);
        eqCurve.setEQDecibelRange(-range, range);
    };

    // Standard: ±36 dB
    eqScaleCombo.setSelectedId(4, juce::dontSendNotification);

    // Tilt-Funktion (v2.0: ausgeblendet, Code bleibt für spätere Reaktivierung)
    // Slider und Button werden im resized() unsichtbar gesetzt

    // Freeze Button
    analyzerFreezeButton.setButtonText("Freeze");
    analyzerFreezeButton.setClickingTogglesState(true);
    analyzerFreezeButton.setTooltip("Freeze Spectrum\nHolds the current analyzer display.\nUseful for analyzing a specific frequency snapshot in detail.");
    addAndMakeVisible(analyzerFreezeButton);

    analyzerFreezeButton.onClick = [this]()
    {
        bool frozen = analyzerFreezeButton.getToggleState();
        audioProcessor.getPreAnalyzer().setFrozen(frozen);
        audioProcessor.getPostAnalyzer().setFrozen(frozen);
    };

    // Peaks Button
    analyzerPeaksButton.setButtonText("Peaks");
    analyzerPeaksButton.setToggleState(true, juce::dontSendNotification);
    analyzerPeaksButton.setTooltip("Show Peak Markers\nDisplays the loudest frequency peaks with Hz values.\nHelps quickly identify dominant frequencies.");
    addAndMakeVisible(analyzerPeaksButton);

    analyzerPeaksButton.onClick = [this]()
    {
        spectrumAnalyzer.setShowPeakLabels(analyzerPeaksButton.getToggleState());
    };
    
    // Labels Button (für Smart EQ Highlight-Labels)
    showLabelsButton.setButtonText("Labels");
    showLabelsButton.setToggleState(false, juce::dontSendNotification);
    showLabelsButton.setTooltip("Show Frequency Region Labels\nDisplays descriptive labels for typical problem areas:\nRumble, Mud, Boxiness, Presence, Air etc.\nHelps with orientation in the frequency spectrum.");
    addAndMakeVisible(showLabelsButton);
    
    showLabelsButton.onClick = [this]()
    {
        bool showLabels = showLabelsButton.getToggleState();
        bool showOverlay = showLabels && smartModeButton.getToggleState();
        smartHighlightOverlay.setShowLabels(showLabels);
        smartHighlightOverlay.setEnabled(showOverlay);
        smartHighlightOverlay.setVisible(showOverlay);
        spectrumAnalyzer.setShowPeakLabels(showLabels);
        repaint();
    };

    {
        bool initLabels = showLabelsButton.getToggleState();
        bool initOverlay = initLabels && smartModeButton.getToggleState();
        smartHighlightOverlay.setShowLabels(initLabels);
        smartHighlightOverlay.setEnabled(initOverlay);
        smartHighlightOverlay.setVisible(initOverlay);
        spectrumAnalyzer.setShowPeakLabels(initLabels);
    }
    
    // Spektrum-Farbschema ComboBox
    for (int i = 0; i < static_cast<int>(CustomLookAndFeel::NumSchemes); ++i)
    {
        spectrumColorCombo.addItem(
            CustomLookAndFeel::getSpectrumColorSchemeName(
                static_cast<CustomLookAndFeel::SpectrumColorScheme>(i)), i + 1);
    }
    spectrumColorCombo.setTooltip("Spectrum Color Scheme\nChoose different color combinations\nfor input and output spectrum.");
    addAndMakeVisible(spectrumColorCombo);
    
    spectrumColorCombo.onChange = [this]()
    {
        int idx = spectrumColorCombo.getSelectedId() - 1;
        auto scheme = static_cast<CustomLookAndFeel::SpectrumColorScheme>(idx);
        CustomLookAndFeel::setSpectrumColorScheme(scheme);
        spectrumAnalyzer.repaint();
        
        // Speichere Auswahl
        juce::PropertiesFile::Options opts;
        opts.applicationName = "Aura";
        opts.filenameSuffix = ".settings";
        opts.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        juce::PropertiesFile settings(opts);
        settings.setValue("spectrumColorScheme", idx);
        settings.save();
    };
    
    // Lade gespeichertes Farbschema
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName = "Aura";
        opts.filenameSuffix = ".settings";
        opts.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        juce::PropertiesFile settings(opts);
        int savedScheme = settings.getIntValue("spectrumColorScheme", 0);
        if (savedScheme >= 0 && savedScheme < static_cast<int>(CustomLookAndFeel::NumSchemes))
        {
            CustomLookAndFeel::setSpectrumColorScheme(
                static_cast<CustomLookAndFeel::SpectrumColorScheme>(savedScheme));
            spectrumColorCombo.setSelectedId(savedScheme + 1, juce::dontSendNotification);
        }
        else
        {
            spectrumColorCombo.setSelectedId(1, juce::dontSendNotification);
        }
    }
}

void AuraAudioProcessorEditor::updateAnalyzerSettings()
{
    // Aktualisiere Analyzer-Settings aus den Controls
    // (wird bei Bedarf aufgerufen)
}
//==============================================================================
// Fenster-Größen-Persistierung
//==============================================================================

bool AuraAudioProcessorEditor::loadWindowSize(int& width, int& height)
{
    try
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Aura";
        options.filenameSuffix = ".settings";
        options.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        
        juce::PropertiesFile settings(options);
        
        // Versuche, Größe zu laden
        int savedWidth = settings.getIntValue("window_width", -1);
        int savedHeight = settings.getIntValue("window_height", -1);
        
        if (savedWidth > 0 && savedHeight > 0)
        {
            // Überprüfe ob Größe innerhalb sinnvoller Grenzen liegt
            // v2.1 FIX: Grenzen konsistent mit setResizeLimits(1000, 550, 1800, 1000)
            if (savedWidth >= 800 && savedWidth <= 1800 && 
                savedHeight >= 550 && savedHeight <= 1000)
            {
                width = savedWidth;
                height = savedHeight;
                return true;
            }
        }
    }
    catch (const std::exception&)
    {
        // Fehler beim Laden - nutze Standard
    }
    
    return false;
}

void AuraAudioProcessorEditor::saveWindowSize(int width, int height)
{
    try
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Aura";
        options.filenameSuffix = ".settings";
        options.folderName = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("Aura").getFullPathName();
        
        juce::PropertiesFile settings(options);
        
        // v2.1 FIX: Grenzen konsistent mit setResizeLimits(1000, 550, 1800, 1000)
        if (width >= 800 && width <= 1800 && height >= 550 && height <= 1000)
        {
            settings.setValue("window_width", width);
            settings.setValue("window_height", height);
            settings.save();
        }
    }
    catch (const std::exception&)
    {
        // Fehler beim Speichern - ignorieren
    }
}

//==============================================================================
// Smart EQ Implementation
//==============================================================================

void AuraAudioProcessorEditor::setupSmartEQ()
{
    // Smart Mode Button
    smartModeButton.setButtonText("Smart EQ");
    smartModeButton.setClickingTogglesState(true);
    smartModeButton.setTooltip("Smart EQ Mode\nEnables AI-based spectrum analysis.\nAutomatically detects problem areas like resonances,\nmaskings and imbalances.\nGenerates EQ recommendations that can be applied per click.");
    addAndMakeVisible(smartModeButton);
    
    // Button mit Parameter verbinden (für State-Speicherung im VST3)
    smartModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(),
        ParameterIDs::SMART_MODE_ENABLED,
        smartModeButton
    );
    
    smartModeButton.onClick = [this]()
    {
        bool isActive = smartModeButton.getToggleState();
        // SmartAnalyzer wird jetzt im Processor über Parameter gesteuert
        bool showOverlay = isActive && showLabelsButton.getToggleState();
        smartHighlightOverlay.setEnabled(showOverlay);
        smartHighlightOverlay.setVisible(showOverlay);
        smartRecommendationPanel.setAnalysisEnabled(isActive);
        smartRecommendationPanel.setVisible(isActive);  // Panel ein-/ausblenden
        
        // Live SmartEQ Panel auch ein-/ausblenden
        if (liveSmartEQPanel != nullptr)
        {
            liveSmartEQPanel->setVisible(isActive);
        }
        
        if (!isActive)
        {
            smartHighlightOverlay.clearProblems();
            smartRecommendationPanel.clearRecommendations();
            
            // Live Auto-EQ Parameter auch deaktivieren
            if (auto* liveEqParam = audioProcessor.getAPVTS().getParameter(ParameterIDs::LIVE_SMART_EQ_ENABLED))
            {
                liveEqParam->setValueNotifyingHost(0.0f);
            }
        }
        
        // Layout neu berechnen um Platz für Panel zu schaffen
        resized();
    };
    
    // Smart Highlight Overlay (über dem Spectrum Analyzer)
    addAndMakeVisible(smartHighlightOverlay);
    smartHighlightOverlay.setFrequencyRange(20.0f, 20000.0f);
    smartHighlightOverlay.setOpacity(0.18f);
    smartHighlightOverlay.setDisplayMode(SmartHighlightOverlay::DisplayMode::Regions);
    smartHighlightOverlay.setVisible(false);
    
    // Callback wenn auf Problem geklickt wird
    smartHighlightOverlay.onProblemClicked = [this](const SmartAnalyzer::FrequencyProblem& problem)
    {
        // Empfehlung für dieses Problem finden und anwenden
        const auto& recs = smartEQRecommendation.getRecommendations();
        for (size_t i = 0; i < recs.size(); ++i)
        {
            if (std::abs(recs[i].frequency - problem.frequency) < 10.0f)
            {
                applySmartRecommendation(static_cast<int>(i));
                break;
            }
        }
    };
    
    // Smart Recommendation Panel (rechts neben Analyzer)
    addAndMakeVisible(smartRecommendationPanel);
    
    // v2.0: AI Explanation Engine verbinden
    smartRecommendationPanel.setAIExplanationEngine(&audioProcessor.getAIExplanationEngine());
    
    // Callbacks für das Panel
    smartRecommendationPanel.onEnableChanged = [this](bool enabled)
    {
        // Button-Zustand setzen (Attachment synchronisiert automatisch mit Parameter)
        smartModeButton.setToggleState(enabled, juce::sendNotification);
        bool showOverlay = enabled && showLabelsButton.getToggleState();
        smartHighlightOverlay.setEnabled(showOverlay);
        smartHighlightOverlay.setVisible(showOverlay);
    };
    
    smartRecommendationPanel.onApplyRecommendation = [this](int index)
    {
        applySmartRecommendation(index);
    };
    
    smartRecommendationPanel.onApplyAll = [this]()
    {
        applyAllSmartRecommendations();
    };
    
    smartRecommendationPanel.onSensitivityChanged = [this](float sensitivity)
    {
        audioProcessor.getSmartAnalyzer().setSensitivity(sensitivity);
    };
    
    // Callback wenn Panel eingeklappt/ausgeklappt wird - Layout aktualisieren
    smartRecommendationPanel.onCollapsedChanged = [this](bool /*collapsed*/)
    {
        resized();
    };
    
    // Initial-Zustand basierend auf Parameter (NICHT hardcoded false!)
    // ButtonAttachment hat bereits den Button-State vom Parameter synchronisiert.
    bool smartModeInitiallyEnabled = smartModeButton.getToggleState();
    smartHighlightOverlay.setEnabled(smartModeInitiallyEnabled);
    smartHighlightOverlay.setVisible(smartModeInitiallyEnabled);
    smartRecommendationPanel.setVisible(smartModeInitiallyEnabled);
    smartRecommendationPanel.setCollapsed(true);  // Standardmäßig eingeklappt
    
    // Live Auto-EQ Parameter explizit auf false setzen beim Start
    if (auto* liveEqParam = audioProcessor.getAPVTS().getParameter(ParameterIDs::LIVE_SMART_EQ_ENABLED))
    {
        liveEqParam->setValueNotifyingHost(0.0f);
    }
    
    // Live Smart EQ Panel erstellen
    liveSmartEQPanel = std::make_unique<LiveSmartEQPanel>(
        audioProcessor.getAPVTS(),
        audioProcessor.getLiveSmartEQ()
    );
    addAndMakeVisible(*liveSmartEQPanel);
    liveSmartEQPanel->setVisible(smartModeInitiallyEnabled);  // Basierend auf Parameter
    liveSmartEQPanel->setCollapsed(true);  // Standardmäßig eingeklappt
    
    // v2.0: Detektoren verbinden für Status-Anzeige
    liveSmartEQPanel->setTransientDetector(&audioProcessor.getTransientDetector());
    liveSmartEQPanel->setTemporalPatternDetector(&audioProcessor.getTemporalPatternDetector());
    
    // Collapse-Callback für LiveSmartEQPanel
    liveSmartEQPanel->onCollapsedChanged = [this](bool /*collapsed*/)
    {
        resized();
    };
    
    // Reference-Verwendung-Callback für LiveSmartEQPanel
    liveSmartEQPanel->onUseReferenceChanged = [this](bool useReference)
    {
        auto& liveEQ = audioProcessor.getLiveSmartEQ();
        liveEQ.setUseReferenceAsTarget(useReference);
        
        auto& refPlayer = audioProcessor.getReferencePlayer();
        bool isLoaded = refPlayer.isLoaded();
        const auto& spectrum = refPlayer.getSpectrumMagnitudes();
        
        DBG("onUseReferenceChanged: useRef=" + juce::String(useReference ? "yes" : "no") +
            ", isLoaded=" + juce::String(isLoaded ? "yes" : "no") +
            ", spectrumSize=" + juce::String(spectrum.size()));
        
        if (useReference && isLoaded && !spectrum.empty())
        {
            // Reference-Spektrum mit loadReferenceForMatching laden
            liveEQ.loadReferenceForMatching(spectrum);
            DBG("Reference-Spektrum geladen: " + juce::String(spectrum.size()) + " bins");
        }
        else if (!useReference)
        {
            liveEQ.clearReferenceSpectrum();
            DBG("Reference-Spektrum geleert");
        }
        else
        {
            DBG("WARNUNG: Reference nicht verfügbar!");
        }
    };
    
    // Reference Track Panel erstellen
    referenceTrackPanel = std::make_unique<ReferenceTrackPanel>(
        audioProcessor.getReferencePlayer()
    );
    addAndMakeVisible(*referenceTrackPanel);
    referenceTrackPanel->setVisible(false);  // Initial versteckt
    
    // Reference-Panel Callbacks
    referenceTrackPanel->onSpectrumOverlayChanged = [this](bool enabled)
    {
        // Spectrum Overlay im Analyzer ein/ausschalten
        spectrumAnalyzer.setReferenceSpectrumEnabled(enabled);
        if (enabled && audioProcessor.getReferencePlayer().isLoaded())
        {
            spectrumAnalyzer.setReferenceSpectrum(audioProcessor.getReferencePlayer().getSpectrumMagnitudes());
        }
    };
    
    referenceTrackPanel->onABCompareChanged = [this](bool /*abMode*/)
    {
        // A/B Vergleich - könnte in Zukunft erweitert werden
        // Aktuell nur als Toggle
    };
    
    // Höhenänderung-Callback
    referenceTrackPanel->onHeightChanged = [this](int newHeight)
    {
        referencePanelHeight = newHeight;
        resized();
    };
    
    // NEU: Match EQ Button Callback
    referenceTrackPanel->onMatchEQClicked = [this]()
    {
        if (audioProcessor.getReferencePlayer().isLoaded())
        {
            // Reference-Spektrum für Matching laden
            audioProcessor.loadReferenceForMatching(
                audioProcessor.getReferencePlayer().getSpectrumMagnitudes());
            
            // Matching aktivieren
            audioProcessor.applyEQMatch(true);
            
            // Benutzer informieren
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Match EQ",
                "EQ-Matching wurde aktiviert!\n\nDas Plugin analysiert jetzt den Input und passt den EQ automatisch an das Reference-Spektrum an.",
                "OK");
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Match EQ",
                "Bitte erst eine Reference-Datei laden!",
                "OK");
        }
    };
    
    // NEU: Match Strength Callback
    referenceTrackPanel->onMatchStrengthChanged = [this](float strength)
    {
        audioProcessor.getLiveSmartEQ().setMatchStrength(strength);
    };
    
    // Callback wenn Reference-Datei geladen wird - LiveSmartEQ aktualisieren
    audioProcessor.getReferencePlayer().onFileLoaded = [this](const juce::File&)
    {
        // LiveSmartEQPanel über verfügbare Reference informieren
        if (liveSmartEQPanel)
        {
            liveSmartEQPanel->setReferenceAvailable(true);
            
            // Falls "Use Reference" bereits aktiviert, Spektrum mit SpectralMatcher laden
            if (liveSmartEQPanel->isUsingReference())
            {
                // WICHTIG: loadReferenceForMatching statt setReferenceSpectrum nutzen!
                audioProcessor.getLiveSmartEQ().loadReferenceForMatching(
                    audioProcessor.getReferencePlayer().getSpectrumMagnitudes());
                
                DBG("Reference-Spektrum für Matching geladen: " + 
                    juce::String(audioProcessor.getReferencePlayer().getSpectrumMagnitudes().size()) + " bins");
            }
        }
    };
    
    audioProcessor.getReferencePlayer().onFileUnloaded = [this]()
    {
        if (liveSmartEQPanel)
        {
            liveSmartEQPanel->setReferenceAvailable(false);
            audioProcessor.getLiveSmartEQ().clearReferenceSpectrum();
        }
    };
    
    // Reference-Button für Header
    referenceButton.setButtonText("Ref");
    referenceButton.setClickingTogglesState(true);
    referenceButton.setTooltip("Reference Track\nLoad a reference song to compare the frequency spectrum\nof your track with a professional reference.\nThe Spectral Matcher can automatically align the differences.");
    referenceButton.onClick = [this]()
    {
        showReferencePanel = referenceButton.getToggleState();
        referenceTrackPanel->setVisible(showReferencePanel);
        resized();
    };
    addAndMakeVisible(referenceButton);
    
    // WebSearch Panel
    webSearchPanel = std::make_unique<WebSearchPanel>(
        audioProcessor.getWebSearchEngine()
    );
    addAndMakeVisible(*webSearchPanel);
    webSearchPanel->setVisible(false);
    
    webSearchButton.setButtonText("Web");
    webSearchButton.setClickingTogglesState(true);
    webSearchButton.setTooltip("Web Search\nSearch the web for mixing tips, frequency information,\nand audio production knowledge.");
    webSearchButton.onClick = [this]()
    {
        showWebSearchPanel = webSearchButton.getToggleState();
        webSearchPanel->setVisible(showWebSearchPanel);
        resized();
    };
    addAndMakeVisible(webSearchButton);
}

void AuraAudioProcessorEditor::updateSmartAnalysis()
{
    if (!smartModeButton.getToggleState())
        return;
    
    auto& smartAnalyzer = audioProcessor.getSmartAnalyzer();
    
    // HINWEIS: analyze() wird bereits im Audio-Thread (processBlock) aufgerufen.
    // Hier nur die Ergebnisse lesen - kein doppelter Aufruf!
    
    // Audio-Level Check: Keine Empfehlungen ohne Signal
    // getOutputLevelLeft/Right liefert dB-Werte (z.B. -20.0 dB)
    float audioLevelDB = std::max(audioProcessor.getOutputLevelLeft(), audioProcessor.getOutputLevelRight());
    if (audioLevelDB < -80.0f)
    {
        // Kein Audio — Probleme und Empfehlungen leeren
        smartHighlightOverlay.updateProblems({});
        smartRecommendationPanel.updateRecommendations({});
        return;
    }
    
    // B.4: SmartAnalyzer-Referenz sicherstellen (für Confidence Heat-Map Rendering)
    smartHighlightOverlay.setSmartAnalyzer(&smartAnalyzer);

    // Overlay aktualisieren
    smartHighlightOverlay.updateProblems(smartAnalyzer.getDetectedProblems());

    // D.2: Band-Konflikt-Warnung im EQ-Kurven-Bereich aktualisieren
    eqCurve.setBandConflictCount(audioProcessor.getBandConflictCount());

    // Empfehlungen generieren
    smartEQRecommendation.updateRecommendations(smartAnalyzer, audioProcessor.getEQProcessor());
    smartRecommendationPanel.updateRecommendations(smartEQRecommendation.getRecommendations());

    // B.1: Delta-Correction-Curve: LiveSmartEQ → SpectrumAnalyzer (grüne Kurve)
    auto& liveEQ = audioProcessor.getLiveSmartEQ();
    if (liveEQ.isEnabled())
    {
        if (liveEQ.isCorrectionCurveDirty())
        {
            const auto& curve = liveEQ.getCorrectionCurve();
            std::vector<float> curveVec(curve.begin(), curve.end());
            spectrumAnalyzer.setMatchCurve(curveVec);
            spectrumAnalyzer.setMatchCurveEnabled(true);
            liveEQ.clearCorrectionCurveDirty();
        }
    }
    else
    {
        spectrumAnalyzer.setMatchCurveEnabled(false);
    }
}

void AuraAudioProcessorEditor::applySmartRecommendation(int index)
{
    if (smartEQRecommendation.applyRecommendation(index, audioProcessor.getEQProcessor(), 
                                                   audioProcessor.getAPVTS()))
    {
        // GUI aktualisieren
        updateFromProcessor();
        updateBandControlsDisplay();
        
        // Panel aktualisieren um applied-Status zu zeigen
        smartRecommendationPanel.updateRecommendations(smartEQRecommendation.getRecommendations());
    }
}

void AuraAudioProcessorEditor::applyAllSmartRecommendations()
{
    int appliedCount = smartEQRecommendation.applyAllRecommendations(
        audioProcessor.getEQProcessor(), audioProcessor.getAPVTS());
    
    if (appliedCount > 0)
    {
        // GUI aktualisieren
        updateFromProcessor();
        updateBandControlsDisplay();
        
        // Panel aktualisieren
        smartRecommendationPanel.updateRecommendations(smartEQRecommendation.getRecommendations());
        
        // Kurze Bestätigung
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Smart EQ",
            juce::String(appliedCount) + " recommendation(s) applied.",
            "OK");
    }
}

// ============================================================================
// Lizenz-System: Trial-Banner und Dialog
// ============================================================================

void AuraAudioProcessorEditor::updateTrialBanner()
{
    auto& lm = LicenseManager::getInstance();
    lm.refreshCachedStatus();
    auto status = lm.getCachedLicenseStatus();
    
    switch (status)
    {
        case LicenseManager::LicenseStatus::Licensed:
            trialBannerLabel.setVisible(false);
            break;
            
        case LicenseManager::LicenseStatus::Trial:
        {
            int days = lm.getTrialDaysRemaining();
            trialBannerLabel.setVisible(true);
            trialBannerLabel.setText(
                juce::String::formatted("TESTVERSION - %d Tag%s verbleibend  |  Klicken zum Aktivieren",
                                        days, days == 1 ? "" : "e"),
                juce::dontSendNotification);
            trialBannerLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xffe6b800));
            trialBannerLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1a1a1a));
            break;
        }
            
        case LicenseManager::LicenseStatus::TrialExpired:
            trialBannerLabel.setVisible(true);
            trialBannerLabel.setText(
                "TESTVERSION ABGELAUFEN - Audio eingeschraenkt  |  Klicken zum Aktivieren",
                juce::dontSendNotification);
            trialBannerLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xffcc2222));
            trialBannerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            break;
            
        default:
            trialBannerLabel.setVisible(true);
            trialBannerLabel.setText("Nicht lizenziert  |  Klicken zum Aktivieren",
                                     juce::dontSendNotification);
            trialBannerLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xffcc2222));
            trialBannerLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            break;
    }
    
    // Lizenz-Button Text aktualisieren
    if (status == LicenseManager::LicenseStatus::Licensed)
        licenseButton.setButtonText("Lizenz");
    else if (status == LicenseManager::LicenseStatus::Trial)
        licenseButton.setButtonText("Trial");
    else
        licenseButton.setButtonText("Lizenz!");
    
    repaint();
}

void AuraAudioProcessorEditor::showLicenseDialog()
{
    // Falls bereits offen, nach vorne bringen
    if (licenseDialogWindow != nullptr && licenseDialogWindow->isVisible())
    {
        licenseDialogWindow->toFront(true);
        return;
    }
    
    licenseDialogWindow = std::make_unique<LicenseDialogWindow>();
    
    licenseDialogWindow->onLicenseActivated = [this]()
    {
        // Banner und Button-Beschriftung aktualisieren
        updateTrialBanner();
        resized();
        repaint();
    };
    
    licenseDialogWindow->onDialogClosed = [this]()
    {
        // Aufraumen nach Schliessen (Timer, damit kein delete waehrend Callback)
        auto safeForClose = juce::Component::SafePointer<AuraAudioProcessorEditor>(this);
        juce::Timer::callAfterDelay(100, [safeForClose]() {
            if (safeForClose != nullptr)
                safeForClose->licenseDialogWindow.reset();
        });
    };
}

// ============================================================================
// Update-System
// ============================================================================

void AuraAudioProcessorEditor::updateCheckCompleted(const UpdateChecker::UpdateInfo& info)
{
    DBG("Update-Check Ergebnis: " + info.latestVersion 
        + " (Update verfuegbar: " + juce::String(info.updateAvailable ? "Ja" : "Nein") + ")");
    
    updateBanner.showUpdate(info);
    
    if (info.updateAvailable)
        resized();  // Layout neu berechnen damit Banner Platz hat
}

void AuraAudioProcessorEditor::showUpdateDialog(const UpdateChecker::UpdateInfo& info)
{
    if (updateDialogWindow != nullptr && updateDialogWindow->isVisible())
    {
        updateDialogWindow->toFront(true);
        return;
    }
    
    updateDialogWindow = std::make_unique<UpdateDialogWindow>(info, updateChecker);
}
