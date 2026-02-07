#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

/**
 * UndoRedoManager: Verwaltung von Undo/Redo für Parameter-Änderungen.
 * Speichert Snapshots von Band-Konfigurationen für volle Funktionalität.
 */
class UndoRedoManager
{
public:
    UndoRedoManager();

    // Snapshot-Klasse für einzelne Band-States
    struct BandSnapshot
    {
        int bandIndex = -1;
        float frequency = 1000.0f;
        float gain = 0.0f;
        float q = 0.71f;
        int type = 0;
        bool active = false;
        bool bypassed = false;
        float slope = 12.0f;
        int channel = 0;

        BandSnapshot() = default;
        BandSnapshot(int idx, float freq, float g, float q_val, int t, bool act, bool byp, float slp, int ch)
            : bandIndex(idx), frequency(freq), gain(g), q(q_val), type(t), active(act), bypassed(byp), slope(slp), channel(ch)
        {
        }
    };

    // Aktion für Undo/Redo
    struct Action
    {
        virtual ~Action() = default;
        virtual void redo() = 0;
        virtual void undo() = 0;
        virtual juce::String getDescription() const = 0;
    };

    // Band-Parameter-Änderung
    struct BandParameterChangeAction : public Action
    {
        BandSnapshot before;
        BandSnapshot after;
        std::function<void(const BandSnapshot&)> applyFunction;

        BandParameterChangeAction(const BandSnapshot& b, const BandSnapshot& a,
                                  std::function<void(const BandSnapshot&)> apply)
            : before(b), after(a), applyFunction(apply)
        {
        }

        void redo() override { applyFunction(after); }
        void undo() override { applyFunction(before); }
        juce::String getDescription() const override { return "Band Parameter Change"; }
    };

    // Aktion hinzufügen
    void addAction(std::unique_ptr<Action> action);

    // Undo/Redo
    bool canUndo() const { return undoIndex > 0; }
    bool canRedo() const { return undoIndex < (int)actions.size() - 1; }

    void undo();
    void redo();
    void clear();

    int getUndoCount() const { return undoIndex; }
    int getRedoCount() const { return (int)actions.size() - undoIndex - 1; }

private:
    std::vector<std::unique_ptr<Action>> actions;
    int undoIndex = -1;

    static constexpr int MAX_UNDO_STEPS = 100;
};
