# Spreadsheet Feature — Progress Tracker

## Phase 1: Minimal model + view + dock integration
- [x] Create widgets/SpreadsheetWidget/ (SpreadsheetModel, SpreadsheetDelegate, SpreadsheetView)
- [x] Create SpreadsheetExplorer as IMainWindowPlugin
- [x] Register in MainWindow::InitializePlugins
- [x] Add to CMake build (Qt6::Sql added to components)
- [x] Build compiles cleanly
- [ ] Verify rendering with mixed cell types at runtime
- [ ] Hard-code test data to verify token rendering

## Phase 1.5: Row/column background colors
- [ ] Per-row and per-column background color support in SpreadsheetModel
- [ ] Right-click row/column header to set background color (QColorDialog)
- [ ] BackgroundRole in model returns the row/column color
- [ ] Persistence of row/column colors in SQLite

## Phase 2: Mutations + undo/redo
- [x] QUndoStack integration
- [x] SetCellValueCommand, InsertRowsCommand, RemoveRowsCommand, etc.
- [x] Internal vs public mutation API split
- [x] Context menu for row/column operations
- [x] Ctrl+Z / Ctrl+Shift+Z

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

## Future: Global undo support
- [ ] Global undo/redo toolbar buttons (route to the active widget's QUndoStack)
- [ ] Make macro (un)expansion undoable (QUndoCommand wrapping ExpandedMacrosModel changes)
- [ ] Make highlight color set/remove undoable (QUndoCommand wrapping HighlightThemeProxy changes)
- [ ] Consider a global QUndoStack for the application (vs per-widget stacks)

## Cycle Log

### Cycle 1 (2026-04-05)
- Completed Phase 1: widget library, explorer, CMake integration, build passes
- Completed Phase 2: QUndoStack, all command classes, Ctrl+Z/Shift+Z, context menu undo/redo

### Cycle 2 (2026-04-06)
- Token copy/paste from code explorer to sheets (with kind/category)
- All cells editable with smart token-preserving substring edits
- Multiline cells via Shift+Enter with QPlainTextEdit editor
- Row/column background colors with QColorDialog
- Bottom toolbar (+ Row, + Col, + Checkbox Col, Del, Move)
- New Sheet toolbar button, File > New Sheet menu
- Theme-colored headers, grid lines, alternating row colors
- Fixed: empty cells editable, crash on exit, checkbox rendering
- Renamed Spreadsheets to Sheets
- Fixed: theme persistence (blocked auto-save before LoadSettings),
  theme menu radio buttons, WAL checkpoint on exit
- Fixed: token copy goes through CodeExplorer path (SelectedTokensRole)
- Fixed: SpreadsheetDelegate renders tokens with per-token syntax colors
- Fixed: whole-token drawing for proper kerning, configurable tab width
- Fixed: toolbar button (triggered vs toggled), dock objectNames
- **BLOCKER**: Sheet persistence not yet implemented (Phase 5). Sheets
  are lost on exit.
- Next: Phase 5 (SQLite persistence) is critical
- Next: extract shared token painting utility (duplicated in 3 places)
