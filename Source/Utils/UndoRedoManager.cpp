#include "UndoRedoManager.h"

UndoRedoManager::UndoRedoManager()
{
}

void UndoRedoManager::addAction(std::unique_ptr<Action> action)
{
    // Alle Redo-Aktionen löschen (nach einer Änderung)
    while ((int)actions.size() - 1 > undoIndex)
    {
        actions.pop_back();
    }

    // Begrenzen auf MAX_UNDO_STEPS
    if ((int)actions.size() >= MAX_UNDO_STEPS)
    {
        actions.erase(actions.begin());
        undoIndex--;
    }

    actions.push_back(std::move(action));
    undoIndex = (int)actions.size() - 1;

    // Neue Aktion sofort anwenden
    actions[undoIndex]->redo();
}

void UndoRedoManager::undo()
{
    if (canUndo())
    {
        actions[undoIndex]->undo();
        undoIndex--;
    }
}

void UndoRedoManager::redo()
{
    if (canRedo())
    {
        undoIndex++;
        actions[undoIndex]->redo();
    }
}

void UndoRedoManager::clear()
{
    actions.clear();
    undoIndex = -1;
}
