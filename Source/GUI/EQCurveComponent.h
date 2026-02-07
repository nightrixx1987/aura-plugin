#pragma once

#include <JuceHeader.h>
#include "../DSP/EQProcessor.h"
#include "SpectrumAnalyzer.h"
#include "CustomLookAndFeel.h"

/**
 * EQCurveComponent: Zeichnet die EQ-Kurve und ermöglicht interaktive Band-Steuerung.
 */
class EQCurveComponent : public juce::Component,
                         public juce::Timer
{
public:
    // Callback für Parameter-Änderungen
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void bandParameterChanged(int bandIndex, float frequency, float gain, float q) = 0;
        virtual void bandSelected(int bandIndex) = 0;
        virtual void bandCreated(int bandIndex, float frequency) = 0;
        virtual void filterTypeChanged(int bandIndex, ParameterIDs::FilterType type) = 0;
        virtual void bandDeleted(int /*bandIndex*/) {}
        virtual void bandRightClicked(int /*bandIndex*/) {}  // Rechtsklick auf Band-Point
    };

    EQCurveComponent();
    ~EQCurveComponent() override;
    
    // Timer-Kontrolle - muss nach vollständiger Initialisierung aufgerufen werden
    void startCurveUpdates() { startTimerHz(30); }
    void stopCurveUpdates() { stopTimer(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Maus-Interaktion
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // EQ-Processor setzen
    void setEQProcessor(EQProcessor* processor);

    // Band-Parameter direkt setzen (für Synchronisation mit APVTS)
    void setBandParameters(int bandIndex, float frequency, float gain, float q, 
                           ParameterIDs::FilterType type, bool bypassed, bool active);

    // Band löschen/deaktivieren
    void deleteBand(int bandIndex);

    // Listener
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Selektiertes Band
    int getSelectedBand() const { return selectedBand; }
    void setSelectedBand(int bandIndex);
    
    // Band-Position auf dem Bildschirm
    juce::Point<int> getBandScreenPosition(int bandIndex) const;
    
    // Drag-Status (für externe Synchronisation)
    bool isDraggingBand() const { return isDragging; }

    // Koordinaten-Konvertierung (delegiert an SpectrumAnalyzer)
    float frequencyToX(float frequency) const;
    float xToFrequency(float x) const;
    float dbToY(float db) const;
    float yToDb(float y) const;

    // EQ-Dezibel-Bereich einstellen
    void setEQDecibelRange(float minDB, float maxDB);

private:
    EQProcessor* eqProcessor = nullptr;
    juce::ListenerList<Listener> listeners;

    // Band-Positionen (für Hit-Testing)
    struct BandHandle
    {
        int bandIndex = -1;
        float x = 0.0f;
        float y = 0.0f;
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.71f;
        ParameterIDs::FilterType type = ParameterIDs::FilterType::Bell;
        bool bypassed = false;
        bool active = false;
    };
    
    std::array<BandHandle, ParameterIDs::MAX_BANDS> bandHandles;
    
    int selectedBand = -1;
    int hoveredBand = -1;
    bool isDragging = false;
    juce::Point<float> dragStartPos;
    float dragStartFreq = 0.0f;
    float dragStartGain = 0.0f;

    // Kurven-Pfad
    juce::Path curvePath;
    std::array<juce::Path, ParameterIDs::MAX_BANDS> bandPaths;
    
    // Vorberechnete Frequenz-Tabelle (ein Eintrag pro Pixel, berechnet in resized())
    std::vector<float> freqTable;
    
    // Dirty-Flag: nur bei Parameter-Änderung neu berechnen
    bool curvesDirty = true;
    void markCurvesDirty() { curvesDirty = true; }

    // Frequenz/dB-Bereiche
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float minDB = -36.0f;
    float maxDB = 36.0f;

    // Handle-Größe
    static constexpr float HANDLE_RADIUS = 8.0f;
    static constexpr float HANDLE_HIT_RADIUS = 15.0f;

    // Drag-Modus
    enum class DragConstraint { None, HorizontalOnly, VerticalOnly };
    DragConstraint dragConstraint = DragConstraint::None;
    
    // Popup-Status: Welches Band hat gerade ein Popup offen?
    int bandWithOpenPopup = -1;

    // Methoden
    void updateCurvePath();
    void updateBandPaths();
    void drawCurve(juce::Graphics& g);
    void drawBandHandles(juce::Graphics& g);
    void drawBandCurve(juce::Graphics& g, int bandIndex);
    void drawParameterDisplay(juce::Graphics& g, int bandIndex);
    void drawDragGuideLine(juce::Graphics& g, int bandIndex);
    void drawDynamicEQCurves(juce::Graphics& g);
    
    int getBandAtPosition(juce::Point<float> pos) const;
    void notifyBandChanged(int bandIndex);
    void notifyBandSelected(int bandIndex);
    void notifyFilterTypeChanged(int bandIndex, ParameterIDs::FilterType type);
    
    // Kontextmenü
    void showFilterTypeMenu(int bandIndex, juce::Point<int> screenPos);
    
    // Hilfsmethoden für Formatierung
    juce::String formatFrequency(float freq) const;
    juce::String formatGain(float gain) const;
    juce::String formatQ(float q) const;
    
    // Dynamic EQ: Effektiven Gain nach Gain Reduction berechnen
    float calcEffectiveGain(float targetGain, float gainReduction) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQCurveComponent)
};
