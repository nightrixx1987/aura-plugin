#include "PluginProcessor.h"
#include "PluginEditor.h"

AuraAudioProcessorEditor::AuraAudioProcessorEditor(AuraAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      updateTimer(*this),
      spectrumGrabTool(audioProcessor.getEQProcessor())
{
    // GPU-beschleunigtes Rendering aktivieren (beschleunigt Spektrum-Darstellung erheblich)
    openGLContext.setComponentPaintingEnabled(true);
    openGLContext.setContinuousRepainting(false);
    openGLContext.attachTo(*this);
    
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
        addAndMakeVisible(spectrumAnalyzer);
        
        // EQ-Kurve
        eqCurve.setEQProcessor(&audioProcessor.getEQProcessor());
        eqCurve.addListener(this);
        addAndMakeVisible(eqCurve);
        
        // Grab Tool
        addAndMakeVisible(spectrumGrabTool);
        
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
        
        // NEU: System Audio Button (nur Standalone)
        systemAudioButton.setButtonText("Sys Audio");
        systemAudioButton.setClickingTogglesState(true);
        systemAudioButton.setTooltip("System Audio Capture (WASAPI Loopback)\nNimmt den gesamten Windows Audio Output auf und zeigt ihn im Analyzer an.\nSo kannst du z.B. Spotify oder YouTube analysieren und EQ-Einstellungen vornehmen.\n\nACHTUNG: Der Plugin-Output wird stumm geschaltet, um Feedback zu vermeiden!");
        systemAudioButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00aa88));
        systemAudioButton.onClick = [this]()
        {
            bool enabled = systemAudioButton.getToggleState();
            auto& capture = audioProcessor.getSystemAudioCapture();
            
            if (enabled)
            {
                if (capture.startCapture())
                {
                    auto options = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::InfoIcon)
                        .withTitle("System Audio Capture")
                        .withMessage("Windows Audio wird jetzt analysiert!\n\n"
                                     "WICHTIG: Der Output von Aura ist STUMM geschaltet,\n"
                                     "um Feedback/Rueckkopplung zu vermeiden.\n\n"
                                     "Der Original-Sound kommt weiterhin aus Windows.\n"
                                     "Du siehst das Spektrum und kannst EQ-Einstellungen machen.")
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
            }
        };
        addAndMakeVisible(systemAudioButton);
        
        // Theme-Listener registrieren
        ThemeManager::getInstance().onThemeChanged = [this](ThemeManager::ThemeID /*id*/)
        {
            customLookAndFeel.updateColors();
            repaint();
            
            // Trigger repaint für alle sichtbaren Komponenten
            for (int i = 0; i < getNumChildComponents(); ++i)
            {
                if (auto* child = getChildComponent(i))
                    child->repaint();
            }
        };
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
    resetButton.setTooltip("Alle EQ-Baender zuruecksetzen\nSetzt Frequenz, Gain, Q-Faktor und Filtertyp\naller Baender auf die Standardwerte zurueck.");
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
    undoButton.setTooltip("Rueckgaengig (Strg+Z)\nMacht die letzte Parameter-Aenderung rueckgaengig.");
    undoButton.onClick = [this]() { audioProcessor.getUndoManager().undo(); };
    addAndMakeVisible(undoButton);
    
    redoButton.setButtonText(juce::CharPointer_UTF8("\xe2\x86\xaa"));  // ↪ Redo Symbol
    redoButton.setTooltip("Wiederherstellen (Strg+Y)\nStellt eine rueckgaengig gemachte Aenderung wieder her.");
    redoButton.onClick = [this]() { audioProcessor.getUndoManager().redo(); };
    addAndMakeVisible(redoButton);

    // Preset-Component
    presetComponent.addListener(this);
    addAndMakeVisible(presetComponent);
    
    // Output-Controls
    setupOutputControls();
    
    // Analyzer-Controls
    setupAnalyzerControls();
    
    // Smart EQ Setup
    setupSmartEQ();
    
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
    auto licStatus = LicenseManager::getInstance().getLicenseStatus();
    if (licStatus == LicenseManager::LicenseStatus::TrialExpired)
    {
        // Dialog nach kurzem Delay oeffnen (damit GUI erst fertig aufgebaut ist)
        auto safePtrForTrial = juce::Component::SafePointer<AuraAudioProcessorEditor>(this);
        juce::Timer::callAfterDelay(500, [safePtrForTrial]() {
            if (safePtrForTrial != nullptr)
                safePtrForTrial->showLicenseDialog();
        });
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
    openGLContext.detach();
    
    // Lizenz-Dialog schliessen falls offen
    licenseDialogWindow.reset();
    
    // Listener entfernen
    eqCurve.removeListener(this);
    bandControls.removeListener(this);
    bandPopup.removeListener(this);
    setLookAndFeel(nullptr);
}

void AuraAudioProcessorEditor::setupOutputControls()
{
    // Input Gain Slider
    inputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    inputGainSlider.setRange(-24.0f, 24.0f, 0.1f);
    inputGainSlider.setValue(0.0f);
    inputGainSlider.setTooltip("Input Gain (-24 bis +24 dB)\nRegelt die Eingangslautstaerke vor dem EQ.\nNuetzlich um Headroom zu schaffen oder leise Signale anzuheben.");
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
    outputGainSlider.setTooltip("Output Gain (-24 bis +24 dB)\nRegelt die Ausgangslautstaerke nach dem EQ.\nVerwende dies um Lautstaerkeunterschiede nach dem EQ auszugleichen.");
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("Output", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::OUTPUT_GAIN, outputGainSlider);

    // Linear Phase Button (noch nicht implementiert - als "coming soon" markiert)
    linearPhaseButton.setButtonText("Lin Phase");
    linearPhaseButton.setClickingTogglesState(true);
    linearPhaseButton.setTooltip("Linear Phase Modus (Coming Soon)\nVerarbeitet das Signal ohne Phasenverschiebungen.\nIdeal fuer transparentes Mastering - erfordert FFT-basierte Verarbeitung.");
    linearPhaseButton.setEnabled(false);  // Deaktiviert bis Implementierung fertig
    linearPhaseButton.setAlpha(0.4f);
    addAndMakeVisible(linearPhaseButton);

    linearPhaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::LINEAR_PHASE_MODE, linearPhaseButton);

    // Mid/Side Button
    midSideButton.setButtonText("M/S");
    midSideButton.setClickingTogglesState(true);
    midSideButton.setTooltip("Mid/Side Modus\nAktiviert Mid/Side-Verarbeitung statt Stereo (L/R).\nMid = Mitte des Stereofeldes, Side = Seiten.\nErmoeglicht gezieltes EQ-ing von Mono- und Stereo-Anteilen.");
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
    analyzerButton.setTooltip("Spektrum-Analyzer ein/ausschalten\nZeigt das Frequenzspektrum des Audio-Signals in Echtzeit an.");
    addAndMakeVisible(analyzerButton);

    analyzerOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::ANALYZER_ON, analyzerButton);

    // Analyzer Mode ComboBox
    analyzerModeCombo.addItem("Pre", 1);
    analyzerModeCombo.addItem("Post", 2);
    analyzerModeCombo.addItem("Both", 3);
    analyzerModeCombo.setTooltip("Analyzer-Modus\nPre = Signal vor dem EQ\nPost = Signal nach dem EQ\nBoth = Beide gleichzeitig anzeigen");
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
    grabModeButton.setTooltip("Spectrum Grab Modus\nKlicke auf Spitzen im Spektrum um automatisch ein EQ-Band\nan dieser Frequenz mit passendem Gain und Q zu erstellen.");
    addAndMakeVisible(grabModeButton);
    
    grabModeButton.onClick = [this]()
    {
        bool isActive = grabModeButton.getToggleState();
        spectrumGrabTool.setGrabMode(isActive);
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
    wetDrySlider.setTooltip("Wet/Dry Mix (Parallel Processing)\n100% = Nur das EQ-Signal hoeren\n0% = Nur das Originalsignal (Bypass)\nWerte dazwischen mischen Original und EQ-Signal.\nIdeal fuer subtile, parallele EQ-Bearbeitung.");
    addAndMakeVisible(wetDrySlider);
    
    wetDryLabel.setText("Mix", juce::dontSendNotification);
    wetDryLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wetDryLabel);
    
    wetDryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::WET_DRY_MIX, wetDrySlider);
    
    // NEU: Delta-Modus Button
    deltaButton.setButtonText("Delta");
    deltaButton.setClickingTogglesState(true);
    deltaButton.setTooltip("Delta Modus (Differenz-Signal)\nSpielt nur den Unterschied zwischen Original und EQ-Signal ab.\nSo hoerst du exakt, was der EQ veraendert.\nPerfekt zum Aufspueren von Resonanzen und Problemen.");
    deltaButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffdd6633));
    addAndMakeVisible(deltaButton);
    
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::DELTA_MODE, deltaButton);
    
    // NEU: Oversampling ComboBox
    oversamplingCombo.addItem("OS: Off", 1);
    oversamplingCombo.addItem("OS: 2x", 2);
    oversamplingCombo.addItem("OS: 4x", 3);
    oversamplingCombo.setTooltip("Oversampling-Faktor\nOff = Kein Oversampling (niedrigste CPU-Last)\n2x = Doppelte Samplerate (gute Qualitaet)\n4x = Vierfache Samplerate (beste Qualitaet)\n\nReduziert Aliasing-Artefakte bei hohen Frequenzen.\nHoehere Werte = bessere Klangqualitaet, aber mehr CPU-Last.");
    addAndMakeVisible(oversamplingCombo);
    
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::OVERSAMPLING_FACTOR, oversamplingCombo);
    
    // NEU: Resonance Suppressor Button
    suppressorButton.setButtonText("Soothe");
    suppressorButton.setClickingTogglesState(true);
    suppressorButton.setTooltip("Resonance Suppressor (Soothe-aehnlich)\nErkennt und unterdrueckt automatisch stoerende Resonanzen\nim Frequenzspektrum. Arbeitet dynamisch in 16 Baendern.\n\nIdeal gegen harsche Vocals, nervige Raumresonanzen\noder unangenehme Spitzen in Instrumenten.");
    suppressorButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff8844cc));
    addAndMakeVisible(suppressorButton);
    
    suppressorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(), ParameterIDs::SUPPRESSOR_ENABLED, suppressorButton);
    
    // NEU: Piano Roll Overlay
    addAndMakeVisible(pianoRollOverlay);
    pianoRollOverlay.setEnabled(false);
    
    pianoRollButton.setButtonText("Notes");
    pianoRollButton.setClickingTogglesState(true);
    pianoRollButton.setTooltip("Piano Roll / Noten-Overlay\nBlendet musikalische Noten (C0-C10) ueber dem Analyzer ein.\nHilft dabei, EQ-Einstellungen auf bestimmte Toene\nund Instrumente abzustimmen.");
    pianoRollButton.onClick = [this]()
    {
        pianoRollOverlay.setEnabled(pianoRollButton.getToggleState());
    };
    addAndMakeVisible(pianoRollButton);
}

void AuraAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(CustomLookAndFeel::getBackgroundDark());

    // Header-Bereich: 2 Zeilen (90 Pixel gesamt)
    g.setColour(CustomLookAndFeel::getBackgroundMid());
    g.fillRect(0, 0, getWidth(), 90);
    
    // Trennlinie zwischen Zeile 1 und 2
    g.setColour(CustomLookAndFeel::getBackgroundDark().withAlpha(0.5f));
    g.fillRect(0, 55, getWidth(), 1);

    // Plugin-Name
    g.setColour(CustomLookAndFeel::getTextColor());
    g.setFont(juce::Font(juce::FontOptions(20.0f).withStyle("Bold")));
    g.drawText("Aura", 15, 8, 100, 24, juce::Justification::left);

    // Version
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(CustomLookAndFeel::getTextColor().withAlpha(0.5f));
    g.drawText("v1.0", 115, 13, 40, 16, juce::Justification::left);
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

    if (key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        um.undo();
        return true;
    }
    if (key == juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0) ||
        key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        um.redo();
        return true;
    }
    return false;
}

void AuraAudioProcessorEditor::resized()
{
    // Undo/Redo mit Ctrl+Z / Ctrl+Y
    setWantsKeyboardFocus(true);
    
    // Speichere aktuelle Fenstergröße (mit Debounce um Disk-I/O zu reduzieren)
    pendingSaveWidth = getWidth();
    pendingSaveHeight = getHeight();
    if (!saveWindowSizeTimer.isTimerRunning())
        saveWindowSizeTimer.startTimer(500);  // 500ms Debounce
    
    auto bounds = getLocalBounds();
    
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
    
    // Preset-Component links
    presetComponent.setBounds(row1.removeFromLeft(250).reduced(5));
    
    // Lizenz-Button + SysAudio + Update-Banner + Theme Selector (oben rechts)
    auto rightRow1 = row1.removeFromRight(updateBanner.isVisible() ? 530 : 350).reduced(5, 8);
    licenseButton.setBounds(rightRow1.removeFromLeft(55));
    rightRow1.removeFromLeft(5);
    systemAudioButton.setBounds(rightRow1.removeFromLeft(70));
    rightRow1.removeFromLeft(8);
    
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
    auto row2 = fullHeaderArea.reduced(8, 2);
    const int gap = 6;  // Abstand zwischen Buttons
    
    // Links: Processing-Buttons
    undoButton.setBounds(row2.removeFromLeft(28).reduced(0, 2));
    row2.removeFromLeft(2);
    redoButton.setBounds(row2.removeFromLeft(28).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    resetButton.setBounds(row2.removeFromLeft(50).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    oversamplingCombo.setBounds(row2.removeFromLeft(72).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    deltaButton.setBounds(row2.removeFromLeft(58).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    suppressorButton.setBounds(row2.removeFromLeft(62).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    smartModeButton.setBounds(row2.removeFromLeft(78).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    referenceButton.setBounds(row2.removeFromLeft(42).reduced(0, 2));
    row2.removeFromLeft(gap);
    
    grabModeButton.setBounds(row2.removeFromLeft(52).reduced(0, 2));
    row2.removeFromLeft(gap + 4);  // Trennung zwischen Features und Analyzer
    
    // Rechts: Analyzer-Buttons (von rechts nach links platziert)
    linearPhaseButton.setBounds(row2.removeFromRight(90).reduced(0, 2));
    row2.removeFromRight(gap);
    
    midSideButton.setBounds(row2.removeFromRight(58).reduced(0, 2));
    row2.removeFromRight(gap);
    
    analyzerButton.setBounds(row2.removeFromRight(76).reduced(0, 2));
    row2.removeFromRight(gap);
    
    analyzerModeCombo.setBounds(row2.removeFromRight(70).reduced(0, 2));

    // ==========================================
    // Analyzer-Settings Panel (Pro-Q Style)
    // ==========================================
    auto analyzerSettingsArea = bounds.removeFromBottom(35);
    auto settingsRow = analyzerSettingsArea.reduced(5, 3);

    // Von links nach rechts: Resolution, Range, Speed, EQ Scale, Tilt, Freeze, Peaks
    analyzerResolutionCombo.setBounds(settingsRow.removeFromLeft(110));
    settingsRow.removeFromLeft(8);

    analyzerRangeCombo.setBounds(settingsRow.removeFromLeft(80));
    settingsRow.removeFromLeft(8);

    analyzerSpeedCombo.setBounds(settingsRow.removeFromLeft(90));
    settingsRow.removeFromLeft(8);

    eqScaleCombo.setBounds(settingsRow.removeFromLeft(85));
    settingsRow.removeFromLeft(15);

    analyzerTiltButton.setBounds(settingsRow.removeFromLeft(45));
    settingsRow.removeFromLeft(5);
    analyzerTiltSlider.setBounds(settingsRow.removeFromLeft(130));
    settingsRow.removeFromLeft(15);

    analyzerFreezeButton.setBounds(settingsRow.removeFromLeft(60));
    settingsRow.removeFromLeft(8);

    analyzerPeaksButton.setBounds(settingsRow.removeFromLeft(55));
    settingsRow.removeFromLeft(8);
    
    showLabelsButton.setBounds(settingsRow.removeFromLeft(70));
    settingsRow.removeFromLeft(8);
    
    // NEU: Piano Roll Button
    pianoRollButton.setBounds(settingsRow.removeFromLeft(55));
    settingsRow.removeFromLeft(8);
    
    // NEU: Spektrum-Farbschema
    spectrumColorCombo.setBounds(settingsRow.removeFromLeft(140));

    // Band-Controls unten (optimierte Größe)
    auto bandControlsArea = bounds.removeFromBottom(150);

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
    
    spectrumAnalyzer.setBounds(mainArea);
    eqCurve.setBounds(mainArea);
    spectrumGrabTool.setBounds(mainArea);
    smartHighlightOverlay.setBounds(mainArea);
    pianoRollOverlay.setBounds(mainArea);  // NEU: Piano Roll über Analyzer
}

void AuraAudioProcessorEditor::updateFromProcessor()
{
    auto& apvts = audioProcessor.getAPVTS();
    
    // NEU: Trial-Banner regelmaessig aktualisieren (~alle 5 Sekunden bei 25 FPS)
    {
        if (++bannerUpdateCounter >= 125)  // 25 FPS * 5s
        {
            bannerUpdateCounter = 0;
            updateTrialBanner();
        }
    }
    
    // Live Smart EQ: Pending Parameter-Änderungen im Message-Thread anwenden (RT-safe)
    auto& liveSmartEQ = audioProcessor.getLiveSmartEQ();
    liveSmartEQ.applyPendingParameterChanges(apvts);
    
    // Linear Phase EQ: Magnitude-Response aktualisieren (vom GUI-Thread)
    auto& linearPhaseEQ = audioProcessor.getLinearPhaseEQ();
    if (linearPhaseEQ.isEnabled())
    {
        linearPhaseEQ.updateMagnitudeResponse(audioProcessor.getEQProcessor());
    }
    
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
        auto* typeParam = apvts.getRawParameterValue(ParameterIDs::getBandTypeID(i));
        auto* bypassParam = apvts.getRawParameterValue(ParameterIDs::getBandBypassID(i));
        auto* activeParam = apvts.getRawParameterValue(ParameterIDs::getBandActiveID(i));
        
        if (freqParam == nullptr || gainParam == nullptr || qParam == nullptr ||
            typeParam == nullptr || bypassParam == nullptr || activeParam == nullptr)
            continue;
            
        float freq = freqParam->load();
        float gain = gainParam->load();
        float q = qParam->load();
        int type = static_cast<int>(typeParam->load());
        bool bypass = bypassParam->load() > 0.5f;
        bool active = activeParam->load() > 0.5f;

        eqCurve.setBandParameters(i, freq, gain, q,
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
        float audioLevel = std::max(outputLevelLeft, outputLevelRight);
        bool hasAudio = (audioLevel > 0.0001f);  // ca. -80 dB
        
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
}

void AuraAudioProcessorEditor::updateBandControlsDisplay()
{
    int selectedBand = eqCurve.getSelectedBand();

    if (selectedBand >= 0)
    {
        auto& apvts = audioProcessor.getAPVTS();

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

    // Alle Parameter des gelöschten Bandes auf Standard zurücksetzen
    if (auto* activeParam = apvts.getParameter(ParameterIDs::getBandActiveID(bandIndex)))
        activeParam->setValueNotifyingHost(0.0f);

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

    if (auto* slopeParam = apvts.getParameter(ParameterIDs::getBandSlopeID(bandIndex)))
        slopeParam->setValueNotifyingHost(slopeParam->convertTo0to1(12.0f));

    // Selection aufräumen wenn nötig
    if (bandControls.getCurrentBandIndex() == bandIndex)
        bandControls.clearSelection();

    updateFromProcessor();
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
    else if (parameterName == "bypass")
        paramID = ParameterIDs::getBandBypassID(bandIndex);

    if (paramID.isNotEmpty())
    {
        if (auto* param = apvts.getParameter(paramID))
        {
            param->setValueNotifyingHost(param->convertTo0to1(value));
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
    bandDeleted(bandIndex);
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
    analyzerResolutionCombo.setTooltip("FFT Aufloesung\nHoehere Werte = genauere Darstellung tiefer Frequenzen,\naber langsamere Reaktionszeit.\nNiedrigere Werte = schnellere Reaktion, weniger Detail.");
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
    analyzerRangeCombo.setTooltip("Anzeigebereich (dB)\nBestimmt den sichtbaren Dynamikumfang des Analyzers.\nGroessere Bereiche zeigen mehr Detail bei leisen Signalen.");
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
    analyzerSpeedCombo.setTooltip("Analyzer-Geschwindigkeit\nLangsam = Glattere Darstellung, gut fuer Uebersicht\nSchnell = Reaktionsfreudigere Anzeige, gut fuer Details");
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
    eqScaleCombo.setTooltip("EQ Gain-Skalierung\nBestimmt den maximalen Gain-Bereich der EQ-Kurve.\nKleinere Bereiche (z.B. +/-12 dB) zeigen feinere Details,\ngroessere Bereiche (z.B. +/-30 dB) erlauben staerkere Eingriffe.");
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

    // Tilt Slider
    analyzerTiltSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerTiltSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
    analyzerTiltSlider.setRange(ParameterIDs::MIN_ANALYZER_TILT, ParameterIDs::MAX_ANALYZER_TILT, 0.5f);
    analyzerTiltSlider.setValue(ParameterIDs::DEFAULT_ANALYZER_TILT);
    analyzerTiltSlider.setTextValueSuffix(" dB/oct");
    analyzerTiltSlider.setTooltip("Spektrum-Tilt Kompensation (dB/Oktave)\nGleicht die natuerliche Neigung des Frequenzspektrums aus.\nMusik hat typisch mehr Energie in den Tiefen - mit Tilt\nwird die Anzeige visuell ausgeglichener.");
    addAndMakeVisible(analyzerTiltSlider);

    analyzerTiltSlider.onValueChange = [this]()
    {
        float tilt = static_cast<float>(analyzerTiltSlider.getValue());
        audioProcessor.getPreAnalyzer().setTiltSlope(tilt);
        audioProcessor.getPostAnalyzer().setTiltSlope(tilt);
    };

    // Tilt Enable Button (Standard: OFF)
    analyzerTiltButton.setButtonText("Tilt");
    analyzerTiltButton.setToggleState(false, juce::dontSendNotification);
    analyzerTiltButton.setTooltip("Tilt-Kompensation aktivieren\nWendet die eingestellte Spektrum-Neigung auf die Analyzer-Anzeige an.");
    addAndMakeVisible(analyzerTiltButton);

    analyzerTiltButton.onClick = [this]()
    {
        bool enabled = analyzerTiltButton.getToggleState();
        audioProcessor.getPreAnalyzer().setTiltEnabled(enabled);
        audioProcessor.getPostAnalyzer().setTiltEnabled(enabled);
        analyzerTiltSlider.setEnabled(enabled);
    };
    
    // Tilt-Slider standardmäßig deaktiviert
    analyzerTiltSlider.setEnabled(false);

    // Freeze Button
    analyzerFreezeButton.setButtonText("Freeze");
    analyzerFreezeButton.setClickingTogglesState(true);
    analyzerFreezeButton.setTooltip("Spektrum einfrieren\nHaelt die aktuelle Analyzer-Anzeige an.\nNuetzlich um ein bestimmtes Frequenzbild in Ruhe zu analysieren.");
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
    analyzerPeaksButton.setTooltip("Spitzen-Markierungen anzeigen\nZeigt die lautesten Frequenzspitzen mit Hz-Wert im Analyzer an.\nHilft beim schnellen Identifizieren dominanter Frequenzen.");
    addAndMakeVisible(analyzerPeaksButton);

    analyzerPeaksButton.onClick = [this]()
    {
        spectrumAnalyzer.setShowPeakLabels(analyzerPeaksButton.getToggleState());
    };
    
    // Labels Button (für Smart EQ Highlight-Labels)
    showLabelsButton.setButtonText("Labels");
    showLabelsButton.setToggleState(true, juce::dontSendNotification);
    showLabelsButton.setTooltip("Frequenzbereich-Labels anzeigen\nBlendet beschreibende Labels fuer typische Problembereiche ein:\nRumble, Mud, Boxiness, Presence, Air usw.\nHilft bei der Orientierung im Frequenzspektrum.");
    addAndMakeVisible(showLabelsButton);
    
    showLabelsButton.onClick = [this]()
    {
        smartHighlightOverlay.setShowLabels(showLabelsButton.getToggleState());
    };
    
    // Spektrum-Farbschema ComboBox
    for (int i = 0; i < static_cast<int>(CustomLookAndFeel::NumSchemes); ++i)
    {
        spectrumColorCombo.addItem(
            CustomLookAndFeel::getSpectrumColorSchemeName(
                static_cast<CustomLookAndFeel::SpectrumColorScheme>(i)), i + 1);
    }
    spectrumColorCombo.setTooltip("Spektrum-Farbschema\nWaehle verschiedene Farbkombinationen\nfuer Input- und Output-Spektrum.");
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
            if (savedWidth >= 800 && savedWidth <= 1920 && 
                savedHeight >= 550 && savedHeight <= 1200)
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
        
        // Speichere nur wenn Größe im gültigen Bereich ist
        if (width >= 800 && width <= 1600 && height >= 550 && height <= 1000)
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
    smartModeButton.setTooltip("Smart EQ Modus\nAktiviert die KI-basierte Spektrum-Analyse.\nErkennt automatisch Problembereiche wie Resonanzen,\nMaskierungen und Ungleichgewichte.\nGeneriert EQ-Empfehlungen die per Klick angewendet werden koennen.");
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
        smartHighlightOverlay.setEnabled(isActive);
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
    smartHighlightOverlay.setOpacity(0.25f);
    smartHighlightOverlay.setDisplayMode(SmartHighlightOverlay::DisplayMode::Regions);
    
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
    
    // Callbacks für das Panel
    smartRecommendationPanel.onEnableChanged = [this](bool enabled)
    {
        // Button-Zustand setzen (Attachment synchronisiert automatisch mit Parameter)
        smartModeButton.setToggleState(enabled, juce::sendNotification);
        smartHighlightOverlay.setEnabled(enabled);
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
    
    // Initial-Zustand basierend auf Parameter
    bool smartModeInitiallyEnabled = false;  // Smart EQ standardmäßig aus
    smartModeButton.setToggleState(smartModeInitiallyEnabled, juce::dontSendNotification);
    smartHighlightOverlay.setEnabled(smartModeInitiallyEnabled);
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
    referenceButton.setTooltip("Reference Track\nLade einen Referenz-Song um das Frequenzspektrum\ndeines Tracks mit einer professionellen Referenz zu vergleichen.\nDer Spectral Matcher kann die Unterschiede automatisch angleichen.");
    referenceButton.onClick = [this]()
    {
        showReferencePanel = referenceButton.getToggleState();
        referenceTrackPanel->setVisible(showReferencePanel);
        resized();
    };
    addAndMakeVisible(referenceButton);
}

void AuraAudioProcessorEditor::updateSmartAnalysis()
{
    if (!smartModeButton.getToggleState())
        return;
    
    auto& smartAnalyzer = audioProcessor.getSmartAnalyzer();
    
    // HINWEIS: analyze() wird bereits im Audio-Thread (processBlock) aufgerufen.
    // Hier nur die Ergebnisse lesen - kein doppelter Aufruf!
    
    // Overlay aktualisieren
    smartHighlightOverlay.updateProblems(smartAnalyzer.getDetectedProblems());
    
    // Empfehlungen generieren
    smartEQRecommendation.updateRecommendations(smartAnalyzer, audioProcessor.getEQProcessor());
    smartRecommendationPanel.updateRecommendations(smartEQRecommendation.getRecommendations());
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
            juce::String(appliedCount) + " Empfehlung(en) angewendet.",
            "OK");
    }
}

// ============================================================================
// Lizenz-System: Trial-Banner und Dialog
// ============================================================================

void AuraAudioProcessorEditor::updateTrialBanner()
{
    auto& lm = LicenseManager::getInstance();
    auto status = lm.getLicenseStatus();
    
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
