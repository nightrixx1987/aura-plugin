#pragma once

#include <JuceHeader.h>

/**
 * KeyboardShortcutManager: Verwaltung von Tastatur-Shortcuts für professionellen Workflow.
 * Pro-Q3-ähnliche Shortcuts für schnelle Bedienung.
 */
class KeyboardShortcutManager
{
public:
    enum class ShortcutAction
    {
        // Band-Management
        CreateBandAtCursor,      // D
        DeleteSelectedBand,      // Backspace
        DuplicateBand,          // Ctrl+D
        CopyBand,               // Ctrl+C
        PasteBand,              // Ctrl+V
        
        // Navigation
        NextBand,               // Tab oder Pfeil rechts
        PreviousBand,           // Shift+Tab oder Pfeil links
        ToggleBypass,           // B
        ToggleBandActive,       // Space
        
        // UI-Modi
        ToggleAnalyzer,         // A
        ToggleLinearPhase,      // L
        ResetAllBands,          // Ctrl+R
        ResetSelectedBand,      // R
        
        // Schnelle Parameter
        IncreaseGain,           // Pfeil hoch
        DecreaseGain,           // Pfeil unten
        IncreaseQ,              // Shift+Pfeil hoch
        DecreaseQ,              // Shift+Pfeil unten
        
        // Undo/Redo
        Undo,                   // Ctrl+Z
        Redo,                   // Ctrl+Y / Ctrl+Shift+Z
        
        // Presets
        PreviousPreset,         // Ctrl+[
        NextPreset,             // Ctrl+]
        SavePreset,             // Ctrl+S
        
        NumActions
    };

    using ActionCallback = std::function<void(ShortcutAction)>;

    KeyboardShortcutManager();

    // Callback registrieren
    void onShortcutAction(ActionCallback callback) { actionCallback = callback; }

    // Tastendruck verarbeiten
    bool processKeyPress(const juce::KeyPress& key, juce::Component* focusedComponent);

    // Shortcut-Info anzeigen
    juce::String getShortcutDisplayString(ShortcutAction action) const;
    juce::String getShortcutDescription(ShortcutAction action) const;

private:
    ActionCallback actionCallback;

    // Shortcut-Zuordnungen
    struct Shortcut
    {
        juce::KeyPress key;
        ShortcutAction action;
        juce::String description;
    };

    std::vector<Shortcut> shortcuts;

    void setupDefaultShortcuts();
};

/**
 * Schnell-Zugriff Overlay für Einstellungen und Parameter
 * Ähnlich Pro-Q3's Overlay-Menü für Schnellzugriff
 */
class QuickAccessOverlay : public juce::Component
{
public:
    QuickAccessOverlay();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void show(juce::Point<int> screenPos);
    void hide();

    bool isVisible() const { return isShown; }

private:
    bool isShown = false;
    juce::Point<int> position;

    struct QuickAction
    {
        juce::String name;
        juce::String shortcut;
        std::function<void()> callback;
    };

    std::vector<QuickAction> actions;
};

/**
 * Band Inspector: Detaillierte numerische Anzeige und Bearbeitung
 * Für präzise Eingabe von Parametern bei laufender Solopräsenz
 */
class BandInspector : public juce::Component
{
public:
    struct BandData
    {
        int bandIndex = -1;
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.71f;
        int filterType = 0;
        bool active = false;
        bool bypassed = false;
    };

    using ChangeCallback = std::function<void(const BandData&)>;

    BandInspector();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setBandData(const BandData& data);
    void onDataChanged(ChangeCallback callback) { changeCallback = callback; }

private:
    BandData currentData;
    ChangeCallback changeCallback;

    juce::Label frequencyLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label typeLabel;

    // Numerische Input-Felder für präzise Werteeingabe
    juce::TextEditor frequencyInput;
    juce::TextEditor gainInput;
    juce::TextEditor qInput;
};
