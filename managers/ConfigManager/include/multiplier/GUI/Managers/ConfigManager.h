// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>
#include <optional>

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QVector>

#include <unordered_map>

#include <multiplier/Types.h>

class QMenu;
class QUndoGroup;

namespace mx {
class FileLocationCache;
class Index;
}  // namespace mx
namespace mx::gui {

class ActionManager;
class ConfigManagerImpl;
class MediaManager;
class ThemeManager;
class TriggerHandle;

// Manages the global configuration.
class ConfigManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

  std::shared_ptr<ConfigManagerImpl> d;

 public:
  virtual ~ConfigManager(void);
  
  explicit ConfigManager(QApplication &application, QObject *parent = nullptr);

  //! Get access to the global action manager.
  class ActionManager &ActionManager(void) const noexcept;

  //! Get access to the global theme manager.
  class ThemeManager &ThemeManager(void) const noexcept;

  //! Get access to the global media manager.
  class MediaManager &MediaManager(void) const noexcept;

  //! Get access to the global undo group. All QUndoStacks should be added
  //! to this group so that the global undo/redo toolbar buttons work.
  QUndoGroup &UndoGroup(void) const noexcept;

  //! Get access to the current index.
  const class Index &Index(void) const noexcept;

  //! Change the current index. `db_path` is used to derive the project
  //! settings file path (e.g., /path/to/foo.db -> /path/to/foo.qmx).
  void SetIndex(const class Index &index,
                const QString &db_path = {}) noexcept;

  //! Return the shared file location cache. This is used to compute locations
  //! of things, taking into account the current configuration (tab width, and
  //! tab stops).
  const class FileLocationCache &FileLocationCache(void) const noexcept;

  //! Configuration for item delegates.
  struct ItemDelegateConfig {

    //! If present, then whitespace is replaced by this.
    std::optional<std::string> whitespace_replacement;
  };

  //! Set an item delegate on `view` that pays attention to the theme. This
  //! allows items using `IModel` to present tokens.
  //!
  //! NOTE(pag): This should only be applied to views backed by `IModel`s,
  //!            either directly or by proxy.
  //!
  //! NOTE(pag): This exists as a function of the `ConfigManager` and not the
  //!            `ThemeManager` because tab width might be configurable in the
  //!            future.
  void InstallItemDelegate(QAbstractItemView *view,
                           const ItemDelegateConfig &config={}) const;

  //! Let each manager populate a View menu with its relevant actions.
  void PopulateViewMenu(QMenu *menu);

  //! Get the current tab width (in spaces).
  unsigned TabWidth(void) const noexcept;

  //! Set the tab width (in spaces). Persists to settings.
  void SetTabWidth(unsigned width);

  //! Get/set whether tab stops are used for location calculations.
  bool UseTabStops(void) const noexcept;
  void SetUseTabStops(bool use);

  //! Python interpreter path for agent scripting.
  QString PythonInterpreterPath(void) const;
  void SetPythonInterpreterPath(const QString &path);

  //! Save all persistent settings to disk.
  void SaveSettings(void) const;

  //! Load persistent settings from disk.
  void LoadSettings(void);

  //! Save/load header state (column ordering, widths, etc.) for a named view.
  void SaveHeaderState(const QString &id, const QByteArray &state) const;
  QByteArray LoadHeaderState(const QString &id) const;

  //! Save/load the main window layout (dock positions, sizes, visibility).
  void SaveWindowLayout(const QByteArray &state,
                        const QByteArray &geometry) const;
  bool LoadWindowLayout(QByteArray &state, QByteArray &geometry) const;

  //! Save/load per-project settings (sibling .qmx file next to the .db).
  void SaveProjectSettings(void) const;
  void LoadProjectSettings(void);

  //! Save/load expanded macros (per-project).
  void SaveExpandedMacros(const QSet<mx::RawEntityId> &macros) const;
  QSet<mx::RawEntityId> LoadExpandedMacros(void) const;

  //! Save/load entity highlight colors (per-project).
  using HighlightColorMap =
      std::unordered_map<mx::RawEntityId, std::pair<QColor, QColor>>;
  void SaveHighlightColors(const HighlightColorMap &colors) const;
  HighlightColorMap LoadHighlightColors(void) const;

  //! Save/load spreadsheet sheets (per-project).
  struct SheetColumnInfo {
    QString name;
    QColor color;
    bool clickable{false};
    int width{-1};  // -1 = default/auto, >0 = explicit pixel width.
  };

  struct SheetData {
    int sheet_id{-1};
    QString name;
    QString description;
    QString role;       // "general", "task_list", "findings", etc.
    QString closed_at;  // ISO 8601 timestamp; empty = open.
    QVector<SheetColumnInfo> columns;
    QVector<QVector<QString>> cells;          // cells[row][col] = JSON value
    QHash<int, QColor> row_colors;
  };
  int SaveSheet(const SheetData &sheet) const;
  QVector<SheetData> LoadOpenSheets(void) const;
  void DeleteSheet(int sheet_id) const;

  struct ClosedSheetInfo {
    int sheet_id{-1};
    QString name;
    QString description;
    QString closed_at;
  };
  QVector<ClosedSheetInfo> LoadClosedSheets(void) const;
  SheetData LoadSheetById(int sheet_id) const;

  //! Document storage (per-project). Documents are stored by ID so
  //! multiple sheet cells can reference the same document.
  int CreateDocument(const QString &content = {},
                     const QString &title = {}) const;
  QString LoadDocumentContent(int doc_id) const;
  QString LoadDocumentTitle(int doc_id) const;
  void SaveDocumentContent(int doc_id, const QString &content) const;
  void SaveDocumentTitle(int doc_id, const QString &title) const;

  //! Monotonic counter bumped whenever any document title changes.
  //! DocumentCell rendering compares its cached timestamp against this.
  static quint64 DocumentTitleVersion(void) { return doc_title_version_; }
  static void BumpDocumentTitleVersion(void) { ++doc_title_version_; }

  struct DocumentInfo {
    int doc_id{-1};
    QString title;
    QString description;
    QString created_at;
    QString updated_at;
  };
  QVector<DocumentInfo> LoadAllDocuments(void) const;
  void SaveDocumentDescription(int doc_id, const QString &desc) const;
  void SoftDeleteDocument(int doc_id) const;
  void UndeleteDocument(int doc_id) const;
  int DocumentReferenceCount(int doc_id) const;

  //! Save/load which document tabs are open (per-project).
  void SaveOpenDocumentIds(const QVector<int> &doc_ids) const;
  QVector<int> LoadOpenDocumentIds(void) const;

  //! Save/load navigation history (per-project).
  struct NavigationEntry {
    mx::RawEntityId entity_id{mx::kInvalidEntityId};
    unsigned line{0};
    unsigned column{0};
    QString label;
  };
  void SaveNavigationHistory(
      const std::vector<NavigationEntry> &entries,
      const QString &key = QStringLiteral("Main")) const;
  std::vector<NavigationEntry> LoadNavigationHistory(
      const QString &key = QStringLiteral("Main")) const;

  //! Agent session persistence (per-project).
  struct AgentSessionInfo {
    int64_t session_id{-1};
    QString name;
    QString system_prompt;
    QString backend;
    QString model;
    QString status;
    QString created_at;
    QString updated_at;
    int total_prompt_tokens{0};
    int total_completion_tokens{0};
  };

  int64_t CreateAgentSession(const QString &name,
                             const QString &system_prompt,
                             const QString &backend,
                             const QString &model) const;
  void UpdateAgentSessionStatus(int64_t session_id,
                                const QString &status) const;
  void UpdateAgentSessionTokens(int64_t session_id, int prompt_tokens,
                                int completion_tokens) const;
  QVector<AgentSessionInfo> LoadAgentSessions(void) const;
  QString LoadAgentSessionSystemPrompt(int64_t session_id) const;
  void DeleteAgentSession(int64_t session_id) const;

  //! Agent message persistence (per-project).
  struct AgentMessageInfo {
    int64_t message_id{-1};
    int64_t session_id{-1};
    QString role;
    QString content;
    QString tool_name;
    QString tool_call_id;
    QString tool_args;
    QString tool_result;
    QString timestamp;
    int token_count{0};
  };

  int64_t SaveAgentMessage(int64_t session_id, const QString &role,
                           const QString &content,
                           const QString &tool_name = {},
                           const QString &tool_call_id = {},
                           const QString &tool_args = {},
                           const QString &tool_result = {},
                           int token_count = 0) const;
  QVector<AgentMessageInfo> LoadAgentMessages(int64_t session_id) const;

  //! Agent checkpoint persistence (per-project).
  struct CheckpointInfo {
    int64_t checkpoint_id{-1};
    int64_t session_id{-1};
    QString summary;
    QString created_at;
  };
  int64_t SaveAgentCheckpoint(int64_t session_id,
                              const QString &summary) const;
  QVector<CheckpointInfo> LoadAgentCheckpoints(int64_t session_id) const;

  //! Agent observation persistence (per-project).
  int64_t SaveAgentObservation(int64_t session_id,
                               const QString &content) const;
  QVector<QPair<QString, QString>> LoadAgentObservations(
      int64_t session_id) const;

  //! Document category support (per-project).
  QVector<DocumentInfo> LoadDocumentsByCategory(
      const QString &category) const;
  void SetDocumentCategory(int doc_id, const QString &category) const;

 signals:
  void IndexChanged(const ConfigManager &config_manager);
  void TabWidthChanged(unsigned tab_width);
  void UseTabStopsChanged(bool use_tab_stops);

 private:
  static quint64 doc_title_version_;
};

}  // namespace mx::gui

Q_DECLARE_METATYPE(mx::gui::ConfigManager::SheetData)
