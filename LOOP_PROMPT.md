# Spreadsheet Feature Loop Prompt

You are running autonomously in a loop. Never ask questions. Never propose plans for approval. Never say "should I" or "would you like". Just do the work. If you face a decision, make it, document why in a commit message, and move on. If you're wrong, a future cycle will catch and fix it.

## Your Task

Implement the spreadsheet views feature for qt-multiplier, following the design plan in `~/Downloads/spreadsheet-design-plan.md`. **Adapt the plan to the qt-multiplier way** — the plan is a design document, not copy-pasteable code. Specifically:

- **File organization**: Use qt-multiplier conventions. Widgets go under `widgets/`, with `include/multiplier/GUI/Widgets/` for public headers and `src/` for implementation. New widget libraries follow the `mx_<name>` CMake target pattern. The SpreadsheetManager should follow the pattern of other managers in `managers/`. The SpreadsheetExplorer (or equivalent plugin) follows `IMainWindowPlugin` pattern in `explorers/`.
- **Include paths**: Use `<multiplier/GUI/...>` style includes, not `<qtmultiplier/spreadsheet/...>`.
- **Manager pattern**: SpreadsheetManager should mirror ConfigManager/ThemeManager/MediaManager patterns — owned by ConfigManager or MainWindow, accessed via reference.
- **Explorer/Plugin pattern**: Integration with the main window uses `IMainWindowPlugin`. Dock widgets use `IWindowWidget` and `IWindowManager::AddDockWidget`/`AddCentralWidget`. Follow the CodeSearchExplorer or ReferenceExplorer as templates.
- **Token rendering**: Reuse the existing `ThemedItemDelegate` infrastructure or the `ColumnTintDelegate` pattern from CodeSearchExplorer. Do NOT rewrite token painting.
- **SQLite persistence**: Use the project database connection already available via ConfigManager (the `project_db` QSqlDatabase). Create `gui_spreadsheet_*` tables alongside existing `gui_*` tables.
- **Signal routing**: Use `RequestPrimaryClick`/`RequestSecondaryClick`/`RequestKeyPress` signals through `IWindowWidget`, exactly like other explorers.
- **Theme integration**: Connect to `ThemeManager::ThemeChanged` for font/color updates.
- **Build system**: Add new subdirectories to `widgets/CMakeLists.txt` and `explorers/CMakeLists.txt`. Link against `mx_qt_library`, `mx_multiplier_library`, `mx_config_manager`, etc.
- **MetaTypes**: Register new types (`FormulaCell`, etc.) with `Q_DECLARE_METATYPE` and `qRegisterMetaType` as needed.
- **Commit identity**: Always use `--author="Peter Goodman <peter.goodman@gmail.com>"` and set `GIT_COMMITTER_NAME="Peter Goodman" GIT_COMMITTER_EMAIL="peter.goodman@gmail.com"`.

## Implementation Phases

Follow the phased approach from the plan (sections 11), adapted to qt-multiplier:

### Phase 1: Minimal model + view + dock integration
- Create `widgets/SpreadsheetWidget/` with SpreadsheetModel, SpreadsheetDelegate, SpreadsheetView
- Create `explorers/SpreadsheetExplorer/` as an IMainWindowPlugin
- Register in MainWindow::InitializePlugins
- Hard-code a test sheet and verify rendering with mixed cell types (strings, bools, tokens)
- Build and verify it compiles and shows up as a dock

### Phase 2: Mutations + undo/redo
- QUndoStack, all command classes
- Context menu for row/column operations
- Ctrl+Z / Ctrl+Shift+Z

### Phase 3: Sorting and filtering
- SpreadsheetProxyModel with lessThan override
- Per-column filters

### Phase 4: Copy/paste and export
- mimeData with all four formats
- HTML and Markdown export
- Cross-sheet and external paste

### Phase 5: SQLite persistence
- gui_spreadsheet_* tables (created via ConfigManager's project_db)
- value_to_json / value_from_json
- Undo history serialization

### Phase 6: Full integration
- "Open in Spreadsheet" actions from code search results
- Blank sheet creation from menu
- SpreadsheetManager lifecycle

### Phase 7: Formula system (if Python bindings enabled)
- FormulaCell, FormulaExecutor
- Python formula tasks
- Stale indicators, recompute actions

## Tracking

Create `SPREADSHEET_TODOS.md` in the repo root to track progress. Update it each cycle with:
- What was completed
- What's next
- Any decisions made and why

## Rules

- Build after every significant change. Fix build errors immediately.
- Commit frequently — at least once per phase or significant sub-task. Don't accumulate large uncommitted deltas.
- The build command is: `cd /Users/orangesloth/Build/multiplier/Release/qt-multiplier && cmake --build . 2>&1 | tail -20`
- If CMake needs reconfiguring: `cd /Users/orangesloth/Build/multiplier/Release/qt-multiplier && cmake /Users/orangesloth/Code/qt-multiplier`
- `-Werror` is enabled — all warnings are errors. Fix them.
- The macdeployqt rpath error for Python is known and ignorable.
- Never modify files in `libraries/` (third-party code).
- Read existing code before writing new code — understand the patterns first.
- When the SPREADSHEET_TODOS.md shows all phases complete with no remaining items, the task is done. Output "SPREADSHEET IMPLEMENTATION COMPLETE" and stop.
