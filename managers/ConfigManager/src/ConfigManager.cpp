// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Managers/ConfigManager.h>

#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/Token.h>

#include <QActionGroup>
#include <QDir>
#include <QMenu>

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
      "  label TEXT)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_header_states ("
      "  key TEXT PRIMARY KEY, state BLOB)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_expanded_macros ("
      "  entity_id INTEGER PRIMARY KEY)"));
  q.exec(QStringLiteral(
      "CREATE TABLE IF NOT EXISTS gui_highlight_colors ("
      "  entity_id INTEGER PRIMARY KEY, fg TEXT, bg TEXT)"));

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
  if (auto theme = d->theme_manager.Theme()) {
    std::cerr << "~ConfigManager: saving theme_id='"
              << theme->Id().toStdString() << "'" << std::endl;
  }
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
  auto tab_menu = new QMenu(tr("Tab Size"), menu);
  menu->addMenu(tab_menu);

  auto tab_group = new QActionGroup(tab_menu);
  tab_group->setExclusive(true);
  for (unsigned i = 1; i <= 16; ++i) {
    auto action = new QAction(QString::number(i), tab_group);
    action->setCheckable(true);
    action->setChecked(i == d->tab_width);
    connect(action, &QAction::triggered, this, [this, i] () { SetTabWidth(i); });
    tab_menu->addAction(action);
  }
  connect(this, &ConfigManager::TabWidthChanged,
          tab_menu, [tab_group] (unsigned w) {
            for (auto *a : tab_group->actions())
              a->setChecked(a->text().toUInt() == w);
          });

  auto tab_stops_action = new QAction(tr("Use Tab Stops"), menu);
  tab_stops_action->setCheckable(true);
  tab_stops_action->setChecked(d->use_tab_stops);
  menu->addAction(tab_stops_action);
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
    std::cerr << "SaveSettings: writing theme_id='"
              << theme->Id().toStdString() << "'" << std::endl;
    SetSetting(d->global_db, QStringLiteral("theme_id"), theme->Id());
  }
  if (!d->global_db.commit()) {
    std::cerr << "SaveSettings: COMMIT FAILED: "
              << d->global_db.lastError().text().toStdString() << std::endl;
  }

  // Verify the write.
  auto verify = GetSetting(d->global_db, QStringLiteral("theme_id"));
  std::cerr << "SaveSettings: verify readback='" << verify.toStdString()
            << "'" << std::endl;
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
  std::cerr << "LoadSettings: theme_id='" << theme_id.toStdString() << "'"
            << std::endl;
  if (!theme_id.isEmpty()) {
    if (auto theme = d->theme_manager.Find(theme_id)) {
      std::cerr << "LoadSettings: found theme, setting it" << std::endl;
      d->theme_manager.SetTheme(theme);
    } else {
      std::cerr << "LoadSettings: theme NOT found!" << std::endl;
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
    if (val >= 1 && val <= 16) {
      d->tab_width = val;
      d->RebuildFileLocationCache();
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
  if (!d->global_db.isOpen()) return;
  SetSetting(d->global_db, QStringLiteral("window_state"),
             QString::fromLatin1(state.toBase64()));
  SetSetting(d->global_db, QStringLiteral("window_geometry"),
             QString::fromLatin1(geometry.toBase64()));
}

bool ConfigManager::LoadWindowLayout(QByteArray &state,
                                     QByteArray &geometry) const {
  if (!d->global_db.isOpen()) return false;
  auto s = GetSetting(d->global_db, QStringLiteral("window_state"));
  if (s.isEmpty()) return false;
  state = QByteArray::fromBase64(s.toLatin1());
  geometry = QByteArray::fromBase64(
      GetSetting(d->global_db, QStringLiteral("window_geometry")).toLatin1());
  return true;
}

// --- Expanded macros ---

void ConfigManager::SaveExpandedMacros(const QSet<RawEntityId> &macros) const {
  if (!d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("DELETE FROM gui_expanded_macros"));
  q.prepare(QStringLiteral(
      "INSERT INTO gui_expanded_macros (entity_id) VALUES (?)"));
  for (auto id : macros) {
    q.addBindValue(qulonglong(id));
    q.exec();
  }
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

// --- Highlight colors ---

void ConfigManager::SaveHighlightColors(const HighlightColorMap &colors) const {
  if (!d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.exec(QStringLiteral("DELETE FROM gui_highlight_colors"));
  q.prepare(QStringLiteral(
      "INSERT INTO gui_highlight_colors (entity_id, fg, bg) VALUES (?, ?, ?)"));
  for (const auto &[id, fg_bg] : colors) {
    q.addBindValue(qulonglong(id));
    q.addBindValue(fg_bg.first.name(QColor::HexArgb));
    q.addBindValue(fg_bg.second.name(QColor::HexArgb));
    q.exec();
  }
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

// --- Navigation history ---

void ConfigManager::SaveNavigationHistory(
    const std::vector<NavigationEntry> &entries, const QString &key) const {
  if (!d || !d->project_db.isValid() || !d->project_db.isOpen()) return;
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral("DELETE FROM gui_history WHERE widget_key = ?"));
  q.addBindValue(key);
  q.exec();

  q.prepare(QStringLiteral(
      "INSERT INTO gui_history (widget_key, entity_id, label) VALUES (?, ?, ?)"));
  for (const auto &e : entries) {
    q.addBindValue(key);
    q.addBindValue(qulonglong(e.entity_id));
    q.addBindValue(e.label);
    q.exec();
  }
}

std::vector<ConfigManager::NavigationEntry>
ConfigManager::LoadNavigationHistory(const QString &key) const {
  if (!d || !d->project_db.isValid() || !d->project_db.isOpen()) return {};
  QSqlQuery q(d->project_db);
  q.prepare(QStringLiteral(
      "SELECT entity_id, label FROM gui_history "
      "WHERE widget_key = ? ORDER BY id ASC"));
  q.addBindValue(key);
  std::vector<NavigationEntry> result;
  if (q.exec()) {
    while (q.next()) {
      NavigationEntry e;
      e.entity_id = static_cast<RawEntityId>(q.value(0).toULongLong());
      e.label = q.value(1).toString();
      result.push_back(std::move(e));
    }
  }
  return result;
}

}  // namespace mx::gui
