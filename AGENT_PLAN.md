# Agent Integration Plan for qt-multiplier

## Overview

Integrate an LLM agent directly into qt-multiplier that can drive GUI actions
(create/edit sheets, documents, cells, checkboxes, colors, ratings) and execute
Python scripts via the Multiplier API. The agent supports multiple backends
(Claude, OpenAI/Codex, Bedrock, vLLM) and persists all conversations and
progress to SQLite for full observability and resumability. A secondary
"observer" agent can review the primary agent's work and make recommendations.

---

## Architecture

### Public Interfaces (interfaces/)
- **ILLMBackend** - Abstract LLM backend interface with sendMessage(), validateCredentials()
- Structs: LLMMessage, ToolCall, ToolDefinition, LLMResponse, LLMConfig

### Managers

**LLMManager** (managers/LLMManager/)
- Public header only: LLMManager.h
- Private widgets: ClaudeBackend, OpenAICompatBackend, BedrockBackend
- Central configuration of backends (add/remove/configure)
- QSettings persistence for backend configs
- Active backend selection

**AgentManager** (managers/AgentManager/)
- Public headers: AgentManager.h, AgentMessage.h
- Private widgets: AgentSession, AgentTool, AgentToolRegistry
- Private tools: SpreadsheetTools, DocumentTools, PythonTools, NavigationTools, SessionTools
- Session lifecycle: create, send, pause, resume, cancel
- Tool registration (explorers register tools at startup)
- Signals for all state changes (UI binding)

### Explorers
**AgentExplorer** (explorers/)
- IMainWindowPlugin subclass
- Private widgets:
  - AgentConversationWidget (chat view)
  - AgentConfigPanel (backend/model selection)
  - AgentToolLogWidget (tool call history)
  - AgentSessionListWidget (session browser)

---

## Implementation Steps (for loop prompt)

### DONE: Steps 1-4 (Core Infrastructure)
- ILLMBackend interface in interfaces/
- LLMManager with Claude, OpenAI, Bedrock backends
- AgentManager with session loop, tool registry
- All build clean

### Step 5: Spreadsheet Tools
- `managers/AgentManager/src/tools/SpreadsheetTools.h/cpp`
- Tools: CreateSheet, ListSheets, GetSheetSummary, ReadCell, WriteCell,
  ReadRow, ReadColumn, AddRow, InsertRow, DeleteRow, AddColumn,
  SetRowColor, ClearRowColor, SetCheckbox, SortSheet,
  ReadSheetRange, GetSheetAsMarkdown
- Each tool: name(), description(), parametersSchema(), execute()
- Tools need a SpreadsheetToolContext with pointers to ConfigManager
  and a way to invoke SpreadsheetExplorer methods on the GUI thread
- Use QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection) for
  GUI-thread operations

### Step 6: Document Tools
- `managers/AgentManager/src/tools/DocumentTools.h/cpp`
- Tools: CreateDocument, ReadDocument, EditDocument, ListDocuments,
  LinkDocumentToCell, SearchDocuments
- Documents now support **categories** (see below)

### Step 7: Document Categories
- Extend the document system to support categorized documents:
  - Report templates
  - Prompts (agent system prompts, tailored to codebases)
  - Skill templates (reusable agent behaviors)
  - Notes (default, current behavior)
  - Custom categories
- Add `category TEXT DEFAULT 'note'` column to gui_documents table
- Add ConfigManager methods: LoadDocumentsByCategory(), SetDocumentCategory()
- Agent tools: CreateCategorizedDocument, ListDocumentsByCategory
- The LLM can create new prompts stored as documents with category='prompt',
  tailored to the specific codebase being analyzed

### Step 8: Python and Navigation Tools
- `managers/AgentManager/src/tools/PythonTools.h/cpp`
  - RunPython, CreateScriptFile
  - Conditionally compiled with MX_ENABLE_PYTHON
- `managers/AgentManager/src/tools/NavigationTools.h/cpp`
  - SearchEntities, GetDefinition, GetReferences, GetCallers, GetCallees, ListFiles
  - Uses multiplier Index API

### Step 9: Session Tools
- `managers/AgentManager/src/tools/SessionTools.h/cpp`
  - GetAuditContext, SaveCheckpoint, LogObservation

### Step 10: ConfigManager Persistence Extensions
- New tables in project DB (.qmx):
  - gui_agent_sessions (session_id, name, system_prompt, backend, model, status, timestamps, token counts)
  - gui_agent_messages (message_id, session_id, role, content, tool_name, tool_call_id, tool_args, tool_result, timestamp, token_count)
  - gui_agent_checkpoints (checkpoint_id, session_id, summary, created_at)
  - gui_agent_observations (observation_id, session_id, content, created_at)
  - ALTER gui_documents ADD COLUMN category TEXT DEFAULT 'note'
- ConfigManager methods for CRUD on all tables
- Auto-create tables in project DB initialization

### Step 11: AgentExplorer - Core Plugin
- `explorers/include/multiplier/GUI/Explorers/AgentExplorer.h`
- `explorers/src/Explorers/AgentExplorer/AgentExplorer.cpp`
- IMainWindowPlugin subclass
- Creates central widget container and dock widgets
- Toolbar: New Session, Send/Pause/Stop, Observer toggle
- Add to explorers CMakeLists.txt, link mx_llm_manager and mx_agent_manager
- Add to MainWindow::InitializePlugins()

### Step 12: Agent Conversation Widget
- `explorers/src/Explorers/AgentExplorer/AgentConversationWidget.h/cpp`
- QScrollArea-based chat view
- Message bubbles: user (right), assistant (left), tool calls (collapsible)
- QPlainTextEdit input + Send button
- Theme-aware, auto-scroll

### Step 13: Agent Config Panel
- `explorers/src/Explorers/AgentExplorer/AgentConfigPanel.h/cpp`
- Backend selector, API key, model, base URL
- System prompt editor (can load prompts from document category='prompt')
- Max iterations, temperature
- Persist via LLMManager

### Step 14: Agent Tool Log Widget
- `explorers/src/Explorers/AgentExplorer/AgentToolLogWidget.h/cpp`
- QTreeView: timestamp, tool name, duration, status
- Expandable detail rows, color-coded

### Step 15: Agent Session List Widget
- `explorers/src/Explorers/AgentExplorer/AgentSessionListWidget.h/cpp`
- List of persisted sessions
- Resume, delete, rename, export

### Step 16: Wire Everything Together
- Connect AgentExplorer to LLMManager and AgentManager
- Register spreadsheet/document/python/navigation tools at startup
- Full integration: configure backend, send message, see tools execute

### Step 17: Observer Mode
- Second AgentSession that reads primary session's state
- GetPrimarySessionContext tool
- ObserverRecommendation tool
- Writes to dedicated observer notes document
- Toggle in toolbar

### Step 18: Polish and Testing
- Error handling, edge cases
- Persistence verification
- Build verification
