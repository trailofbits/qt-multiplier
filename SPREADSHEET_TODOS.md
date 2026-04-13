# Spreadsheet & Documents — Remaining Work

## Sorting and filtering
- [ ] SpreadsheetProxyModel with lessThan override
- [ ] Per-column filter support
- [ ] Filter UI (header context menu)

## Undo history serialization
- [ ] Merge per-sheet undo stacks into a global stack
- [ ] Command serialization/deserialization for DB persistence
- [ ] Undo stack reconstruction on load

## Formula system
- [ ] FormulaCell struct and Q_DECLARE_METATYPE
- [ ] FormulaExecutor with QThreadPool
- [ ] PythonFormulaTask (if MX_ENABLE_PYTHON)
- [ ] Stale indicator rendering
- [ ] Recompute actions in context menu
- [ ] Error display
- [ ] Python formula editor with auto-complete popup

## Documents — new document creation
- [ ] Dropdown from the New Document toolbar button (instead of single action)
- [ ] "Create from Template" — pick from predefined document templates (e.g. analysis report, vulnerability summary, code review)
- [ ] "Create from Prompt" — enter an LLM prompt that generates the initial document content
- [ ] Template management (create/edit/delete custom templates)

## Documents — sync and export
- [ ] "Link to Git repo" — associate a document with a file path in a git repo for version-controlled sync
- [ ] Export document to standalone file (HTML, Markdown, PDF)

## Code search
- [ ] Ctrl+Shift+F "find in file" — focus toolbar regex search with file-level filter (X button to clear, replaces on subsequent use)

## Cleanup
- [ ] Extract shared token painting utility (duplicated in SpreadsheetDelegate, ThemedItemDelegate, CodeWidget)
- [ ] Remove debug stderr logging from SpreadsheetExplorer, SpreadsheetView, DocumentExplorer
