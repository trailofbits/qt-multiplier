# Spreadsheet Feature — Progress Tracker

## Phase 1: Minimal model + view + dock integration
- [x] Create widgets/SpreadsheetWidget/ (SpreadsheetModel, SpreadsheetDelegate, SpreadsheetView)
- [x] Create SpreadsheetExplorer as IMainWindowPlugin
- [x] Register in MainWindow::InitializePlugins
- [x] Add to CMake build (Qt6::Sql added to components)
- [x] Build compiles cleanly
- [ ] Verify rendering with mixed cell types at runtime
- [ ] Hard-code test data to verify token rendering

## Phase 2: Mutations + undo/redo
- [ ] QUndoStack integration
- [ ] SetCellValueCommand, InsertRowsCommand, RemoveRowsCommand, etc.
- [ ] Internal vs public mutation API split
- [ ] Context menu for row/column operations
- [ ] Ctrl+Z / Ctrl+Shift+Z

## Phase 3: Sorting and filtering
- [ ] SpreadsheetProxyModel with lessThan override
- [ ] Per-column filter support
- [ ] Filter UI (header context menu)

## Phase 4: Copy/paste and export
- [ ] mimeData with internal binary, plain text, HTML, Markdown formats
- [ ] selection_to_html with inline-styled token spans
- [ ] selection_to_markdown with backtick-wrapped code spans
- [ ] copy/paste/cut with undo support
- [ ] Copy as Markdown (Ctrl+Shift+C), Copy as HTML actions
- [ ] Cross-sheet and external paste
- [ ] Proper token painting via ThemedItemDelegate wrapping

## Phase 5: SQLite persistence
- [ ] gui_spreadsheet_* tables via ConfigManager project_db
- [ ] value_to_json / value_from_json helpers
- [ ] save_sheet / load_all_sheets
- [ ] Undo history serialization
- [ ] Undo stack reconstruction on load

## Phase 6: Full integration
- [ ] SpreadsheetManager lifecycle (owned by ConfigManager or MainWindow)
- [ ] "Open in Spreadsheet" action on code search results
- [ ] Blank sheet creation from menu
- [ ] Dock integration via IWindowManager
- [ ] New toolbar icon for creating a sheet (register with MediaManager, add via AddToolBarButton)

## Phase 7: Formula system
- [ ] FormulaCell struct and Q_DECLARE_METATYPE
- [ ] FormulaExecutor with QThreadPool
- [ ] PythonFormulaTask (if MX_ENABLE_PYTHON)
- [ ] Stale indicator rendering
- [ ] Recompute actions in context menu
- [ ] Error display
- [ ] Python formula editor with auto-complete popup (reuse PythonCompletionModel from PythonConsoleWidget)

## Cycle Log

### Cycle 1 (2026-04-05)
- Started Phase 1
- Creating widget library files (SpreadsheetModel, SpreadsheetView, SpreadsheetDelegate)
- Creating SpreadsheetExplorer plugin
- Next: build and verify
