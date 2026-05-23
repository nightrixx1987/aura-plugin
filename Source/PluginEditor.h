#pragma once

#include <JuceHeader.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "GUI/EQCurveComponent.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/BandControls.h"
#include "GUI/BandPopup.h"
#include "GUI/CustomLookAndFeel.h"
#include "GUI/PresetComponent.h"
#include "GUI/SpectrumGrabTool.h"
#include "GUI/LevelMeter.h"
#include "GUI/ThemeSelector.h"
#include "GUI/AuraSettingsPanel.h"
#include "GUI/SmartHighlightOverlay.h"
#include "GUI/SmartRecommendationPanel.h"
#include "GUI/LiveSmartEQPanel.h"
#include "GUI/ReferenceTrackPanel.h"
#include "GUI/AudioSourceSelector.h"
#include "GUI/PianoRollOverlay.h"
#include "GUI/SpectralDynamicsPanel.h"
#include "GUI/CrossChannelPanel.h"
#include "GUI/EQSketchTool.h"
#include "GUI/MultiReferencePanel.h"
#include "GUI/GenreMorphWidget.h"
#include "GUI/CustomProfilePanel.h"
#include "GUI/TutorialOverlay.h"
#include "GUI/WebSearchPanel.h"
#include "DSP/SmartEQRecommendation.h"
#include "Licensing/LicenseManager.h"
#include "Licensing/LicenseDialog.h"
#include "Utils/UpdateChecker.h"
#include "GUI/UpdateNotification.h"

/**
 * AuraAudioProcessorEditor: Haupt-GUI des Plugins.
 */
class AuraAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public EQCurveComponent::Listener,
                                   public BandControls::Listener,
                                   public BandPopup::Listener,
                                   public PresetComponent::Listener,
                                   public UpdateChecker::Listener
{
public:
    enum class ToolbarTab
    {
        Processing = 0,
        Smart,
        Analyzer
    };

    explicit AuraAudioProcessorEditor(AuraAudioProcessor&);
    ~AuraAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;

    // EQCurveComponent::Listener
    void bandParameterChanged(int bandIndex, float frequency, float gain, float q) override;
    void bandSelected(int bandIndex) override;
    void bandCreated(int bandIndex, float frequency) override;
    void filterTypeChanged(int bandIndex, ParameterIDs::FilterType type) override;
    void bandDeleted(int bandIndex) override;
    void bandRightClicked(int bandIndex) override;  // Rechtsklick -> Popup zeigen
    void bandAutoListenUpdate(int bandIndex, float freq, float q) override;
    void bandAutoListenStop() override;
    void sketchCompleted(const std::vector<EQSketchTool::GeneratedBand>& bands) override;

    // BandControls::Listener
    void bandControlChanged(int bandIndex, const juce::String& parameterName, float value) override;

    // BandPopup::Listener
    void bandPopupValueChanged(int bandIndex, const juce::String& parameterName, float value) override;
    void bandPopupDeleteRequested(int bandIndex) override;
    void bandPopupBypassChanged(int bandIndex, bool bypassed) override;

    // PresetComponent::Listener
    void presetSelected(const PresetManager::PresetData& preset) override;

private:
    AuraAudioProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    // Haupt-Komponenten
    SpectrumAnalyzer spectrumAnalyzer;
    EQCurveComponent eqCurve;
    BandControls bandControls;
    BandPopup bandPopup;
    PresetComponent presetComponent { &audioProcessor };
    SpectrumGrabTool spectrumGrabTool;
    LevelMeter levelMeter;  // Neue Pegel-Anzeige
    ThemeSelector themeSelector;  // Theme-Auswahl
    juce::TextButton licenseButton;  // Lizenz-Button
    juce::TextButton settingsButton;   // Einstellungen-Button
    juce::ToggleButton systemAudioButton;  // NEU: System Audio Capture Button
    juce::ComboBox sysAudioOutputCombo;    // NEU: Output-Device Routing für Sys Audio
    juce::Label sysAudioOutputLabel;       // Label für Output-Routing
    
    // Smart EQ Komponenten
    SmartHighlightOverlay smartHighlightOverlay;
    SmartRecommendationPanel smartRecommendationPanel;
    SmartEQRecommendation smartEQRecommendation;
    juce::ToggleButton smartModeButton;
    
    // Live Smart EQ Panel
    std::unique_ptr<LiveSmartEQPanel> liveSmartEQPanel;
    
    // Reference Track Panel
    std::unique_ptr<ReferenceTrackPanel> referenceTrackPanel;
    bool showReferencePanel = false;
    juce::ToggleButton referenceButton;  // Toggle für Reference-Panel

    // Globale Controls
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;
    juce::ComboBox phaseModeCombo;
    juce::ComboBox eqQualityCombo;
    juce::ComboBox characterModeCombo;
    juce::Slider phaseCrossoverSlider;
    juce::ToggleButton linearPhaseButton;
    juce::Label linearPhaseLabel;
    juce::ToggleButton midSideButton;
    juce::ToggleButton analyzerButton;
    juce::ToggleButton phaseButton;  // Phase-Anzeige Toggle
    juce::ComboBox analyzerModeCombo;
    juce::ToggleButton grabModeButton;
    juce::TextButton resetButton;  // Reset-Button für alle Bänder

    // Toolbar Tabs
    juce::TextButton processingTabButton;
    juce::TextButton smartTabButton;
    juce::TextButton analyzerTabButton;
    ToolbarTab currentToolbarTab_ = ToolbarTab::Processing;
    
    // Accordion Expand/Collapse State
    bool processingAdvancedExpanded_ = false;
    bool smartAdvancedExpanded_ = false;
    bool analyzerAdvancedExpanded_ = false;
    juce::TextButton processingExpandButton;
    juce::TextButton smartExpandButton;
    juce::TextButton analyzerExpandButton;
    
    // NEU: Undo/Redo Buttons
    juce::TextButton undoButton;
    juce::TextButton redoButton;
    
    // NEU: Wet/Dry Mix
    juce::Slider wetDrySlider;
    juce::Label wetDryLabel;
    
    // NEU: Delta-Modus Button
    juce::ToggleButton deltaButton;
    
    // NEU: Oversampling ComboBox
    juce::ComboBox oversamplingCombo;
    
    // NEU: Resonance Suppressor Controls
    juce::ToggleButton suppressorButton;
    
    // NEU: Piano Roll Overlay
    PianoRollOverlay pianoRollOverlay;
    juce::ToggleButton pianoRollButton;
    
    // ===== v2.0: Neue GUI Panels =====
    std::unique_ptr<SpectralDynamicsPanel> spectralDynamicsPanel;
    std::unique_ptr<CrossChannelPanel> crossChannelPanel;
    juce::ToggleButton spectralDynButton;   // Toggle für SpectralDynamics Panel
    juce::ToggleButton crossChannelButton;  // Toggle für CrossChannel Panel
    juce::ToggleButton eqSketchButton;      // Toggle für EQ Sketch Mode
    juce::ToggleButton genreMorphButton;    // Toggle für Genre Morph Widget
    juce::ToggleButton multiRefButton;      // Toggle für Multi-Reference Panel
    juce::ToggleButton tutorialButton;      // Toggle für Tutorial/Lernmodus
    std::unique_ptr<MultiReferencePanel> multiRefPanel;
    std::unique_ptr<GenreMorphWidget> genreMorphWidget;
    std::unique_ptr<CustomProfilePanel> customProfilePanel;
    TutorialOverlay tutorialOverlayPanel;
    bool showSpectralDynPanel = false;
    bool showCrossChannelPanel = false;
    bool showGenreMorphPanel = false;
    bool showMultiRefPanel = false;
    bool showTutorialOverlay = false;
    
    // WebSearch Panel
    std::unique_ptr<WebSearchPanel> webSearchPanel;
    juce::ToggleButton webSearchButton;
    bool showWebSearchPanel = false;
    
    // v2.0 FIX: Sketch-Schutz — verhindert sofortiges Überschreiben durch Genre Morph
    bool sketchActive_ = false;

    // NEU: Lizenz-Dialog Fenster
    std::unique_ptr<LicenseDialogWindow> licenseDialogWindow;
    
    // NEU: Update-System
    UpdateChecker updateChecker;
    UpdateNotificationBanner updateBanner;
    std::unique_ptr<UpdateDialogWindow> updateDialogWindow;
    void updateCheckCompleted(const UpdateChecker::UpdateInfo& info) override;
    void showUpdateDialog(const UpdateChecker::UpdateInfo& info);
    
    // NEU: Tooltip-Fenster (wird ben\u00f6tigt damit JUCE Tooltips anzeigt)
    juce::TooltipWindow tooltipWindow { this, 500 };  // 500ms Verz\u00f6gerung
    
    // NEU: Trial-Banner am unteren Rand
    juce::Label trialBannerLabel;
    int bannerUpdateCounter = 0;  // Member statt static (thread-safe bei mehreren Instanzen)
    int outputRoutingWatchdogCounter = 0;  // Watchdog für Output-Renderer Auto-Restart
public:
    void updateTrialBanner();
    void showLicenseDialog();
private:

    // Analyzer-Settings (Pro-Q Style)
    juce::ComboBox analyzerResolutionCombo;
    juce::ComboBox analyzerRangeCombo;
    juce::ComboBox analyzerSpeedCombo;
    juce::ComboBox eqScaleCombo;  // EQ Gain Scale Selector
    juce::Slider analyzerTiltSlider;
    juce::ToggleButton analyzerTiltButton;
    juce::ToggleButton analyzerFreezeButton;
    juce::ToggleButton analyzerPeaksButton;
    juce::ToggleButton showLabelsButton;  // Toggle für Smart EQ Labels
    juce::ComboBox spectrumColorCombo;  // Spektrum-Farbschema Auswahl
    
    // Reference Panel Höhe
    int referencePanelHeight = 130;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linearPhaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> eqQualityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> characterModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaseCrossoverAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> analyzerOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> analyzerModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> smartModeAttachment;
    
    // NEU: Attachments für neue Controls
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetDryAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> suppressorAttachment;

    // Analyzer-Settings Helper
    void setupAnalyzerControls();
    void updateAnalyzerSettings();

    // Timer für GUI-Updates
    class UpdateTimer : public juce::Timer
    {
    public:
        UpdateTimer(AuraAudioProcessorEditor& e) : editor(e) {}
        void timerCallback() override { editor.updateFromProcessor(); }
    private:
        AuraAudioProcessorEditor& editor;
    };
    UpdateTimer updateTimer;

    void updateFromProcessor();
    void setupOutputControls();
    void updateBandControlsDisplay();
    void syncPhaseModeControlsFromState();
    void applyPreset(const PresetManager::PresetData& preset);
    void applyToolbarTabVisibility();
    
    // NEU: Sys Audio Output-Routing
    void refreshSysAudioOutputDevices();
    
    // Smart EQ Methoden
    void setupSmartEQ();
    void updateSmartAnalysis();
    void applySmartRecommendation(int index);
    void applyAllSmartRecommendations();
    
    // Zeige Popup bei Rechtsklick auf Band
    void showBandPopup(int bandIndex);
    
    // Fenster-Größen-Persistierung
    bool loadWindowSize(int& width, int& height);
    void saveWindowSize(int width, int height);
    
    // Debounce-Timer für Fenster-Größe (vermeidet Disk-I/O bei jedem Resize-Frame)
    int pendingSaveWidth = 0;
    int pendingSaveHeight = 0;
    class SaveWindowSizeTimer : public juce::Timer
    {
    public:
        SaveWindowSizeTimer(AuraAudioProcessorEditor& e) : editor(e) {}
        void timerCallback() override
        {
            stopTimer();
            editor.saveWindowSize(editor.pendingSaveWidth, editor.pendingSaveHeight);
        }
    private:
        AuraAudioProcessorEditor& editor;
    };
    SaveWindowSizeTimer saveWindowSizeTimer { *this };
    
    // GPU-beschleunigtes Rendering (reduziert CPU-Last des Spektrum-Renderings erheblich)
    juce::OpenGLContext openGLContext;
    bool phaseModeControlSyncInProgress_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraAudioProcessorEditor)
};
