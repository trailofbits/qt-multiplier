// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Managers/ConfigManager.h>

#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/Token.h>

#include <QActionGroup>
#include <QDateTime>
#include <QDir>
#include <QMenu>
#include <QUndoGroup>

#include <iostream>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <multiplier/Index.h>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

#include "ThemedItemDelegate.h"

namespace mx::gui {

quint64 ConfigManager::doc_title_version_ = 0;

namespace {

static constexpr unsigned kDefaultTabWidth = 4u;

static const QString kGlobalDbConn = QStringLiteral("mx_gui_global");
static const QString kProjectDbConn = QStringLiteral("mx_gui_project");

static QString GlobalDbPath(void) {
  return QDir::homePath() + QStringLiteral("/.multiplier.db");
}

static QSqlDatabase OpenDb(const QString &path, const QString &conn_name) {
  if (QSqlDatabase::contains(conn_name)) {
    QSqlDatabase::removeDatabase(conn_name);
  }
  auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn_name);
  db.setDatabaseName(path);
  if (!db.open()) {
    return db;
  }

  QSqlQuery q(db);
  q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_settings ("
      "  key TEXT PRIMARY KEY, value TEXT)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_history ("
      "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  widget_key TEXT NOT NULL,"
      "  entity_id INTEGER NOT NULL,"
      "  label TEXT,"
      "  line INTEGER DEFAULT 0,"
      "  col INTEGER DEFAULT 0)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_header_states ("
      "  key TEXT PRIMARY KEY, state BLOB)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_expanded_macros ("
      "  entity_id INTEGER PRIMARY KEY)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_highlight_colors ("
      "  entity_id INTEGER PRIMARY KEY, fg TEXT, bg TEXT)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_renamed_entities ("
      "  entity_id INTEGER PRIMARY KEY, new_name TEXT NOT NULL)"));

  // Spreadsheet persistence tables.
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_sheets ("
      "  sheet_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  name TEXT NOT NULL,"
      "  description TEXT,"
      "  closed_at TEXT,"
      "  column_order TEXT,"
      "  role TEXT DEFAULT 'general')"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_sheet_columns ("
      "  sheet_id INTEGER NOT NULL,"
      "  col_index INTEGER NOT NULL,"
      "  name TEXT NOT NULL,"
      "  color TEXT,"
      "  clickable INTEGER NOT NULL DEFAULT 0,"
      "  col_width INTEGER NOT NULL DEFAULT -1,"
      "  PRIMARY KEY (sheet_id, col_index))"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_sheet_cells ("
      "  sheet_id INTEGER NOT NULL,"
      "  row_num INTEGER NOT NULL,"
      "  col_index INTEGER NOT NULL,"
      "  value TEXT NOT NULL,"
      "  PRIMARY KEY (sheet_id, row_num, col_index))"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_sheet_row_colors ("
      "  sheet_id INTEGER NOT NULL,"
      "  row_num INTEGER NOT NULL,"
      "  color TEXT NOT NULL,"
      "  PRIMARY KEY (sheet_id, row_num))"));

  // Document storage table.
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_documents ("
      "  doc_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  content TEXT NOT NULL DEFAULT '',"
      "  title TEXT,"
      "  description TEXT,"
      "  created_at TEXT,"
      "  updated_at TEXT,"
      "  deleted INTEGER NOT NULL DEFAULT 0,"
      "  category TEXT DEFAULT 'note',"
      "  format TEXT DEFAULT 'html')"));

  // Migration: add format column for existing databases.
  q.exec(QStringLiteral(
      "ALTER TABLE gui_documents ADD COLUMN format TEXT DEFAULT 'html'"));

  // Cost tracking tables.
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_cost_nodes ("
      "  node_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  session_id INTEGER NOT NULL,"
      "  parent_node_id INTEGER,"
      "  node_type TEXT NOT NULL,"
      "  tool_name TEXT,"
      "  model TEXT,"
      "  input_tokens INTEGER DEFAULT 0,"
      "  output_tokens INTEGER DEFAULT 0,"
      "  duration_ms INTEGER DEFAULT 0,"
      "  cost_usd REAL DEFAULT 0.0,"
      "  started_at TEXT NOT NULL,"
      "  completed_at TEXT,"
      "  message_id INTEGER,"
      "  metadata TEXT)"));

  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_cost_rates ("
      "  model TEXT PRIMARY KEY,"
      "  input_per_million REAL NOT NULL,"
      "  output_per_million REAL NOT NULL,"
      "  updated_at TEXT NOT NULL)"));

  // Cost dependency edges table.
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_cost_edges ("
      "  edge_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  from_node_id INTEGER NOT NULL,"
      "  to_node_id INTEGER NOT NULL,"
      "  edge_type TEXT NOT NULL)"));

  // Seed known model rates.
  q.exec(QStringLiteral(
      "INSERT OR IGNORE INTO gui_cost_rates VALUES "
      "('claude-sonnet-4-20250514', 3.0, 15.0, '2025-05-14')"));
  q.exec(QStringLiteral(
      "INSERT OR IGNORE INTO gui_cost_rates VALUES "
      "('claude-opus-4-20250514', 15.0, 75.0, '2025-05-14')"));
  q.exec(QStringLiteral(
      "INSERT OR IGNORE INTO gui_cost_rates VALUES "
      "('claude-haiku-4-5-20251001', 0.80, 4.0, '2025-10-01')"));
  q.exec(QStringLiteral(
      "INSERT OR IGNORE INTO gui_cost_rates VALUES "
      "('gpt-4o', 2.5, 10.0, '2025-01-01')"));
  q.exec(QStringLiteral(
      "INSERT OR IGNORE INTO gui_cost_rates VALUES "
      "('gpt-4o-mini', 0.15, 0.60, '2025-01-01')"));

  // Agent session tables.
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_agent_sessions ("
      "  session_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  name TEXT NOT NULL,"
      "  system_prompt TEXT,"
      "  backend TEXT NOT NULL,"
      "  model TEXT NOT NULL,"
      "  status TEXT NOT NULL DEFAULT 'active',"
      "  created_at TEXT NOT NULL,"
      "  updated_at TEXT NOT NULL,"
      "  total_prompt_tokens INTEGER DEFAULT 0,"
      "  total_completion_tokens INTEGER DEFAULT 0,"
      "  primary_session_id INTEGER DEFAULT -1)"));

  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_agent_messages ("
      "  message_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  session_id INTEGER NOT NULL,"
      "  role TEXT NOT NULL,"
      "  content TEXT,"
      "  tool_name TEXT,"
      "  tool_call_id TEXT,"
      "  tool_args TEXT,"
      "  tool_result TEXT,"
      "  timestamp TEXT NOT NULL,"
      "  token_count INTEGER DEFAULT 0,"
      "  duration_ms INTEGER DEFAULT 0)"));

  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_agent_checkpoints ("
      "  checkpoint_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  session_id INTEGER NOT NULL,"
      "  summary TEXT NOT NULL,"
      "  created_at TEXT NOT NULL)"));

  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_agent_observations ("
      "  observation_id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "  session_id INTEGER NOT NULL,"
      "  content TEXT NOT NULL,"
      "  created_at TEXT NOT NULL)"));

  return db;
}

static void SetSetting(QSqlDatabase &db, const QString &key,
                        const QString &value) {
  QSqlQuery q(db);
  if (!q.prepare(QStringLiteral(
      "INSERT OR REPLACE INTO gui_settings (key, value) VALUES (?, ?)"))) {
    std::cerr << "SetSetting prepare failed: "
              << q.lastError().text().toStdString() << std::endl;
    return;
  }
  q.addBindValue(key);
  q.addBindValue(value);
  if (!q.exec()) {
    std::cerr << "SetSetting exec failed for key='" << key.toStdString()
              << "': " << q.lastError().text().toStdString() << std::endl;
  }
}

static QString GetSetting(QSqlDatabase &db, const QString &key,
                           const QString &def = {}) {
  QSqlQuery q(db);
  q.prepare(QStringLiteral("SELECT value FROM gui_settings WHERE key = ?"));
  q.addBindValue(key);
  if (q.exec() && q.next()) return q.value(0).toString();
  return def;
}

}  // namespace

class ConfigManagerImpl {
 public:
  class ThemeManager theme_manager;
  class MediaManager media_manager;
  class ActionManager action_manager;
  class FileLocationCache file_location_cache;
  class Index index;
  QUndoGroup undo_group;

  unsigned tab_width{kDefaultTabWidth};
  bool use_tab_stops{true};
  bool settings_loaded{false};
  bool shutting_down{false};
  QString db_path;

  QSqlDatabase global_db;
  QSqlDatabase project_db;

  void RebuildFileLocationCache(void) {
    FileLocationConfiguration config;
    config.tab_width = tab_width;
    config.use_tab_stops = use_tab_stops;
    file_location_cache.clear();
    file_location_cache = FileLocationCache(config);
  }

  inline ConfigManagerImpl(QApplication &application, QObject *self)
      : theme_manager(application, self),
        media_manager(theme_manager, self) {
    global_db = OpenDb(GlobalDbPath(), kGlobalDbConn);
  }
};

ConfigManager::~ConfigManager(void) {
  d->shutting_down = true;
  SaveSettings();

  // Force WAL checkpoint so data is written to the main DB file before
  // the connection is destroyed.
  if (d->global_db.isValid() && d->global_db.isOpen()) {
    QSqlQuery q(d->global_db);
    q.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"));
    d->global_db.close();
  }
  if (d->project_db.isValid() && d->project_db.isOpen()) {
    QSqlQuery q(d->project_db);
    q.exec(QStringLiteral("PRAGMA wal_checkpoint(FULL)"));
    d->project_db.close();
  }
}

ConfigManager::ConfigManager(QApplication &application, QObject *parent)
    : QObject(parent),
      d(std::make_shared<ConfigManagerImpl>(application, this)) {
  // Auto-save on theme change, but not during startup (before
  // LoadSettings) or shutdown (when proxy destruction resets theme).
  using TM = class ThemeManager;
  connect(&(d->theme_manager), &TM::ThemeChanged,
          this, [this] (const TM &) {
            if (d->settings_loaded && !d->shutting_down) {
              SaveSettings();
            }
          });
}

class ActionManager &ConfigManager::ActionManager(void) const noexcept {
  return d->action_manager;
}

class ThemeManager &ConfigManager::ThemeManager(void) const noexcept {
  return d->theme_manager;
}

class MediaManager &ConfigManager::MediaManager(void) const noexcept {
  return d->media_manager;
}

const class Index &ConfigManager::Index(void) const noexcept {
  return d->index;
}

QUndoGroup &ConfigManager::UndoGroup(void) const noexcept {
  return d->undo_group;
}

QString ConfigManager::DatabasePath(void) const {
  return d->db_path;
}

void ConfigManager::NotifyExternalSheetsChanged(void) {
  emit ExternalSheetsChanged();
}

void ConfigManager::NotifyExternalDocumentsChanged(void) {
  emit ExternalDocumentsChanged();
}

void ConfigManager::SetIndex(const class Index &index,
                             const QString &db_path) noexcept {
  if (!d->db_path.isEmpty()) SaveProjectSettings();

  d->file_location_cache.clear();
  d->index = index;
  d->db_path = db_path;

  // Open our own connection to the multiplier .db for GUI tables.
  if (QSqlDatabase::contains(kProjectDbConn)) {
    QSqlDatabase::removeDatabase(kProjectDbConn);
  }
  if (!db_path.isEmpty()) {
    d->project_db = OpenDb(db_path, kProjectDbConn);
    LoadProjectSettings();
  } else {
    d->project_db = QSqlDatabase();
  }

  emit IndexChanged(*this);
}

const class FileLocationCache &
ConfigManager::FileLocationCache(void) const noexcept {
  return d->file_location_cache;
}

void ConfigManager::InstallItemDelegate(
    QAbstractItemView *view, const ItemDelegateConfig &config) const {
  auto set_delegate = [=, this] (const class ThemeManager &self) {
    QAbstractItemDelegate *old_delegate = view->itemDelegate();
    auto theme = self.Theme();
    view->setFont(theme->Font());
    auto new_delegate = new ThemedItemDelegate(
        std::move(theme), config.whitespace_replacement, d->tab_width, view);
    view->setItemDelegate(new_delegate);
    if (old_delegate) old_delegate->deleteLater();
  };
  set_delegate(d->theme_manager);
  using TM2 = class ThemeManager;
  connect(&(d->theme_manager), &TM2::ThemeChanged,
          view, std::move(set_delegate));
}

void ConfigManager::PopulateViewMenu(QMenu *menu) {
  d->theme_manager.PopulateViewMenu(menu);

  menu->addSeparator();

  // --- Font & Tabs submenu ---
  auto *ft_menu = new QMenu(tr("Font && Tabs"), menu);
  menu->addMenu(ft_menu);

  // Font size controls.
  auto *increase_font = new QAction(tr("Increase Font Size"), ft_menu);
  increase_font->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
  ft_menu->addAction(increase_font);
  connect(increase_font, &QAction::triggered, this, [this] () {
    d->theme_manager.SetFontSizeDelta(d->theme_manager.FontSizeDelta() + 1);
  });

  auto *decrease_font = new QAction(tr("Decrease Font Size"), ft_menu);
  decrease_font->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
  ft_menu->addAction(decrease_font);
  connect(decrease_font, &QAction::triggered, this, [this] () {
    d->theme_manager.SetFontSizeDelta(d->theme_manager.FontSizeDelta() - 1);
  });

  auto *reset_font = new QAction(tr("Reset Font Size"), ft_menu);
  reset_font->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  ft_menu->addAction(reset_font);
  connect(reset_font, &QAction::triggered, this, [this] () {
    d->theme_manager.SetFontSizeDelta(0);
  });

  ft_menu->addSeparator();

  // Tab size submenu.
  auto *tab_menu = new QMenu(tr("Tab Size"), ft_menu);
  ft_menu->addMenu(tab_menu);

  auto *tab_group = new QActionGroup(tab_menu);
  tab_group->setExclusive(true);
  for (unsigned i = 1; i <= 16; ++i) {
    auto *action = new QAction(QString::number(i), tab_group);
    action->setCheckable(true);
    action->setChecked(i == d->tab_width);
    connect(action, &QAction::triggered, this,
            [this, i] () { SetTabWidth(i); });
    tab_menu->addAction(action);
  }
  connect(this, &ConfigManager::TabWidthChanged,
          tab_menu, [tab_group] (unsigned w) {
            for (auto *a : tab_group->actions())
              a->setChecked(a->text().toUInt() == w);
          });

  // Use Tab Stops toggle.
  auto *tab_stops_action = new QAction(tr("Use Tab Stops"), ft_menu);
  tab_stops_action->setCheckable(true);
  tab_stops_action->setChecked(d->use_tab_stops);
  ft_menu->addAction(tab_stops_action);
  connect(tab_stops_action, &QAction::toggled,
          this, &ConfigManager::SetUseTabStops);
  connect(this, &ConfigManager::UseTabStopsChanged,
          tab_stops_action, &QAction::setChecked);
}

unsigned ConfigManager::TabWidth(void) const noexcept { return d->tab_width; }

void ConfigManager::SetTabWidth(unsigned width) {
  width = std::clamp(width, 1u, 16u);
  if (d->tab_width == width) return;
  d->tab_width = width;
  d->RebuildFileLocationCache();
  SaveProjectSettings();
  emit TabWidthChanged(width);
}

bool ConfigManager::UseTabStops(void) const noexcept { return d->use_tab_stops; }

void ConfigManager::SetUseTabStops(bool use) {
  if (d->use_tab_stops == use) return;
  d->use_tab_stops = use;
  d->RebuildFileLocationCache();
  SaveSettings();
  emit UseTabStopsChanged(use);
}

QString ConfigManager::PythonInterpreterPath(void) const {
  return GetSetting(d->global_db, QStringLiteral("python_interpreter_path"));
}

void ConfigManager::SetPythonInterpreterPath(const QString &path) {
  SetSetting(d->global_db, QStringLiteral("python_interpreter_path"), path);
}

QString ConfigManager::WorkspacePath(void) const {
  return GetSetting(d->global_db, QStringLiteral("workspace_path"));
}

void ConfigManager::SetWorkspacePath(const QString &path) {
  SetSetting(d->global_db, QStringLiteral("workspace_path"), path);
}

QString ConfigManager::CCompilerPath(void) const {
  return GetSetting(d->global_db, QStringLiteral("c_compiler_path"));
}

void ConfigManager::SetCCompilerPath(const QString &path) {
  SetSetting(d->global_db, QStringLiteral("c_compiler_path"), path);
}

QString ConfigManager::CXXCompilerPath(void) const {
  return GetSetting(d->global_db, QStringLiteral("cxx_compiler_path"));
}

void ConfigManager::SetCXXCompilerPath(const QString &path) {
  SetSetting(d->global_db, QStringLiteral("cxx_compiler_path"), path);
}

QString ConfigManager::SDKRoot(void) const {
  return GetSetting(d->global_db, QStringLiteral("sdk_root"));
}

void ConfigManager::SetSDKRoot(const QString &path) {
  SetSetting(d->global_db, QStringLiteral("sdk_root"), path);
}

// --- Global settings ---

void ConfigManager::SaveSettings(void) const {
  if (!d->global_db.isValid() || !d->global_db.isOpen()) {
    std::cerr << "SaveSettings: global_db not open!" << std::endl;
    return;
  }

  d->global_db.transaction();
  SetSetting(d->global_db, QStringLiteral("use_tab_stops"),
             d->use_tab_stops ? QStringLiteral("1") : QStringLiteral("0"));
  SetSetting(d->global_db, QStringLiteral("font_size_delta"),
             QString::number(d->theme_manager.FontSizeDelta()));
  if (auto theme = d->theme_manager.Theme()) {
    SetSetting(d->global_db, QStringLiteral("theme_id"), theme->Id());
  }
  d->global_db.commit();
}

void ConfigManager::LoadSettings(void) {
  if (!d->global_db.isOpen()) return;

  d->use_tab_stops = GetSetting(d->global_db, QStringLiteral("use_tab_stops"),
                                QStringLiteral("1")) == QStringLiteral("1");
  d->RebuildFileLocationCache();

  int delta = GetSetting(d->global_db, QStringLiteral("font_size_delta"),
                         QStringLiteral("0")).toInt();
  if (delta != 0) d->theme_manager.SetFontSizeDelta(delta);

  auto theme_id = GetSetting(d->global_db, QStringLiteral("theme_id"));
  if (!theme_id.isEmpty()) {
    if (auto theme = d->theme_manager.Find(theme_id)) {
      d->theme_manager.SetTheme(theme);
    }
  }

  d->settings_loaded = true;
}

// --- Per-project settings ---

void ConfigManager::SaveProjectSettings(void) const {
  if (!d->project_db.isOpen()) return;
  SetSetting(d->project_db, QStringLiteral("tab_width"),
             QString::number(d->tab_width));
}

void ConfigManager::LoadProjectSettings(void) {
  if (!d->project_db.isOpen()) return;
  auto tw = GetSetting(d->project_db, QStringLiteral("tab_width"));
  if (!tw.isEmpty()) {
    unsigned val = tw.toUInt();
    if (val >= 1 && val <= 16 && val != d->tab_width) {
      d->tab_width = val;
      d->RebuildFileLocationCache();
      emit TabWidthChanged(val);
    }
  }
}

// --- Header states ---

void ConfigManager::SaveHeaderState(const QString &id,
                                    const QByteArray &state) const {
  auto &db = d->project_db.isOpen() ? d->project_db : d->global_db;
  if (!db.isOpen()) return;
  QSqlQuery q(db);
  q.prepare(QStringLiteral(
      "INSERT OR REPLACE INTO gui_header_states (key, state) VALUES (?, ?)"));
  q.addBindValue(id);
  q.addBindValue(state);
  q.exec();
}

QByteArray ConfigManager::LoadHeaderState(const QString &id) const {
  auto &db = d->project_db.isOpen() ? d->project_db : d->global_db;
  if (!db.isOpen()) return {};
  QSqlQuery q(db);
  q.prepare(QStringLiteral("SELECT state FROM gui_header_states WHERE key = ?"));
  q.addBindValue(id);
  if (q.exec() && q.next()) return q.value(0).toByteArray();
  return {};
}

// --- Window layout ---

void ConfigManager::SaveWindowLayout(const QByteArray &state,
                                     const QByteArray &geometry) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  SetSetting(d->project_db, QStringLiteral("window_state"),
             QString::fromLatin1(state.toBase64()));
  SetSetting(d->project_db, QStringLiteral("window_geometry"),
             QString::fromLatin1(geometry.toBase64()));
}

bool ConfigManager::LoadWindowLayout(QByteArray &state,
                                     QByteArray &geometry) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return false;
  auto s = GetSetting(d->project_db, QStringLiteral("window_state"));
  if (s.isEmpty()) return false;
  state = QByteArray::fromBase64(s.toLatin1());
  geometry = QByteArray::fromBase64(
      GetSetting(d->project_db, QStringLiteral("window_geometry")).toLatin1());
  return true;
}

// --- Expanded macros ---

void ConfigManager::SaveExpandedMacros(const QSet<RawEntityId> &macros) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  d->project_db.transaction();
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("DELETE FROM gui_expanded_macros"));
  q.prepare(QStringLiteral(
      "INSERT INTO gui_expanded_macros (entity_id) VALUES (?)"));
  for (auto id : macros) {
    q.addBindValue(static_cast<qint64>(id));
    q.exec();
  }
  d->project_db.commit();
}

QSet<RawEntityId> ConfigManager::LoadExpandedMacros(void) const {
  if (!d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("SELECT entity_id FROM gui_expanded_macros"));
  QSet<RawEntityId> result;
  while (q.next())
    result.insert(static_cast<RawEntityId>(q.value(0).toULongLong()));
  return result;
}

// --- Renamed entities ---

void ConfigManager::SaveRenamedEntities(
    const QMap<RawEntityId, QString> &renames) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  d->project_db.transaction();
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("DELETE FROM gui_renamed_entities"));
  q.prepare(QStringLiteral(
      "INSERT INTO gui_renamed_entities (entity_id, new_name) VALUES (?, ?)"));
  for (auto it = renames.constBegin(); it != renames.constEnd(); ++it) {
    q.addBindValue(static_cast<qint64>(it.key()));
    q.addBindValue(it.value());
    q.exec();
  }
  d->project_db.commit();
}

QMap<RawEntityId, QString> ConfigManager::LoadRenamedEntities(void) const {
  if (!d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral(
      "SELECT entity_id, new_name FROM gui_renamed_entities"));
  QMap<RawEntityId, QString> result;
  while (q.next()) {
    auto id = static_cast<RawEntityId>(q.value(0).toULongLong());
    result.insert(id, q.value(1).toString());
  }
  return result;
}

// --- Highlight colors ---

void ConfigManager::SaveHighlightColors(const HighlightColorMap &colors) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  d->project_db.transaction();
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("DELETE FROM gui_highlight_colors"));
  q.prepare(QStringLiteral(
      "INSERT INTO gui_highlight_colors (entity_id, fg, bg) VALUES (?, ?, ?)"));
  for (const auto &[id, fg_bg] : colors) {
    q.addBindValue(static_cast<qint64>(id));
    q.addBindValue(fg_bg.first.name(QColor::HexArgb));
    q.addBindValue(fg_bg.second.name(QColor::HexArgb));
    q.exec();
  }
  d->project_db.commit();
}

ConfigManager::HighlightColorMap ConfigManager::LoadHighlightColors(void) const {
  if (!d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("SELECT entity_id, fg, bg FROM gui_highlight_colors"));
  HighlightColorMap result;
  while (q.next()) {
    auto id = static_cast<RawEntityId>(q.value(0).toULongLong());
    QColor fg(q.value(1).toString());
    QColor bg(q.value(2).toString());
    if (fg.isValid() && bg.isValid())
      result.emplace(id, std::make_pair(fg, bg));
  }
  return result;
}

// --- Spreadsheet persistence ---

int ConfigManager::SaveSheet(const SheetData &sheet) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;

  d->project_db.transaction();

  QSqlQuery q(d->project_db);
  int sheet_id = sheet.sheet_id;

  if (sheet_id < 0) {
    // Insert new sheet.
    q.prepare(QStringLiteral(
        "INSERT INTO gui_sheets (name, description, role, closed_at) "
        "VALUES (?, ?, ?, ?)"));
    q.addBindValue(sheet.name);
    q.addBindValue(sheet.description.isEmpty() ? QVariant()
                                                : sheet.description);
    q.addBindValue(sheet.role.isEmpty() ? QStringLiteral("general")
                                        : sheet.role);
    q.addBindValue(sheet.closed_at.isEmpty() ? QVariant()
                                              : sheet.closed_at);
    q.exec();
    sheet_id = q.lastInsertId().toInt();
  } else {
    // Update existing sheet.
    q.prepare(QStringLiteral(
        "UPDATE gui_sheets SET name = ?, description = ?, role = ?, "
        "closed_at = ? WHERE sheet_id = ?"));
    q.addBindValue(sheet.name);
    q.addBindValue(sheet.description.isEmpty() ? QVariant()
                                                : sheet.description);
    q.addBindValue(sheet.role.isEmpty() ? QStringLiteral("general")
                                        : sheet.role);
    q.addBindValue(sheet.closed_at.isEmpty() ? QVariant()
                                              : sheet.closed_at);
    q.addBindValue(sheet_id);
    q.exec();

    // Clear old data.
    q.prepare(QStringLiteral("DELETE FROM gui_sheet_columns WHERE sheet_id = ?"));
    q.addBindValue(sheet_id);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM gui_sheet_cells WHERE sheet_id = ?"));
    q.addBindValue(sheet_id);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM gui_sheet_row_colors WHERE sheet_id = ?"));
    q.addBindValue(sheet_id);
    q.exec();
  }

  // Save columns.
  q.prepare(QStringLiteral(
      "INSERT INTO gui_sheet_columns "
      "(sheet_id, col_index, name, color, clickable, col_width) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  for (int i = 0; i < sheet.columns.size(); ++i) {
    q.addBindValue(sheet_id);
    q.addBindValue(i);
    q.addBindValue(sheet.columns[i].name);
    q.addBindValue(sheet.columns[i].color.isValid()
                   ? sheet.columns[i].color.name(QColor::HexArgb)
                   : QString());
    q.addBindValue(sheet.columns[i].clickable ? 1 : 0);
    q.addBindValue(sheet.columns[i].width);
    q.exec();
  }

  // Save cells.
  q.prepare(QStringLiteral(
      "INSERT INTO gui_sheet_cells (sheet_id, row_num, col_index, value) "
      "VALUES (?, ?, ?, ?)"));
  for (int r = 0; r < sheet.cells.size(); ++r) {
    for (int c = 0; c < sheet.cells[r].size(); ++c) {
      if (!sheet.cells[r][c].isEmpty()) {
        q.addBindValue(sheet_id);
        q.addBindValue(r);
        q.addBindValue(c);
        q.addBindValue(sheet.cells[r][c]);
        q.exec();
      }
    }
  }

  // Save row colors.
  q.prepare(QStringLiteral(
      "INSERT INTO gui_sheet_row_colors (sheet_id, row_num, color) "
      "VALUES (?, ?, ?)"));
  for (auto it = sheet.row_colors.constBegin();
       it != sheet.row_colors.constEnd(); ++it) {
    q.addBindValue(sheet_id);
    q.addBindValue(it.key());
    q.addBindValue(it.value().name(QColor::HexArgb));
    q.exec();
  }

  d->project_db.commit();
  return sheet_id;
}

QVector<ConfigManager::SheetData> ConfigManager::LoadOpenSheets(void) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};

  QVector<SheetData> result;

  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral(
      "SELECT sheet_id, name, description, role FROM gui_sheets "
      "WHERE closed_at IS NULL ORDER BY sheet_id"));

  while (q.next()) {
    SheetData sheet;
    sheet.sheet_id = q.value(0).toInt();
    sheet.name = q.value(1).toString();
    sheet.description = q.value(2).toString();
    sheet.role = q.value(3).toString();

    // Load columns.
    QSqlQuery cq(d->project_db);
    cq.prepare(QStringLiteral(
        "SELECT col_index, name, color, clickable, col_width FROM gui_sheet_columns "
        "WHERE sheet_id = ? ORDER BY col_index"));
    cq.addBindValue(sheet.sheet_id);
    cq.exec();
    while (cq.next()) {
      SheetColumnInfo ci;
      ci.name = cq.value(1).toString();
      auto col_color_str = cq.value(2).toString();
      ci.color = col_color_str.isEmpty() ? QColor() : QColor(col_color_str);
      ci.clickable = cq.value(3).toInt() != 0;
      ci.width = cq.value(4).toInt();
      sheet.columns.push_back(ci);
    }

    // Load cells — find max row.
    QSqlQuery rq(d->project_db);
    rq.prepare(QStringLiteral(
        "SELECT MAX(row_num) FROM gui_sheet_cells WHERE sheet_id = ?"));
    rq.addBindValue(sheet.sheet_id);
    rq.exec();
    int max_row = -1;
    if (rq.next()) {
      max_row = rq.value(0).toInt();
    }

    int num_cols = static_cast<int>(sheet.columns.size());
    sheet.cells.resize(max_row + 1);
    for (auto &row : sheet.cells) {
      row.resize(num_cols);
    }

    QSqlQuery cellq(d->project_db);
    cellq.prepare(QStringLiteral(
        "SELECT row_num, col_index, value FROM gui_sheet_cells "
        "WHERE sheet_id = ?"));
    cellq.addBindValue(sheet.sheet_id);
    cellq.exec();
    while (cellq.next()) {
      int r = cellq.value(0).toInt();
      int c = cellq.value(1).toInt();
      if (r < sheet.cells.size() && c < num_cols) {
        sheet.cells[r][c] = cellq.value(2).toString();
      }
    }

    // Load row colors.
    QSqlQuery rcq(d->project_db);
    rcq.prepare(QStringLiteral(
        "SELECT row_num, color FROM gui_sheet_row_colors WHERE sheet_id = ?"));
    rcq.addBindValue(sheet.sheet_id);
    rcq.exec();
    while (rcq.next()) {
      int row = rcq.value(0).toInt();
      QColor color(rcq.value(1).toString());
      if (color.isValid()) {
        sheet.row_colors[row] = color;
      }
    }

    result.push_back(std::move(sheet));
  }

  return result;
}

void ConfigManager::DeleteSheet(int sheet_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;

  d->project_db.transaction();
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral("DELETE FROM gui_sheet_cells WHERE sheet_id = ?"));
  q.addBindValue(sheet_id);
  q.exec();
  q.prepare(QStringLiteral("DELETE FROM gui_sheet_columns WHERE sheet_id = ?"));
  q.addBindValue(sheet_id);
  q.exec();
  q.prepare(QStringLiteral("DELETE FROM gui_sheet_row_colors WHERE sheet_id = ?"));
  q.addBindValue(sheet_id);
  q.exec();
  q.prepare(QStringLiteral("DELETE FROM gui_sheets WHERE sheet_id = ?"));
  q.addBindValue(sheet_id);
  q.exec();
  d->project_db.commit();
}

QVector<ConfigManager::ClosedSheetInfo>
ConfigManager::LoadClosedSheets(void) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};

  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral(
      "SELECT sheet_id, name, description, closed_at FROM gui_sheets "
      "WHERE closed_at IS NOT NULL ORDER BY closed_at DESC"));

  QVector<ClosedSheetInfo> result;
  while (q.next()) {
    ClosedSheetInfo info;
    info.sheet_id = q.value(0).toInt();
    info.name = q.value(1).toString();
    info.description = q.value(2).toString();
    info.closed_at = q.value(3).toString();
    result.push_back(std::move(info));
  }
  return result;
}

ConfigManager::SheetData ConfigManager::LoadSheetById(int sheet_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};

  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT sheet_id, name, description, role FROM gui_sheets "
      "WHERE sheet_id = ?"));
  q.addBindValue(sheet_id);
  q.exec();
  if (!q.next()) return {};

  SheetData sheet;
  sheet.sheet_id = q.value(0).toInt();
  sheet.name = q.value(1).toString();
  sheet.description = q.value(2).toString();
  sheet.role = q.value(3).toString();

  // Load columns.
  QSqlQuery cq(d->project_db);
  cq.prepare(QStringLiteral(
      "SELECT col_index, name, color, clickable, col_width FROM gui_sheet_columns "
      "WHERE sheet_id = ? ORDER BY col_index"));
  cq.addBindValue(sheet_id);
  cq.exec();
  while (cq.next()) {
    SheetColumnInfo ci;
    ci.name = cq.value(1).toString();
    auto col_color_str = cq.value(2).toString();
    ci.color = col_color_str.isEmpty() ? QColor() : QColor(col_color_str);
    ci.clickable = cq.value(3).toInt() != 0;
    sheet.columns.push_back(ci);
  }

  // Load cells.
  QSqlQuery rq(d->project_db);
  rq.prepare(QStringLiteral(
      "SELECT MAX(row_num) FROM gui_sheet_cells WHERE sheet_id = ?"));
  rq.addBindValue(sheet_id);
  rq.exec();
  int max_row = -1;
  if (rq.next()) {
    max_row = rq.value(0).toInt();
  }

  int num_cols = static_cast<int>(sheet.columns.size());
  sheet.cells.resize(max_row + 1);
  for (auto &row : sheet.cells) {
    row.resize(num_cols);
  }

  QSqlQuery cellq(d->project_db);
  cellq.prepare(QStringLiteral(
      "SELECT row_num, col_index, value FROM gui_sheet_cells "
      "WHERE sheet_id = ?"));
  cellq.addBindValue(sheet_id);
  cellq.exec();
  while (cellq.next()) {
    int r = cellq.value(0).toInt();
    int c = cellq.value(1).toInt();
    if (r < sheet.cells.size() && c < num_cols) {
      sheet.cells[r][c] = cellq.value(2).toString();
    }
  }

  // Load row colors.
  QSqlQuery rcq(d->project_db);
  rcq.prepare(QStringLiteral(
      "SELECT row_num, color FROM gui_sheet_row_colors WHERE sheet_id = ?"));
  rcq.addBindValue(sheet_id);
  rcq.exec();
  while (rcq.next()) {
    int row = rcq.value(0).toInt();
    QColor color(rcq.value(1).toString());
    if (color.isValid()) {
      sheet.row_colors[row] = color;
    }
  }

  // Clear the closed_at timestamp so it's now open.
  q.prepare(QStringLiteral(
      "UPDATE gui_sheets SET closed_at = NULL WHERE sheet_id = ?"));
  q.addBindValue(sheet_id);
  q.exec();

  return sheet;
}

// --- Documents ---

int ConfigManager::CreateDocument(const QString &content,
                                  const QString &title,
                                  const QString &format) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_documents (content, title, format, created_at, updated_at) "
      "VALUES (?, ?, ?, ?, ?)"));
  q.addBindValue(content.isNull() ? QStringLiteral("") : content);
  q.addBindValue(title);
  q.addBindValue(format.isEmpty() ? QStringLiteral("html") : format);
  q.addBindValue(now);
  q.addBindValue(now);
  if (!q.exec()) return -1;
  auto id = q.lastInsertId();
  return id.isValid() ? id.toInt() : -1;
}

QString ConfigManager::LoadDocumentContent(int doc_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT content FROM gui_documents WHERE doc_id = ?"));
  q.addBindValue(doc_id);
  q.exec();
  return q.next() ? q.value(0).toString() : QString();
}

QString ConfigManager::LoadDocumentTitle(int doc_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT title FROM gui_documents WHERE doc_id = ?"));
  q.addBindValue(doc_id);
  q.exec();
  return q.next() ? q.value(0).toString() : QString();
}

QString ConfigManager::LoadDocumentFormat(int doc_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT format FROM gui_documents WHERE doc_id = ?"));
  q.addBindValue(doc_id);
  q.exec();
  if (q.next()) {
    auto fmt = q.value(0).toString();
    return fmt.isEmpty() ? QStringLiteral("html") : fmt;
  }
  return QStringLiteral("html");
}

void ConfigManager::SaveDocumentContent(int doc_id,
                                        const QString &content) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_documents SET content = ?, updated_at = ? "
      "WHERE doc_id = ?"));
  q.addBindValue(content);
  q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  q.addBindValue(doc_id);
  q.exec();
}

void ConfigManager::SaveDocumentTitle(int doc_id,
                                      const QString &title) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_documents SET title = ? WHERE doc_id = ?"));
  q.addBindValue(title);
  q.addBindValue(doc_id);
  q.exec();
  BumpDocumentTitleVersion();
}

QVector<ConfigManager::DocumentInfo>
ConfigManager::LoadAllDocuments(void) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral(
      "SELECT doc_id, title, description, created_at, updated_at, format, "
      "category FROM gui_documents WHERE deleted = 0 ORDER BY doc_id"));
  QVector<DocumentInfo> result;
  while (q.next()) {
    DocumentInfo info;
    info.doc_id = q.value(0).toInt();
    info.title = q.value(1).toString();
    info.description = q.value(2).toString();
    info.created_at = q.value(3).toString();
    info.updated_at = q.value(4).toString();
    info.format = q.value(5).toString();
    if (info.format.isEmpty()) info.format = QStringLiteral("html");
    info.category = q.value(6).toString();
    if (info.category.isEmpty()) info.category = QStringLiteral("note");
    result.push_back(std::move(info));
  }
  return result;
}

void ConfigManager::SaveDocumentDescription(int doc_id,
                                            const QString &desc) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_documents SET description = ? WHERE doc_id = ?"));
  q.addBindValue(desc);
  q.addBindValue(doc_id);
  q.exec();
}

void ConfigManager::SoftDeleteDocument(int doc_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_documents SET deleted = 1 WHERE doc_id = ?"));
  q.addBindValue(doc_id);
  q.exec();
}

void ConfigManager::UndeleteDocument(int doc_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_documents SET deleted = 0 WHERE doc_id = ?"));
  q.addBindValue(doc_id);
  q.exec();
}

int ConfigManager::DocumentReferenceCount(int doc_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return 0;
  // Search for cells containing this doc_id as a document reference.
  // The JSON format is {"t":"doc","id":N,...}
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT COUNT(*) FROM gui_sheet_cells "
      "WHERE value LIKE ?"));
  q.addBindValue(QStringLiteral("%%\"t\":\"doc\",\"id\":%1%%").arg(doc_id));
  q.exec();
  return q.next() ? q.value(0).toInt() : 0;
}

void ConfigManager::SaveOpenDocumentIds(
    const QVector<int> &doc_ids) const {
  QString val;
  for (int i = 0; i < doc_ids.size(); ++i) {
    if (i > 0) val += QLatin1Char(',');
    val += QString::number(doc_ids[i]);
  }
  SaveHeaderState(QStringLiteral("open_doc_ids"),
                  val.toUtf8());
}

QVector<int> ConfigManager::LoadOpenDocumentIds(void) const {
  auto data = LoadHeaderState(QStringLiteral("open_doc_ids"));
  if (data.isEmpty()) return {};
  QVector<int> result;
  for (const auto &s : QString::fromUtf8(data).split(QLatin1Char(','))) {
    bool ok = false;
    int id = s.toInt(&ok);
    if (ok && id > 0) result.push_back(id);
  }
  return result;
}

// --- Navigation history ---

void ConfigManager::SaveNavigationHistory(
    const std::vector<NavigationEntry> &entries, const QString &key) const {
  if (!d || d->shutting_down) return;
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral("DELETE FROM gui_history WHERE widget_key = ?"));
  q.addBindValue(key);
  q.exec();

  q.prepare(QStringLiteral(
      "INSERT INTO gui_history (widget_key, entity_id, label, line, col)"
      " VALUES (?, ?, ?, ?, ?)"));
  for (const auto &e : entries) {
    q.addBindValue(key);
    q.addBindValue(qulonglong(e.entity_id));
    q.addBindValue(e.label);
    q.addBindValue(e.line);
    q.addBindValue(e.column);
    q.exec();
  }
}

std::vector<ConfigManager::NavigationEntry>
ConfigManager::LoadNavigationHistory(const QString &key) const {
  if (!d || !d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT entity_id, label, line, col FROM gui_history "
      "WHERE widget_key = ? ORDER BY id ASC"));
  q.addBindValue(key);
  std::vector<NavigationEntry> result;
  if (q.exec()) {
    while (q.next()) {
      NavigationEntry e;
      e.entity_id = static_cast<RawEntityId>(q.value(0).toULongLong());
      e.label = q.value(1).toString();
      e.line = q.value(2).toUInt();
      e.column = q.value(3).toUInt();
      result.push_back(std::move(e));
    }
  }
  return result;
}

// --- Agent sessions ---

int64_t ConfigManager::CreateAgentSession(
    const QString &name, const QString &system_prompt,
    const QString &backend, const QString &model,
    int64_t primary_session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_agent_sessions "
      "(name, system_prompt, backend, model, status, created_at, updated_at, "
      "primary_session_id) "
      "VALUES (?, ?, ?, ?, 'active', ?, ?, ?)"));
  q.addBindValue(name);
  q.addBindValue(system_prompt);
  q.addBindValue(backend);
  q.addBindValue(model);
  q.addBindValue(now);
  q.addBindValue(now);
  q.addBindValue(static_cast<qlonglong>(primary_session_id));
  if (!q.exec()) return -1;
  auto id = q.lastInsertId();
  return id.isValid() ? id.toLongLong() : -1;
}

void ConfigManager::UpdateAgentSessionStatus(int64_t session_id,
                                             const QString &status) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_agent_sessions SET status = ?, updated_at = ? "
      "WHERE session_id = ?"));
  q.addBindValue(status);
  q.addBindValue(now);
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
}

void ConfigManager::UpdateAgentSessionTokens(
    int64_t session_id, int prompt_tokens, int completion_tokens) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_agent_sessions SET "
      "total_prompt_tokens = total_prompt_tokens + ?, "
      "total_completion_tokens = total_completion_tokens + ?, "
      "updated_at = ? WHERE session_id = ?"));
  q.addBindValue(prompt_tokens);
  q.addBindValue(completion_tokens);
  q.addBindValue(now);
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
}

QVector<ConfigManager::AgentSessionInfo>
ConfigManager::LoadAgentSessions(void) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral(
      "SELECT session_id, name, system_prompt, backend, model, status, "
      "created_at, updated_at, total_prompt_tokens, total_completion_tokens "
      "FROM gui_agent_sessions ORDER BY session_id DESC"));
  QVector<AgentSessionInfo> result;
  while (q.next()) {
    AgentSessionInfo info;
    info.session_id = q.value(0).toLongLong();
    info.name = q.value(1).toString();
    info.system_prompt = q.value(2).toString();
    info.backend = q.value(3).toString();
    info.model = q.value(4).toString();
    info.status = q.value(5).toString();
    info.created_at = q.value(6).toString();
    info.updated_at = q.value(7).toString();
    info.total_prompt_tokens = q.value(8).toInt();
    info.total_completion_tokens = q.value(9).toInt();
    result.push_back(std::move(info));
  }
  return result;
}

QString ConfigManager::LoadAgentSessionSystemPrompt(
    int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT system_prompt FROM gui_agent_sessions WHERE session_id = ?"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  return q.next() ? q.value(0).toString() : QString();
}

void ConfigManager::DeleteAgentSession(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  d->project_db.transaction();
  QSqlQuery q(d->project_db);
  auto sid = static_cast<qlonglong>(session_id);

  q.prepare(QStringLiteral(
      "DELETE FROM gui_agent_messages WHERE session_id = ?"));
  q.addBindValue(sid);
  q.exec();

  q.prepare(QStringLiteral(
      "DELETE FROM gui_agent_checkpoints WHERE session_id = ?"));
  q.addBindValue(sid);
  q.exec();

  q.prepare(QStringLiteral(
      "DELETE FROM gui_agent_observations WHERE session_id = ?"));
  q.addBindValue(sid);
  q.exec();

  // Delete cost edges that reference nodes in this session.
  q.prepare(QStringLiteral(
      "DELETE FROM gui_cost_edges WHERE from_node_id IN "
      "(SELECT node_id FROM gui_cost_nodes WHERE session_id = ?) "
      "OR to_node_id IN "
      "(SELECT node_id FROM gui_cost_nodes WHERE session_id = ?)"));
  q.addBindValue(sid);
  q.addBindValue(sid);
  q.exec();

  q.prepare(QStringLiteral(
      "DELETE FROM gui_cost_nodes WHERE session_id = ?"));
  q.addBindValue(sid);
  q.exec();

  q.prepare(QStringLiteral(
      "DELETE FROM gui_agent_sessions WHERE session_id = ?"));
  q.addBindValue(sid);
  q.exec();

  d->project_db.commit();
}

// --- Agent messages ---

int64_t ConfigManager::SaveAgentMessage(
    int64_t session_id, const QString &role, const QString &content,
    const QString &tool_name, const QString &tool_call_id,
    const QString &tool_args, const QString &tool_result,
    int token_count, int duration_ms) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_agent_messages "
      "(session_id, role, content, tool_name, tool_call_id, "
      "tool_args, tool_result, timestamp, token_count, duration_ms) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.addBindValue(role);
  q.addBindValue(content);
  q.addBindValue(tool_name.isEmpty() ? QVariant() : tool_name);
  q.addBindValue(tool_call_id.isEmpty() ? QVariant() : tool_call_id);
  q.addBindValue(tool_args.isEmpty() ? QVariant() : tool_args);
  q.addBindValue(tool_result.isEmpty() ? QVariant() : tool_result);
  q.addBindValue(now);
  q.addBindValue(token_count);
  q.addBindValue(duration_ms);
  if (!q.exec()) return -1;
  auto id = q.lastInsertId();
  return id.isValid() ? id.toLongLong() : -1;
}

QVector<ConfigManager::AgentMessageInfo>
ConfigManager::LoadAgentMessages(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT message_id, session_id, role, content, tool_name, "
      "tool_call_id, tool_args, tool_result, timestamp, token_count, "
      "duration_ms "
      "FROM gui_agent_messages WHERE session_id = ? ORDER BY message_id"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<AgentMessageInfo> result;
  while (q.next()) {
    AgentMessageInfo info;
    info.message_id = q.value(0).toLongLong();
    info.session_id = q.value(1).toLongLong();
    info.role = q.value(2).toString();
    info.content = q.value(3).toString();
    info.tool_name = q.value(4).toString();
    info.tool_call_id = q.value(5).toString();
    info.tool_args = q.value(6).toString();
    info.tool_result = q.value(7).toString();
    info.timestamp = q.value(8).toString();
    info.token_count = q.value(9).toInt();
    info.duration_ms = q.value(10).toInt();
    result.push_back(std::move(info));
  }
  return result;
}

// --- Agent checkpoints ---

int64_t ConfigManager::SaveAgentCheckpoint(int64_t session_id,
                                           const QString &summary) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_agent_checkpoints "
      "(session_id, summary, created_at) VALUES (?, ?, ?)"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.addBindValue(summary);
  q.addBindValue(now);
  if (!q.exec()) return -1;
  auto id = q.lastInsertId();
  return id.isValid() ? id.toLongLong() : -1;
}

QVector<ConfigManager::CheckpointInfo>
ConfigManager::LoadAgentCheckpoints(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT checkpoint_id, session_id, summary, created_at "
      "FROM gui_agent_checkpoints WHERE session_id = ? "
      "ORDER BY checkpoint_id"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<CheckpointInfo> result;
  while (q.next()) {
    CheckpointInfo info;
    info.checkpoint_id = q.value(0).toLongLong();
    info.session_id = q.value(1).toLongLong();
    info.summary = q.value(2).toString();
    info.created_at = q.value(3).toString();
    result.push_back(std::move(info));
  }
  return result;
}

// --- Agent observations ---

int64_t ConfigManager::SaveAgentObservation(int64_t session_id,
                                            const QString &content) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_agent_observations "
      "(session_id, content, created_at) VALUES (?, ?, ?)"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.addBindValue(content);
  q.addBindValue(now);
  if (!q.exec()) return -1;
  auto id = q.lastInsertId();
  return id.isValid() ? id.toLongLong() : -1;
}

QVector<QPair<QString, QString>>
ConfigManager::LoadAgentObservations(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT content, created_at FROM gui_agent_observations "
      "WHERE session_id = ? ORDER BY observation_id"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<QPair<QString, QString>> result;
  while (q.next()) {
    result.push_back({q.value(0).toString(), q.value(1).toString()});
  }
  return result;
}

// --- Document categories ---

QVector<ConfigManager::DocumentInfo>
ConfigManager::LoadDocumentsByCategory(const QString &category) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT doc_id, title, description, created_at, updated_at, format "
      "FROM gui_documents WHERE deleted = 0 AND category = ? "
      "ORDER BY doc_id"));
  q.addBindValue(category);
  q.exec();
  QVector<DocumentInfo> result;
  while (q.next()) {
    DocumentInfo info;
    info.doc_id = q.value(0).toInt();
    info.title = q.value(1).toString();
    info.description = q.value(2).toString();
    info.created_at = q.value(3).toString();
    info.updated_at = q.value(4).toString();
    info.format = q.value(5).toString();
    if (info.format.isEmpty()) info.format = QStringLiteral("html");
    info.category = category;
    result.push_back(std::move(info));
  }
  return result;
}

void ConfigManager::SetDocumentCategory(int doc_id,
                                        const QString &category) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_documents SET category = ? WHERE doc_id = ?"));
  q.addBindValue(category);
  q.addBindValue(doc_id);
  q.exec();
}

// --- Cost tracking ---

int64_t ConfigManager::CreateCostNode(
    int64_t session_id, int64_t parent_node_id,
    const QString &node_type, const QString &tool_name,
    const QString &model) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return -1;
  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_cost_nodes "
      "(session_id, parent_node_id, node_type, tool_name, model, started_at) "
      "VALUES (?, ?, ?, ?, ?, ?)"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.addBindValue(parent_node_id >= 0 ? QVariant(static_cast<qlonglong>(parent_node_id))
                                     : QVariant());
  q.addBindValue(node_type);
  q.addBindValue(tool_name.isEmpty() ? QVariant() : tool_name);
  q.addBindValue(model.isEmpty() ? QVariant() : model);
  q.addBindValue(now);
  if (!q.exec()) return -1;
  auto id = q.lastInsertId();
  return id.isValid() ? id.toLongLong() : -1;
}

void ConfigManager::CompleteCostNode(
    int64_t node_id, int input_tokens, int output_tokens,
    int duration_ms, const QString &metadata) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  if (node_id < 0) return;

  auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

  // Look up the model for this node to compute cost.
  double cost_usd = 0.0;
  QSqlQuery mq(d->project_db);
  mq.prepare(QStringLiteral("SELECT model FROM gui_cost_nodes WHERE node_id = ?"));
  mq.addBindValue(static_cast<qlonglong>(node_id));
  if (mq.exec() && mq.next()) {
    auto model = mq.value(0).toString();
    if (!model.isEmpty()) {
      double input_rate = LookupCostRate(model, true);
      double output_rate = LookupCostRate(model, false);
      cost_usd = (input_tokens * input_rate + output_tokens * output_rate)
                 / 1000000.0;
    }
  }

  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "UPDATE gui_cost_nodes SET "
      "input_tokens = ?, output_tokens = ?, duration_ms = ?, "
      "cost_usd = ?, completed_at = ?, metadata = ? "
      "WHERE node_id = ?"));
  q.addBindValue(input_tokens);
  q.addBindValue(output_tokens);
  q.addBindValue(duration_ms);
  q.addBindValue(cost_usd);
  q.addBindValue(now);
  q.addBindValue(metadata.isEmpty() ? QVariant() : metadata);
  q.addBindValue(static_cast<qlonglong>(node_id));
  q.exec();
}

double ConfigManager::LookupCostRate(const QString &model,
                                     bool is_input) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return 0.0;
  QSqlQuery q(d->project_db);
  auto col = is_input ? QStringLiteral("input_per_million")
                      : QStringLiteral("output_per_million");
  q.prepare(QStringLiteral("SELECT %1 FROM gui_cost_rates "
                           "WHERE ? LIKE '%%' || model || '%%' "
                           "ORDER BY length(model) DESC LIMIT 1").arg(col));
  q.addBindValue(model);
  if (q.exec() && q.next()) {
    return q.value(0).toDouble();
  }
  return 0.0;
}

QVector<ConfigManager::CostNodeInfo>
ConfigManager::LoadCostNodes(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT node_id, session_id, parent_node_id, node_type, tool_name, "
      "model, input_tokens, output_tokens, duration_ms, cost_usd, "
      "started_at, completed_at "
      "FROM gui_cost_nodes WHERE session_id = ? ORDER BY started_at"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<CostNodeInfo> result;
  while (q.next()) {
    CostNodeInfo info;
    info.node_id = q.value(0).toLongLong();
    info.session_id = q.value(1).toLongLong();
    info.parent_node_id = q.value(2).isNull() ? -1 : q.value(2).toLongLong();
    info.node_type = q.value(3).toString();
    info.tool_name = q.value(4).toString();
    info.model = q.value(5).toString();
    info.input_tokens = q.value(6).toInt();
    info.output_tokens = q.value(7).toInt();
    info.duration_ms = q.value(8).toInt();
    info.cost_usd = q.value(9).toDouble();
    info.started_at = q.value(10).toString();
    info.completed_at = q.value(11).toString();
    result.push_back(std::move(info));
  }
  return result;
}

ConfigManager::CostSummary
ConfigManager::LoadCostSummary(int64_t session_id) const {
  CostSummary summary;
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return summary;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT COALESCE(SUM(cost_usd), 0), "
      "COALESCE(SUM(input_tokens), 0), "
      "COALESCE(SUM(output_tokens), 0), "
      "COALESCE(SUM(duration_ms), 0), "
      "COALESCE(SUM(CASE WHEN node_type = 'llm_call' THEN 1 ELSE 0 END), 0), "
      "COALESCE(SUM(CASE WHEN node_type = 'tool_call' THEN 1 ELSE 0 END), 0) "
      "FROM gui_cost_nodes WHERE session_id = ?"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  if (q.exec() && q.next()) {
    summary.total_cost_usd = q.value(0).toDouble();
    summary.total_input_tokens = q.value(1).toInt();
    summary.total_output_tokens = q.value(2).toInt();
    summary.total_duration_ms = q.value(3).toInt();
    summary.llm_call_count = q.value(4).toInt();
    summary.tool_call_count = q.value(5).toInt();
  }
  return summary;
}

QVector<ConfigManager::ToolCostBreakdown>
ConfigManager::LoadToolCostBreakdown(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT tool_name, COUNT(*) as call_count, "
      "COALESCE(SUM(cost_usd), 0), "
      "COALESCE(SUM(duration_ms), 0), "
      "COALESCE(AVG(duration_ms), 0) "
      "FROM gui_cost_nodes "
      "WHERE session_id = ? AND node_type = 'tool_call' AND tool_name IS NOT NULL "
      "GROUP BY tool_name ORDER BY SUM(cost_usd) DESC"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<ToolCostBreakdown> result;
  while (q.next()) {
    ToolCostBreakdown info;
    info.tool_name = q.value(0).toString();
    info.call_count = q.value(1).toInt();
    info.total_cost_usd = q.value(2).toDouble();
    info.total_duration_ms = q.value(3).toInt();
    info.avg_duration_ms = q.value(4).toInt();
    result.push_back(std::move(info));
  }
  return result;
}

QVector<ConfigManager::ToolStatistics>
ConfigManager::LoadToolStatistics(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT tool_name, COUNT(*) as calls, "
      "COALESCE(SUM(duration_ms), 0), "
      "COALESCE(MIN(duration_ms), 0), "
      "COALESCE(MAX(duration_ms), 0), "
      "COALESCE(AVG(duration_ms), 0), "
      "COALESCE(SUM(cost_usd), 0) "
      "FROM gui_cost_nodes "
      "WHERE session_id = ? AND node_type = 'tool_call' "
      "AND tool_name IS NOT NULL "
      "GROUP BY tool_name ORDER BY calls DESC"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<ToolStatistics> result;
  while (q.next()) {
    ToolStatistics info;
    info.tool_name = q.value(0).toString();
    info.call_count = q.value(1).toInt();
    info.total_duration_ms = q.value(2).toInt();
    info.min_duration_ms = q.value(3).toInt();
    info.max_duration_ms = q.value(4).toInt();
    info.avg_duration_ms = q.value(5).toInt();
    info.total_cost_usd = q.value(6).toDouble();
    result.push_back(std::move(info));
  }
  return result;
}

QVector<ConfigManager::RoleCostBreakdown>
ConfigManager::LoadRoleCostBreakdown(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT node_type, model, COUNT(*) as call_count, "
      "COALESCE(SUM(input_tokens), 0), "
      "COALESCE(SUM(output_tokens), 0), "
      "COALESCE(SUM(cost_usd), 0) "
      "FROM gui_cost_nodes WHERE session_id = ? "
      "GROUP BY node_type, model"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<RoleCostBreakdown> result;
  while (q.next()) {
    RoleCostBreakdown info;
    info.node_type = q.value(0).toString();
    info.model = q.value(1).toString();
    info.call_count = q.value(2).toInt();
    info.total_input_tokens = q.value(3).toInt();
    info.total_output_tokens = q.value(4).toInt();
    info.total_cost_usd = q.value(5).toDouble();
    result.push_back(std::move(info));
  }
  return result;
}

// --- Cost edge tracking ---

void ConfigManager::CreateCostEdge(int64_t from_node_id, int64_t to_node_id,
                                   const QString &edge_type) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return;
  if (from_node_id < 0 || to_node_id < 0) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "INSERT INTO gui_cost_edges (from_node_id, to_node_id, edge_type) "
      "VALUES (?, ?, ?)"));
  q.addBindValue(static_cast<qlonglong>(from_node_id));
  q.addBindValue(static_cast<qlonglong>(to_node_id));
  q.addBindValue(edge_type);
  q.exec();
}

QVector<ConfigManager::CostEdgeInfo>
ConfigManager::LoadCostEdges(int64_t session_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT e.edge_id, e.from_node_id, e.to_node_id, e.edge_type "
      "FROM gui_cost_edges e "
      "JOIN gui_cost_nodes n ON e.from_node_id = n.node_id "
      "WHERE n.session_id = ? "
      "ORDER BY e.edge_id"));
  q.addBindValue(static_cast<qlonglong>(session_id));
  q.exec();
  QVector<CostEdgeInfo> result;
  while (q.next()) {
    CostEdgeInfo info;
    info.edge_id = q.value(0).toLongLong();
    info.from_node_id = q.value(1).toLongLong();
    info.to_node_id = q.value(2).toLongLong();
    info.edge_type = q.value(3).toString();
    result.push_back(std::move(info));
  }
  return result;
}

QVector<ConfigManager::CostNodeInfo>
ConfigManager::LoadUpstreamNodes(int64_t node_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return {};

  // Walk up the parent chain from node_id to root via parent_node_id.
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "WITH RECURSIVE ancestors(nid) AS ("
      "  SELECT ? "
      "  UNION ALL "
      "  SELECT n.parent_node_id FROM gui_cost_nodes n "
      "  JOIN ancestors a ON n.node_id = a.nid "
      "  WHERE n.parent_node_id IS NOT NULL"
      ") "
      "SELECT node_id, session_id, parent_node_id, node_type, tool_name, "
      "model, input_tokens, output_tokens, duration_ms, cost_usd, "
      "started_at, completed_at "
      "FROM gui_cost_nodes WHERE node_id IN (SELECT nid FROM ancestors) "
      "ORDER BY started_at"));
  q.addBindValue(static_cast<qlonglong>(node_id));
  q.exec();

  QVector<CostNodeInfo> result;
  while (q.next()) {
    CostNodeInfo info;
    info.node_id = q.value(0).toLongLong();
    info.session_id = q.value(1).toLongLong();
    info.parent_node_id = q.value(2).isNull() ? -1 : q.value(2).toLongLong();
    info.node_type = q.value(3).toString();
    info.tool_name = q.value(4).toString();
    info.model = q.value(5).toString();
    info.input_tokens = q.value(6).toInt();
    info.output_tokens = q.value(7).toInt();
    info.duration_ms = q.value(8).toInt();
    info.cost_usd = q.value(9).toDouble();
    info.started_at = q.value(10).toString();
    info.completed_at = q.value(11).toString();
    result.push_back(std::move(info));
  }
  return result;
}

double ConfigManager::ComputeTrueCost(int64_t node_id) const {
  if (!d->project_db.isValid() || !d->project_db.isOpen()) return 0.0;
  if (node_id < 0) return 0.0;

  // Helper: compute subtree cost for a given node.
  auto subtree_cost = [this](int64_t root_id) -> double {
    QSqlQuery sq(d->project_db);
    sq.prepare(QStringLiteral(
        "WITH RECURSIVE subtree(nid) AS ("
        "  SELECT ? "
        "  UNION ALL "
        "  SELECT n.node_id FROM gui_cost_nodes n "
        "  JOIN subtree s ON n.parent_node_id = s.nid"
        ") "
        "SELECT COALESCE(SUM(cost_usd), 0) FROM gui_cost_nodes "
        "WHERE node_id IN (SELECT nid FROM subtree)"));
    sq.addBindValue(static_cast<qlonglong>(root_id));
    if (sq.exec() && sq.next()) {
      return sq.value(0).toDouble();
    }
    return 0.0;
  };

  // Trace path from node to root via parent_node_id.
  QVector<int64_t> path;
  {
    int64_t cur = node_id;
    while (cur >= 0) {
      path.append(cur);
      QSqlQuery pq(d->project_db);
      pq.prepare(QStringLiteral(
          "SELECT parent_node_id FROM gui_cost_nodes WHERE node_id = ?"));
      pq.addBindValue(static_cast<qlonglong>(cur));
      if (pq.exec() && pq.next() && !pq.value(0).isNull()) {
        cur = pq.value(0).toLongLong();
      } else {
        break;
      }
    }
  }

  double cost = 0.0;
  for (auto nid : path) {
    // Add this node's direct cost.
    QSqlQuery cq(d->project_db);
    cq.prepare(QStringLiteral(
        "SELECT cost_usd, parent_node_id FROM gui_cost_nodes "
        "WHERE node_id = ?"));
    cq.addBindValue(static_cast<qlonglong>(nid));
    if (!cq.exec() || !cq.next()) continue;

    cost += cq.value(0).toDouble();
    auto parent_id = cq.value(1).isNull() ? -1 : cq.value(1).toLongLong();

    if (parent_id < 0) continue;

    // Find all siblings (children of the same parent, excluding this node).
    QSqlQuery sq(d->project_db);
    sq.prepare(QStringLiteral(
        "SELECT node_id FROM gui_cost_nodes "
        "WHERE parent_node_id = ? AND node_id != ?"));
    sq.addBindValue(static_cast<qlonglong>(parent_id));
    sq.addBindValue(static_cast<qlonglong>(nid));
    if (!sq.exec()) continue;

    QVector<int64_t> sibling_ids;
    while (sq.next()) {
      sibling_ids.append(sq.value(0).toLongLong());
    }

    if (sibling_ids.isEmpty()) continue;

    // Amortize sibling subtree costs: each sibling's subtree cost is
    // shared equally among all children at this level.
    auto num_children = static_cast<double>(sibling_ids.size() + 1);
    for (auto sib_id : sibling_ids) {
      cost += subtree_cost(sib_id) / static_cast<double>(num_children);
    }
  }

  return cost;
}

}  // namespace mx::gui
