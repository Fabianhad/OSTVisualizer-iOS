// Layer: Domain
// Pure C++. No UIKit, no Foundation, no sqlite, no Objective-C.
//
// The undo / redo history of the plan editor, extracted from UndoRedoController (UND-01, UND-02).
//
// Windows: ost_visualizer/presentation/services/undo_redo_service.py
//   _push_entry (173-195): the entry is appended, the OLDEST entry is dropped when there are more than max_size (50) and the
//   redo stack is cleared;
//   _submit_history_transition (469-524): an undo / redo entry moves to the other stack only when its action COMMITTED; an
//   action that returns False or raises leaves the entry on the stack it came from, where the next undo / redo retries it
//   (AGENTS.md: "Failed or rejected persistence adds no history").
#pragma once
#include <cstddef>
#include <utility>
#include <vector>

namespace ost_domain
{

    // Windows UndoRedoService(max_size=50).
    constexpr std::size_t kUndoHistoryCapacity = 50;

    template <typename Entry>
    class UndoHistory
    {
    public:
        explicit UndoHistory(std::size_t capacity = kUndoHistoryCapacity) : _capacity(capacity) {}

        bool can_undo() const { return !_undo.empty(); }
        bool can_redo() const { return !_redo.empty(); }
        std::size_t undo_count() const { return _undo.size(); }
        std::size_t redo_count() const { return _redo.size(); }

        // A new action: it becomes the newest undo entry, the oldest one is dropped when the capacity is exceeded, and
        // whatever could be redone is gone.
        void push(Entry entry)
        {
            _undo.push_back(std::move(entry));
            if (_undo.size() > _capacity)
                _undo.erase(_undo.begin());
            _redo.clear();
        }

        // Runs `action(entry)` (it returns true when the entry was applied AND stored; it may change the entry) on the newest
        // undo entry. Only on true the entry moves to the redo stack; on false or an exception it stays where it is, so a
        // failed write advances nothing. Returns whether the history advanced.
        template <typename Action>
        bool undo(Action &&action)
        {
            return transition(_undo, _redo, action);
        }

        // The same for the newest redo entry, which moves back to the undo stack.
        template <typename Action>
        bool redo(Action &&action)
        {
            return transition(_redo, _undo, action);
        }

        void clear()
        {
            _undo.clear();
            _redo.clear();
        }

    private:
        template <typename Action>
        static bool transition(std::vector<Entry> &source, std::vector<Entry> &destination, Action &action)
        {
            if (source.empty())
                return false;
            bool applied = false;
            try
            {
                applied = static_cast<bool>(action(source.back()));
            }
            catch (...)
            {
                applied = false;
            }
            if (!applied)
                return false;
            destination.push_back(std::move(source.back()));
            source.pop_back();
            return true;
        }

        std::size_t _capacity;
        std::vector<Entry> _undo;
        std::vector<Entry> _redo;
    };

}
