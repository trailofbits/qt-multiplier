# Agent Integration Loop Prompt

You are implementing an LLM agent integration into qt-multiplier, a Qt6 C++ desktop
application for binary analysis. The full architecture plan is in `AGENT_PLAN.md`.

## Critical Context

- Build dir: `~/Build/multiplier/Release/qt-multiplier`
- **Must use system clang**, not venv compilers. Do NOT activate the venv.
- Build command: `cmake --build ~/Build/multiplier/Release/qt-multiplier 2>&1 | tail -80`
- Reconfigure: `cd ~/Build/multiplier/Release/qt-multiplier && cmake ~/Code/qt-multiplier`
- Qt is vendored (not system Qt). Already built.
- Never put the project DB filename in code or commits.
- Do not mention BrowseMode in code comments.
- Namespace: `mx::gui`. All new code goes in this namespace.
- Style: `Q_DECL_FINAL`, `#pragma once`, `snake_case` for methods, `PascalCase` for classes,
  `m_` prefix for members, `d` pointer pattern for pimpl.
- Copyright: `// Copyright (c) 2024-present, Trail of Bits, Inc.`
- Single-use widgets are **private** to their owning manager/explorer.
  Multi-use interfaces go in `interfaces/`.

## Architecture (already built)

```
interfaces/
  ILLMBackend.h          -- public LLM backend interface

managers/
  LLMManager/            -- owns backends (Claude, OpenAI, Bedrock, vLLM)
    include/.../LLMManager.h
    src/ ClaudeBackend, OpenAICompatBackend, BedrockBackend (private)

  AgentManager/          -- owns sessions, tools, registry
    include/.../AgentManager.h, AgentMessage.h
    src/ AgentSession, AgentTool, AgentToolRegistry (private)
    src/tools/ (to be filled)

explorers/ (to be built)
  AgentExplorer          -- UI plugin
    src/ ConversationWidget, ConfigPanel, ToolLog, SessionList (private)
```

## Your Task Each Iteration

1. **Orient**: Read `AGENT_PROGRESS.md` (create if missing) to see what's done.
2. **Pick the next incomplete step** from below.
3. **Implement it**: Write C++ files, update CMakeLists.txt as needed.
4. **Build-test**: Verify compilation. Fix errors before moving on.
5. **Update progress**: Write to `AGENT_PROGRESS.md`.
6. **Commit**: `git add` new/changed files and commit.

## Implementation Steps

### Step 5: Spreadsheet Tools
- `managers/AgentManager/src/tools/SpreadsheetTools.h/cpp`
- All tools inherit from AgentTool (defined in `managers/AgentManager/src/AgentTool.h`)
- Tools: CreateSheetTool, ListSheetsTool, GetSheetSummaryTool, ReadCellTool,
  WriteCellTool, ReadRowTool, ReadColumnTool, AddRowTool, InsertRowTool,
  DeleteRowTool, AddColumnTool, SetRowColorTool, ClearRowColorTool,
  SetCheckboxTool, SortSheetTool, ReadSheetRangeTool, GetSheetAsMarkdownTool
- Each needs: name(), description(), parametersSchema() -> JSON Schema, execute(QJsonObject) -> QJsonObject
- Tools need access to ConfigManager for DB operations. Pass a struct
  `SpreadsheetToolContext { ConfigManager *config; }` at construction time.
- Use ConfigManager methods: SaveSheet(), LoadOpenSheets(), LoadSheetById() etc.
- For live model operations (the in-memory SpreadsheetModel), tools post to
  the GUI thread via QMetaObject::invokeMethod with BlockingQueuedConnection.
- Register all tools in AgentManager (add a method or do it in constructor).
- Add to AgentManager CMakeLists.txt. Verify builds.

### Step 6: Document Tools
- `managers/AgentManager/src/tools/DocumentTools.h/cpp`
- Tools: CreateDocumentTool, ReadDocumentTool, EditDocumentTool,
  ListDocumentsTool, LinkDocumentToCellTool, SearchDocumentsTool,
  CreateCategorizedDocumentTool, ListDocumentsByCategoryTool
- Uses ConfigManager document methods
- **Document categories**: Documents can have a category field:
  'note' (default), 'prompt', 'report_template', 'skill_template', 'custom'
  The LLM can create prompts tailored to the codebase, stored as documents.
- Add to CMakeLists.txt. Verify builds.

### Step 7: Document Category Support in ConfigManager
- Add `category TEXT DEFAULT 'note'` to gui_documents table schema
- Add ConfigManager methods:
  - `LoadDocumentsByCategory(const QString &category)` -> QVector<DocumentInfo>
  - `SetDocumentCategory(int doc_id, const QString &category)`
  - Update CreateDocument to accept optional category parameter
- Update DocumentInfo struct to include category
- Verify builds.

### Step 8: Python and Navigation Tools
- `managers/AgentManager/src/tools/PythonTools.h/cpp`
  - RunPythonTool: execute code, capture stdout/stderr
  - CreateScriptFileTool: write to temp file
  - Guard with `#ifdef MX_ENABLE_PYTHON`
- `managers/AgentManager/src/tools/NavigationTools.h/cpp`
  - SearchEntitiesTool, GetDefinitionTool, GetReferencesTool,
    GetCallersTool, GetCalleesTool, ListFilesTool
  - Uses mx::Index API (get from ConfigManager)
- Add to CMakeLists.txt. Verify builds.

### Step 9: Session Tools
- `managers/AgentManager/src/tools/SessionTools.h/cpp`
  - GetAuditContextTool: returns summary of current sheets, documents, recent tools
  - SaveCheckpointTool: saves a named checkpoint with summary
  - LogObservationTool: logs a timestamped observation
- Add to CMakeLists.txt. Verify builds.

### Step 10: Persistence Extensions
- Extend ConfigManager with agent-related tables and methods.
- New tables (create in project DB init):
  ```sql
  gui_agent_sessions(session_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT, system_prompt TEXT, backend TEXT, model TEXT,
    status TEXT DEFAULT 'active', created_at TEXT, updated_at TEXT,
    total_prompt_tokens INTEGER DEFAULT 0, total_completion_tokens INTEGER DEFAULT 0)
  gui_agent_messages(message_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER, role TEXT, content TEXT, tool_name TEXT,
    tool_call_id TEXT, tool_args TEXT, tool_result TEXT,
    timestamp TEXT, token_count INTEGER DEFAULT 0)
  gui_agent_checkpoints(checkpoint_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER, summary TEXT, created_at TEXT)
  gui_agent_observations(observation_id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER, content TEXT, created_at TEXT)
  ```
- Add document category column: ALTER TABLE gui_documents ADD COLUMN category TEXT DEFAULT 'note'
- New ConfigManager methods for CRUD on all tables.
- Read existing ConfigManagerImpl pattern (in managers/ConfigManager/src/) to
  understand how tables are created and queries are structured.
- Verify builds.

### Step 11: AgentExplorer Core
- `explorers/include/multiplier/GUI/Explorers/AgentExplorer.h`
- `explorers/src/Explorers/AgentExplorer/AgentExplorer.cpp`
- IMainWindowPlugin subclass (see other explorers for pattern)
- Creates: central TabWidget area, dock for config, dock for tool log,
  dock for session list
- Toolbar buttons: New Session (icon), Pause/Resume, Stop, Observer Mode
- Owns: LLMManager instance, AgentManager instance
- At startup, registers all tools from Steps 5-9 with AgentManager
- Add to explorers/CMakeLists.txt source list
- Link mx_llm_manager and mx_agent_manager to mx_explorers
- Add `#include` and `new AgentExplorer(...)` in MainWindow::InitializePlugins()
- Verify builds.

### Step 12: Conversation Widget
- `explorers/src/Explorers/AgentExplorer/AgentConversationWidget.h/cpp`
- QWidget with QVBoxLayout: QScrollArea (messages) + input area
- Message rendering in scroll area:
  - User: right-aligned bubble, accent color
  - Assistant: left-aligned bubble, different color
  - Tool calls: collapsible card (click to expand args/result)
  - System: centered, muted
- Input: QPlainTextEdit + QPushButton (Send)
- Connect to AgentManager::messageAdded signal
- Theme-aware (connect to ThemeManager::ThemeChanged)
- Auto-scroll to bottom on new messages
- Verify builds.

### Step 13: Config Panel
- `explorers/src/Explorers/AgentExplorer/AgentConfigPanel.h/cpp`
- QWidget with form layout:
  - QComboBox: backend type (Claude/OpenAI/Bedrock/vLLM)
  - QLineEdit: API key (echoMode=Password)
  - QLineEdit: base URL (shown only for OpenAI/vLLM)
  - QComboBox: model name (editable, with presets per backend type)
  - QPlainTextEdit: system prompt
  - QPushButton: Load prompt from documents (category='prompt')
  - QSpinBox: max iterations
  - QDoubleSpinBox: temperature (0.0-2.0, step 0.1)
- Wired to LLMManager for persistence
- Verify builds.

### Step 14: Tool Log Widget
- `explorers/src/Explorers/AgentExplorer/AgentToolLogWidget.h/cpp`
- QTreeView + QStandardItemModel
- Columns: Timestamp, Tool Name, Duration (ms), Status
- Rows expand to show full args and result JSON
- Color: green=success, red=error
- Connected to AgentManager::toolCallStarted/Completed
- Verify builds.

### Step 15: Session List Widget
- `explorers/src/Explorers/AgentExplorer/AgentSessionListWidget.h/cpp`
- QListWidget showing saved sessions
- Each item: name, backend, status, date, token usage
- Double-click to load conversation
- Context menu: rename, delete, export
- Resume button for paused sessions
- Verify builds.

### Step 16: Wire Together
- Connect all widgets in AgentExplorer
- Signal routing: config panel -> LLMManager, conversation -> AgentManager
- Tool registration at startup
- Integration test: configure backend, send message, see response + tools
- Verify full build.

### Step 17: Observer Mode
- Secondary AgentSession in AgentManager
- GetPrimarySessionContextTool: reads primary session state
- ObserverRecommendationTool: writes to observer notes document
- Observer config uses separate backend/model/prompt
- Toggle button in toolbar
- Verify builds.

### Step 18: Polish
- Error handling (network failures, bad keys, malformed responses)
- Edge cases (empty sheets, missing documents, Python disabled)
- Verify persistence: close app, reopen, sessions survive
- Final build.

## Build Notes

- Headers with Q_OBJECT MUST be listed in CMakeLists.txt sources for AUTOMOC.
- Qt Network is already found by LLMManager. AgentManager doesn't need it.
- For thread-safe GUI operations from tools: use QMetaObject::invokeMethod
  with Qt::BlockingQueuedConnection.
- Use `std::unordered_map` instead of `QHash` for containers with `unique_ptr`.
- Look at existing explorers for patterns: DocumentExplorer, SpreadsheetExplorer.
- Look at ConfigManagerImpl for persistence patterns.

## Code References

- Tool base class: `managers/AgentManager/src/AgentTool.h`
- Tool registry: `managers/AgentManager/src/AgentToolRegistry.h`
- Session loop: `managers/AgentManager/src/AgentSession.h`
- ConfigManager: `managers/ConfigManager/include/multiplier/GUI/Managers/ConfigManager.h`
- SpreadsheetModel: `widgets/SpreadsheetWidget/include/multiplier/GUI/Widgets/SpreadsheetModel.h`
- IMainWindowPlugin: `explorers/include/multiplier/GUI/Interfaces/IMainWindowPlugin.h`

## Progress Tracking

Update `AGENT_PROGRESS.md` after each step:
```
## Step N: [Name]
- Status: COMPLETE / IN PROGRESS / BLOCKED
- Files created/modified: [list]
- Build status: PASS / FAIL (with error)
- Committed: [hash]
```
