# Multiplier GUI Tutorial

A comprehensive guide to using the Multiplier graphical user interface for code analysis and exploration.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
  - [macOS Setup](#macos-setup)
  - [Linux Setup](#linux-setup)
- [Getting Started](#getting-started)
  - [Opening a Database](#opening-a-database)
  - [User Interface Overview](#user-interface-overview)
- [Core Features](#core-features)
  - [Browse Mode](#browse-mode)
  - [Project Explorer](#project-explorer)
  - [Code Explorer](#code-explorer)
  - [Information Explorer](#information-explorer)
  - [Entity Explorer](#entity-explorer)
- [Advanced Features](#advanced-features)
  - [Reference Explorer](#reference-explorer)
  - [Code Preview](#code-preview)
  - [Macro Explorer](#macro-explorer)
  - [Highlight Explorer](#highlight-explorer)
- [Themes and Customization](#themes-and-customization)
- [Keyboard Shortcuts](#keyboard-shortcuts)
- [Pro Tips](#pro-tips)
- [Troubleshooting](#troubleshooting)
- [Support](#support)

## Prerequisites

**Important**: Multiplier requires pre-indexed database files to function. You cannot open regular source code directories directly. The application works with SQLite database files created by the `mx-index` tool from the Multiplier indexing pipeline.

### Database Requirements
- **Format**: SQLite databases with `.db` extension
- **Associated files**: `.db-shm` (shared memory) and `.db-wal` (write-ahead log)
- **Creation**: Generated using `mx-index` from the [Multiplier repository](https://github.com/trailofbits/multiplier)
- **Sample databases**: Available in the Releases section for testing

### Generating a New Database

To create your own database, locate the `mx-index` binary within your `Multiplier.app` bundle, then execute the following command:

```bash
mx-index --target compile_commands.json --db /tmp/database.mx
```

**Note**: It is advised to use the temporary folder to avoid potential memory mapping errors within the indexer.

## Installation

### macOS Setup

1. **Download** the `Multiplier.zip` file from the releases page

2. **Remove quarantine attributes** (Required for security):
   ```bash
   xattr -d -r com.apple.quarantine ~/Downloads/Multiplier.zip
   ```

3. **Extract and install**:
   - Extract the zip file
   - Move `Multiplier.app` to your Applications folder

4. **First launch**:
   - Right-click on `Multiplier.app` and select "Open"
   - This bypasses macOS Gatekeeper for the first run

5. **If code signing issues occur**:
   ```bash
   codesign --force --deep -s - ~/Downloads/Multiplier.app
   ```

> **Note**: On macOS, "Meta key" references in this tutorial refer to the ⌘ (Command) key.

### Linux Setup

1. **Download** the `.deb` package from the releases page

2. **Install using apt**:
   ```bash
   sudo apt install ./multiplier-2.0_1.linux_x86_64.deb
   ```

3. **Handle dependency issues** if they occur:
   ```bash
   sudo dpkg -i multiplier-2.0_1.linux_x86_64.deb
   sudo apt install -f
   ```

4. **Installation locations**:
   - SDK: `/opt/multiplier`
   - Binary: `/opt/multiplier/bin/multiplier`

5. **Launch the application**:
   ```bash
   /opt/multiplier/bin/multiplier
   # Or with a specific database:
   /opt/multiplier/bin/multiplier --database /path/to/indexed.db
   ```

> **Note**: On Linux, "Meta key" references refer to the Ctrl key.

## Getting Started

### Opening a Database

1. **Launch Multiplier** from Applications (macOS) or command line (Linux)
2. **File dialog opens automatically** on first launch
3. **Select a database file**:
   - Choose the `.db` file (ignore `.db-shm` and `.db-wal` files)
   - Sample databases are available in the Releases folder
4. **Alternative**: Use command line with `--database` flag

### User Interface Overview

The Multiplier interface consists of four main areas:

```
┌─────────────┬─────────────────────────────┐
│   Project   │                             │
│  Explorer   │        Code View            │
│  (top-left) │       (center)              │
├─────────────┤                             │
│   Entity    │                             │
│  Explorer   │                             │
│ (center-left)│                             │
├─────────────┤                             │
│Information  │                             │
│ Explorer    │                             │
│(bottom-left)│                             │
└─────────────┴─────────────────────────────┘
```

#### Toolbar Components

- **Back button** with history dropdown for navigation
- **Forward button** for returning to previous locations
- **Browse mode toggle** (dark gray when enabled)

## Core Features

### Browse Mode

Browse Mode makes Multiplier behave like a web browser for code navigation:

#### When Enabled (Default)
- Click on entities to navigate to their definitions (like web links)
- Use `Meta+Click` to place cursor without navigating
- Alternative: "micro-drag" click or use arrow keys for cursor placement

#### When Disabled
- Normal clicking places cursor
- Use `Meta+Click` to navigate to definitions

**Tip**: New users should disable Browse Mode initially and use `Meta+Click` for navigation until comfortable with the interface.

### Project Explorer

The Project Explorer shows a hierarchical file tree view of the database contents.

#### Key Features
- **Database-stored files**: Files are stored in the database, not on your local computer
- **Expandable tree**: Use arrow keys or mouse clicks to expand/collapse directories
- **Focus directories**: Right-click directories for "Set as Root" to focus on subdirectories
- **Quick search**: Press `Meta+F` to search/filter files

#### Navigation
- **Single-click**: Select files
- **Double-click**: Open files in the Code Explorer
- **Keyboard**: Use arrow keys for quick navigation
- **Right-click**: Access context menus with additional options

### Code Explorer

The Code Explorer displays syntax-highlighted source code from the database.

#### Features
- **Syntax highlighting**: Entity-aware coloring system
- **Entity highlighting**: Related entities appear in blue-gray
- **Tabbed interface**: Support for multiple open files
- **Line numbers**: With code structure visualization
- **Deduplication**: Content-equivalent files may be merged
- **Word wrapping**: Enable via `View > Enable word wrap`
- **Code preview**: Enable hover previews via `View > Code preview`

#### Entity Interaction
- **Navigation**: Click on highlighted entities (when Browse Mode enabled)
- **Context menus**: Right-click for additional actions
- **Tooltips**: Hover for quick information
- **Cursor placement**: Highlights related entities automatically

### Information Explorer

Shows contextual information about selected entities and code elements.

#### Key Features
- **Auto-updates**: Responds to clicks and navigation
- **Sync control**: Checkbox to enable/disable automatic updates
- **Navigation**: Independent back/forward buttons
- **Attack surface analysis**: Useful for security assessment
- **Entity details**: Types, declarations, usage patterns

#### Usage Tips
- Keep sync enabled for real-time updates
- Disable sync when comparing information across different entities
- Use for understanding entity relationships and dependencies
- Excellent for security analysis (e.g., finding all `SYSCALL_DEFINEx` in Linux kernel)

### Entity Explorer

Database-wide search for entities by name with advanced filtering capabilities.

#### Search Features
- **Word Prefix Match**: Default mode for fast searching
- **Exact Match**: For precise searches using the "Exact" option
- **Containing Match**: Search for entities containing the search string
- **Syntax highlighting**: By entity type (functions, variables, classes)
- **Category filtering**: Dropdown filters by entity type
- **Real-time search**: Results update as you type

#### Advanced Filtering (`Meta+F`)
- **Regular expressions**: `.*` prefix for regex patterns
- **Whole word matching**: `|ab|` syntax
- **Case insensitive**: `Aa` toggle

#### Usage
- Enter search terms to find entities across the entire codebase
- Select either "Exact" or "Containing" search mode
- Use category filters to narrow results by entity type
- Use additional filtering shortcuts for refined searches
- Double-click results to navigate to definitions
- Export results using right-click "Copy Details"

## Advanced Features

### Reference Explorer

Provides deep analysis of code relationships through specialized plugins.

#### Opening the Reference Explorer
The reference explorer can be opened in two ways:
1. Right-clicking on a token
2. With the text cursor on a token, pressing either `T` (for tainting) or `X` (to show the call hierarchy)

#### Interface Components
Within the widget, there are three components that are always available:
- **Graphical tree view**
- **Text representation**
- **Code preview**

Under `View > Reference Explorer` it is possible to choose the default split between the graphical and text representation. Regardless of the setting, you can show either view at any time by dragging the top or lower edge.

The code preview is hidden by default, but can be shown by dragging the right edge toward the center. If visible, it will stay in sync with the selected item.

#### Top Right Corner Buttons
- **Save to active tab**: If there is a reference explorer docked at the bottom, copy the references there and close the dialog. Otherwise, create a new tab.
- **Save to new tab**: Always create a new docked tab at the bottom containing all the references on screen.
- **Close**

#### Call Hierarchy Plugin (`x` hotkey)
- **Purpose**: Shows function call chains and dependencies
- **Tree Structure**: Parent entities contain children that reference them
- **Breadcrumbs**: Display usage context (function calls, assignments, etc.)
- **Sorting**: Use breadcrumbs column to find unusual usage patterns
- **Expansion**: Click row numbers 1-9 for multi-level expansion

**Button States**:
- 🔍 - Open entity in code explorer
- ⊕ (enabled) - Expand to show usages
- ⊕ (disabled) - Already expanded or no usages
- 🏠 - Navigate to original row (for duplicates)

#### Struct Explorer Plugin (`s` hotkey)
- **Purpose**: Analyze struct/class details and memory layout
- **Features**:
  - Field offsets and sizes
  - Recursive expansion of nested structures
  - Cumulative size tracking
  - Memory layout visualization
- **Note**: No column sorting available (unlike call hierarchy)

#### Class Hierarchy Explorer (`C` hotkey)
- **Purpose**: Show inheritance hierarchies and virtual function tables
- **Features**:
  - Complete class relationship visualization
  - Recursive expansion support
  - Inheritance tree navigation

### Code Preview

Quick preview functionality for exploring entity definitions without full navigation.

#### Basic Preview (`p` hotkey)
- **Shows**: Definition and immediate context
- **Features**: Breadcrumbs and usage information
- **Navigation**: Back/forward buttons
- **Search**: `Meta+F` for text search within preview

#### Pinned Preview (`Shift+P`)
- **Purpose**: Permanent preview windows
- **Usage**: Keep references open while exploring other code
- **Benefits**: Compare multiple definitions simultaneously
- **Chaining**: Use `p` within preview to drill through reference chains

### Macro Explorer

Specialized tool for understanding macro definitions and expansions.

#### Features
- **Inline expansion**: Press `e` on macro usage
- **Expansion view**: Shows how macros expand in context
- **Usage tracking**: Find all macro usage across the codebase
- **Nested expansion**: Handle complex macro chains
- **Management**: Remove expansions with ❌ button

#### Benefits
- Understand complex macro-heavy codebases
- Debug macro expansion issues
- Analyze macro usage patterns
- Navigate macro definition chains

### Highlight Explorer

Visual highlighting system for marking and tracking entities.

#### Features
- **Color assignment**: Right-click entity → Highlights → Set Color
- **Toggle highlights**: `h` hotkey (auto-assigns colors)
- **Management**: Highlight explorer dock for organized control
- **Removal**: Context menu options for cleanup

#### Usage
- Mark important entities for visual tracking
- Color-code different types of entities
- Create visual maps of code relationships
- Track entities across multiple files

## Themes and Customization

### Available Themes

#### Dark Theme (Default)
- Custom color scheme optimized for code analysis
- High contrast for entity highlighting
- Reduced eye strain for extended use

#### Light Theme
- Similar to SciTools Understand
- Traditional bright background
- Suitable for well-lit environments

### Theme Management
- **Switch themes**: View > Theme menu
- **Theme proxies**: Stack on base themes for customization
- **Data model access**: Full access for complex coloring rules
- **Custom fonts**: Supported (same size required for all code)

## Keyboard Shortcuts

### Essential Shortcuts

| Key | Action | Description |
|-----|--------|-------------|
| `i` | Information Explorer | Open entity information |
| `Shift+I` | Pinned Information | Open information in new window |
| `p` | Code Preview | Quick preview of entity definition |
| `Shift+P` | Pinned Preview | Open preview in permanent window |
| `s` | Struct Explorer | Analyze struct/class details |
| `x` | Call Hierarchy | Show function call relationships |
| `t` | Tainting | Show tainting analysis in Reference Explorer |
| `e` | Macro Expansion | Expand macros inline |
| `C` | Class Hierarchy | Show class inheritance tree |
| `h` | Toggle Highlights | Auto-assign entity colors |
| `Meta+F` | Find/Filter | Search within current view |

### Navigation Shortcuts

| Key | Action | Description |
|-----|--------|-------------|
| `Meta+Click` | Context Navigation | Navigate (Browse Mode off) or cursor placement (Browse Mode on) |
| `1-9` | Multi-level Expansion | Expand hierarchy levels in explorers |
| Arrow keys | Tree Navigation | Navigate file trees and hierarchies |
| Back/Forward | History Navigation | Navigate through view history |

### Search and Navigation by Platform

#### Find/Filter Shortcuts
|          | macOS         | KDE Plasma | GNOME                      |
|----------|---------------|------------|----------------------------|
| Find     | `Cmd+F`       | `Ctrl+F`   | `Ctrl+F`                   |
| Next     | `Cmd+G`       | `F3`       | `Ctrl+G`, `F3`             |
| Previous | `Cmd+Shift+G` | `Shift+F3` | `Ctrl+Shift+G`, `Shift+F3` |

#### History Navigation
|         | macOS    | KDE Plasma | GNOME      |
|---------|----------|------------|------------|
| Back    | `Cmd+[`  | `Alt+Left` | `Alt+Left` |
| Forward | `Cmd+]`  | `Alt+Right`| `Alt+Right`|

### Platform-Specific Keys

- **macOS**: Meta key = ⌘ (Command)
- **Linux**: Meta key = Ctrl

## Pro Tips

### Efficient Navigation
- **Universal search**: Use `Meta+F` in most views - it's available almost everywhere
- **Context menus**: Right-click frequently for additional options and actions
- **Deep expansion**: Use number keys (1-9) for multi-level hierarchy expansion
- **Permanent references**: Pin explorers (`Shift+` shortcuts) for permanent reference windows

### Analysis Workflows

#### Security Analysis
- Use Information Explorer to assess attack surfaces and input validation
- Search for system call definitions and security-critical functions
- Track data flow through highlighted entities

#### Code Understanding
- Combine Call Hierarchy (`x`) and Struct Explorer (`s`) for comprehensive analysis
- Use Entity Explorer for project-wide symbol searches
- Chain code previews to drill through reference chains

#### Refactoring Preparation
- Use Entity Explorer to find all references before making changes
- Utilize Call Hierarchy to understand impact of function changes
- Pin multiple information windows for comparison

#### Debugging and Analysis
- Use Macro Explorer (`e`) to understand complex macro expansions
- Highlight related entities for visual tracking
- Use struct explorer for memory layout analysis

### Performance Optimization
- **Disable sync**: Turn off Information Explorer sync when comparing multiple entities
- **Targeted searches**: Use category filters in Entity Explorer
- **Window management**: Close unused tabs and pinned windows
- **Directory focus**: Use "Set as Root" in Project Explorer for large codebases

### Database Management
- Keep all database files (.db, .db-shm, .db-wal) in the same directory
- Use absolute paths when launching with `--database` flag
- Backup database files before intensive analysis sessions
- Test with sample databases before analyzing your own code

## Troubleshooting

### Common Issues

#### macOS Quarantine Problems
- **Symptom**: Application won't launch or shows security warnings
- **Solution**: Remove quarantine attributes before extracting
- **Command**: `xattr -d -r com.apple.quarantine ~/Downloads/Multiplier.zip`

#### Linux Dependency Issues
- **Symptom**: Package installation fails with dependency errors
- **Solution**: Use `apt install -f` after initial installation
- **Alternative**: Move .deb file to /tmp before installation

#### Database Loading Problems
- **Symptom**: Cannot open database files
- **Solution**: Ensure all database files (.db, .db-shm, .db-wal) are present
- **Check**: Verify database was created with compatible mx-index version

#### Performance Issues
- **Symptom**: Slow response or high memory usage
- **Solution**: Close unused windows, disable sync options, focus on specific directories
- **Alternative**: Use sample databases to test performance

### Error Messages

#### "Download is performed unsandboxed as root"
- **Cause**: APT security warning on Linux
- **Solution**: Move .deb file to /tmp and install from there
- **Alternative**: Run `sudo chown -R _apt:root /var/lib/apt/lists`

#### Code Signing Issues
- **Cause**: macOS security policies
- **Solution**: Use `codesign --force --deep -s -` command
- **Alternative**: Right-click and "Open" for first launch

## Support

### Getting Help

**Contact Person**: Peter Goodman

**Note**: There is currently no public bug tracker for qt-multiplier. Please email Peter Goodman directly for:
- Bug reports and issues
- Feature requests
- Usage questions
- Integration support

### When Reporting Issues

Please include:
- **Operating system** and version
- **Multiplier version** and installation method
- **Database details** (size, creation method, source project)
- **Steps to reproduce** the problem
- **Error messages** or unexpected behavior
- **Screenshots** if applicable

### Feature Requests

For feature requests or priority use cases:
- Contact the development team via email
- Describe your specific use case and requirements
- Explain how the feature would improve your workflow
- Provide examples of similar functionality in other tools

---

*This tutorial covers the essential features of Multiplier GUI. For advanced usage patterns and integration with the Multiplier indexing pipeline, refer to the main [Multiplier documentation](https://github.com/trailofbits/multiplier).*