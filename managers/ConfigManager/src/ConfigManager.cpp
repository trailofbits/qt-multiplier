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
#include <QFileInfo>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <multiplier/Index.h>

#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

#include "ThemedItemDelegate.h"

namespace mx::gui {
namespace {

static constexpr unsigned kDefaultTabWidth = 4u;

// Keys for the global settings file.
static const QString kSettingsGroup = QStringLiteral("General");
static const QString kTabWidthKey = QStringLiteral("tab_width");
static const QString kThemeIdKey = QStringLiteral("theme_id");
static const QString kFontSizeDeltaKey = QStringLiteral("font_size_delta");
static const QString kUseTabStopsKey = QStringLiteral("use_tab_stops");

// Return the path to the global settings file: $HOME/settings.qmx
static QString GlobalSettingsPath(void) {
  return QDir::homePath() + QStringLiteral("/.multiplier.qmx");
}

// Return the path to the project settings file for a given database:
// /path/to/foo.db -> /path/to/foo.qmx
static QString ProjectSettingsPath(const QString &db_path) {
  if (db_path.isEmpty()) {
    return {};
  }
  QFileInfo fi(db_path);
  return fi.absolutePath() + QStringLiteral("/") +
         fi.completeBaseName() + QStringLiteral(".qmx");
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

  // Path to the currently open database, used to derive project settings path.
  QString db_path;

  void RebuildFileLocationCache(void) {
    FileLocationConfiguration config;
    config.tab_width = tab_width;
    config.use_tab_stops = use_tab_stops;
    file_location_cache.clear();
    file_location_cache = FileLocationCache(config);
  }

  inline ConfigManagerImpl(QApplication &application, QObject *self)
      : theme_manager(application, self),
        media_manager(theme_manager, self) {}
};

ConfigManager::~ConfigManager(void) {
  SaveSettings();
}

ConfigManager::ConfigManager(QApplication &application, QObject *parent)
    : QObject(parent),
      d(std::make_shared<ConfigManagerImpl>(application, this)) {

  // Auto-save when the theme changes.
  using TM = class ThemeManager;
  connect(&(d->theme_manager), &TM::ThemeChanged,
          this, [this] (const TM &) { SaveSettings(); });
}

class ActionManager &ConfigManager::ActionManager(void) const noexcept {
  return d->action_manager;
}

// Get access to the global theme manager.
class ThemeManager &ConfigManager::ThemeManager(void) const noexcept {
  return d->theme_manager;
}

// Get access to the global media manager.
class MediaManager &ConfigManager::MediaManager(void) const noexcept {
  return d->media_manager;
}

//! Get access to the current index.
const class Index &ConfigManager::Index(void) const noexcept {
  return d->index;
}

//! Change the current index.
void ConfigManager::SetIndex(const class Index &index,
                             const QString &db_path) noexcept {
  // Save project settings for the previous database before switching.
  if (!d->db_path.isEmpty()) {
    SaveProjectSettings();
  }

  d->file_location_cache.clear();
  d->index = index;
  d->db_path = db_path;

  // Load project settings for the new database.
  if (!d->db_path.isEmpty()) {
    LoadProjectSettings();
  }

  emit IndexChanged(*this);
}

// Return the shared file location cache.
const class FileLocationCache &
ConfigManager::FileLocationCache(void) const noexcept {
  return d->file_location_cache;
}

//! Set an item delegate on `view` that pays attention to the theme. This
//! allows items using `IModel` to present tokens.
void ConfigManager::InstallItemDelegate(
    QAbstractItemView *view, const ItemDelegateConfig &config) const {

  auto set_delegate = [=, this] (const class ThemeManager &self) {
    QAbstractItemDelegate *old_delegate = view->itemDelegate();

    auto theme = self.Theme();
    view->setFont(theme->Font());

    auto new_delegate = new ThemedItemDelegate(
        std::move(theme), config.whitespace_replacement,
        d->tab_width, view);
    view->setItemDelegate(new_delegate);

    if (old_delegate) {
      old_delegate->deleteLater();
    }
  };

  set_delegate(d->theme_manager);
  using TM2 = class ThemeManager;
  connect(&(d->theme_manager), &TM2::ThemeChanged,
          view, std::move(set_delegate));
}

void ConfigManager::PopulateViewMenu(QMenu *menu) {
  d->theme_manager.PopulateViewMenu(menu);

  // --- Tab size submenu ---
  menu->addSeparator();
  auto tab_menu = new QMenu(tr("Tab Size"), menu);
  menu->addMenu(tab_menu);

  auto tab_group = new QActionGroup(tab_menu);
  tab_group->setExclusive(true);

  for (unsigned i = 1; i <= 16; ++i) {
    auto action = new QAction(QString::number(i), tab_group);
    action->setCheckable(true);
    action->setChecked(i == d->tab_width);
    connect(action, &QAction::triggered, this, [this, i] () {
      SetTabWidth(i);
    });
    tab_menu->addAction(action);
  }

  // Update check marks when tab width changes.
  connect(this, &ConfigManager::TabWidthChanged,
          tab_menu, [tab_group] (unsigned new_width) {
            for (auto *action : tab_group->actions()) {
              action->setChecked(
                  action->text().toUInt() == new_width);
            }
          });

  // --- Tab stops checkbox ---
  auto tab_stops_action = new QAction(tr("Use Tab Stops"), menu);
  tab_stops_action->setCheckable(true);
  tab_stops_action->setChecked(d->use_tab_stops);
  menu->addAction(tab_stops_action);
  connect(tab_stops_action, &QAction::toggled,
          this, &ConfigManager::SetUseTabStops);
  connect(this, &ConfigManager::UseTabStopsChanged,
          tab_stops_action, &QAction::setChecked);
}

unsigned ConfigManager::TabWidth(void) const noexcept {
  return d->tab_width;
}

void ConfigManager::SetTabWidth(unsigned width) {
  if (width < 1) width = 1;
  if (width > 16) width = 16;
  if (d->tab_width == width) return;

  d->tab_width = width;
  d->RebuildFileLocationCache();
  SaveProjectSettings();
  emit TabWidthChanged(width);
}

bool ConfigManager::UseTabStops(void) const noexcept {
  return d->use_tab_stops;
}

void ConfigManager::SetUseTabStops(bool use) {
  if (d->use_tab_stops == use) return;

  d->use_tab_stops = use;
  d->RebuildFileLocationCache();
  SaveSettings();
  emit UseTabStopsChanged(use);
}

// --- Global settings ($HOME/settings.qmx) ---

void ConfigManager::SaveSettings(void) const {
  QSettings settings(GlobalSettingsPath(), QSettings::IniFormat);
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kFontSizeDeltaKey, d->theme_manager.FontSizeDelta());
  settings.setValue(kUseTabStopsKey, d->use_tab_stops);

  if (auto theme = d->theme_manager.Theme()) {
    settings.setValue(kThemeIdKey, theme->Id());
  }

  settings.endGroup();
}

void ConfigManager::LoadSettings(void) {
  QSettings settings(GlobalSettingsPath(), QSettings::IniFormat);
  settings.beginGroup(kSettingsGroup);

  // Use tab stops.
  if (settings.contains(kUseTabStopsKey)) {
    d->use_tab_stops = settings.value(kUseTabStopsKey, true).toBool();
  }

  // Rebuild file location cache with loaded tab settings.
  d->RebuildFileLocationCache();

  // Font size delta.
  if (settings.contains(kFontSizeDeltaKey)) {
    int delta = settings.value(kFontSizeDeltaKey, 0).toInt();
    if (delta != 0) {
      d->theme_manager.SetFontSizeDelta(delta);
    }
  }

  // Theme.
  if (settings.contains(kThemeIdKey)) {
    QString theme_id = settings.value(kThemeIdKey).toString();
    if (auto theme = d->theme_manager.Find(theme_id)) {
      d->theme_manager.SetTheme(theme);
    }
  }

  settings.endGroup();
}

// --- Per-database project settings (/path/to/foo.qmx) ---

void ConfigManager::SaveProjectSettings(void) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return;

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(kSettingsGroup);
  settings.setValue(kTabWidthKey, d->tab_width);
  settings.endGroup();
}

void ConfigManager::LoadProjectSettings(void) {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return;

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(kSettingsGroup);

  if (settings.contains(kTabWidthKey)) {
    unsigned tw = settings.value(kTabWidthKey, kDefaultTabWidth).toUInt();
    if (tw >= 1 && tw <= 16) {
      d->tab_width = tw;
      d->RebuildFileLocationCache();
      // Don't emit TabWidthChanged here — LoadSettings handles menu updates.
    }
  }

  settings.endGroup();
}

// --- Header state persistence ---

void ConfigManager::SaveHeaderState(const QString &id,
                                    const QByteArray &state) const {
  // Save to project settings if we have a database, otherwise global.
  QString path = d->db_path.isEmpty() ? GlobalSettingsPath()
                                      : ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return;

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("HeaderStates"));
  settings.setValue(id, state);
  settings.endGroup();
}

QByteArray ConfigManager::LoadHeaderState(const QString &id) const {
  QString path = d->db_path.isEmpty() ? GlobalSettingsPath()
                                      : ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return {};

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("HeaderStates"));
  auto val = settings.value(id).toByteArray();
  settings.endGroup();
  return val;
}

// --- Window layout persistence ---

void ConfigManager::SaveWindowLayout(const QByteArray &state,
                                     const QByteArray &geometry) const {
  QSettings settings(GlobalSettingsPath(), QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("WindowLayout"));
  settings.setValue(QStringLiteral("state"), state);
  settings.setValue(QStringLiteral("geometry"), geometry);
  settings.endGroup();
}

bool ConfigManager::LoadWindowLayout(QByteArray &state,
                                     QByteArray &geometry) const {
  QSettings settings(GlobalSettingsPath(), QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("WindowLayout"));
  if (!settings.contains(QStringLiteral("state"))) {
    settings.endGroup();
    return false;
  }
  state = settings.value(QStringLiteral("state")).toByteArray();
  geometry = settings.value(QStringLiteral("geometry")).toByteArray();
  settings.endGroup();
  return true;
}

// --- Expanded macros persistence (per-project) ---

void ConfigManager::SaveExpandedMacros(const QSet<RawEntityId> &macros) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return;

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("ExpandedMacros"));
  settings.remove(QString());  // Clear group.

  QStringList ids;
  for (auto id : macros) {
    ids.push_back(QString::number(id));
  }
  settings.setValue(QStringLiteral("ids"), ids);
  settings.endGroup();
}

QSet<RawEntityId> ConfigManager::LoadExpandedMacros(void) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return {};

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("ExpandedMacros"));
  QStringList ids = settings.value(QStringLiteral("ids")).toStringList();
  settings.endGroup();

  QSet<RawEntityId> result;
  for (const auto &id : ids) {
    bool ok = false;
    auto val = id.toULongLong(&ok);
    if (ok) {
      result.insert(val);
    }
  }
  return result;
}

void ConfigManager::SaveHighlightColors(
    const HighlightColorMap &colors) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return;

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("HighlightColors"));
  settings.remove(QString());  // Clear group.

  int i = 0;
  for (const auto &[id, fg_bg] : colors) {
    settings.beginGroup(QString::number(i++));
    settings.setValue(QStringLiteral("id"), qulonglong(id));
    settings.setValue(QStringLiteral("fg"), fg_bg.first.name(QColor::HexArgb));
    settings.setValue(QStringLiteral("bg"), fg_bg.second.name(QColor::HexArgb));
    settings.endGroup();
  }

  settings.endGroup();
}

ConfigManager::HighlightColorMap ConfigManager::LoadHighlightColors(void) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return {};

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("HighlightColors"));

  HighlightColorMap result;
  for (const auto &group : settings.childGroups()) {
    settings.beginGroup(group);
    bool ok = false;
    auto id = static_cast<RawEntityId>(
        settings.value(QStringLiteral("id")).toULongLong(&ok));
    if (ok) {
      QColor fg(settings.value(QStringLiteral("fg")).toString());
      QColor bg(settings.value(QStringLiteral("bg")).toString());
      if (fg.isValid() && bg.isValid()) {
        result.emplace(id, std::make_pair(fg, bg));
      }
    }
    settings.endGroup();
  }

  settings.endGroup();
  return result;
}

void ConfigManager::SaveNavigationHistory(
    const std::vector<NavigationEntry> &entries,
    const QString &key) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return;

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("NavigationHistory_") + key);
  settings.remove(QString());  // Clear group.

  int i = 0;
  for (const auto &entry : entries) {
    settings.beginGroup(QString::number(i++));
    settings.setValue(QStringLiteral("id"), qulonglong(entry.entity_id));
    settings.setValue(QStringLiteral("line"), entry.line);
    settings.setValue(QStringLiteral("col"), entry.column);
    settings.setValue(QStringLiteral("label"), entry.label);
    settings.endGroup();
  }

  settings.endGroup();
}

std::vector<ConfigManager::NavigationEntry>
ConfigManager::LoadNavigationHistory(const QString &key) const {
  QString path = ProjectSettingsPath(d->db_path);
  if (path.isEmpty()) return {};

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("NavigationHistory_") + key);

  std::vector<NavigationEntry> result;
  auto groups = settings.childGroups();

  // Sort numerically to preserve order.
  std::sort(groups.begin(), groups.end(),
            [] (const QString &a, const QString &b) {
              return a.toInt() < b.toInt();
            });

  for (const auto &group : groups) {
    settings.beginGroup(group);
    NavigationEntry entry;
    entry.entity_id = static_cast<RawEntityId>(
        settings.value(QStringLiteral("id")).toULongLong());
    entry.line = settings.value(QStringLiteral("line")).toUInt();
    entry.column = settings.value(QStringLiteral("col")).toUInt();
    entry.label = settings.value(QStringLiteral("label")).toString();
    result.push_back(std::move(entry));
    settings.endGroup();
  }

  settings.endGroup();
  return result;
}

}  // namespace mx::gui
