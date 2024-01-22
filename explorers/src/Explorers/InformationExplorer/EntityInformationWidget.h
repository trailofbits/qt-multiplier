/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include <multiplier/GUI/Interfaces/IInformationExplorerPlugin.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/Index.h>
#include <vector>

#include "EntityInformationRunnable.h"

QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE

namespace mx {
class FileLocationCache;
}  // namespace mx

namespace mx::gui {

class ConfigManager;
class EntityInformationModel;
class HistoryWidget;
class MediaManager;

//! A component that wraps an InformationExplorer widget with its model
class EntityInformationWidget Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Constructor
  EntityInformationWidget(const ConfigManager &config_manager,
                          bool enable_history, QWidget *parent = nullptr);

  //! Destructor
  virtual ~EntityInformationWidget(void);

  //! Requests the internal model to display the specified entity
  void DisplayEntity(
      VariantEntity entity, const FileLocationCache &file_location_cache,
      const std::vector<IInformationExplorerPluginPtr> &plugins,
      bool is_explicit_request, bool add_to_history);

 protected:
  //! Used to implement click support without using the selection model
  virtual bool eventFilter(QObject *object, QEvent *event);

 private slots:
  void OnAllDataFound(void);
  void OnCancelRunningRequest(void);
  void OnChangeSync(int state);
  void OnSearchParametersChange(void);
  void ExpandAllBelow(const QModelIndex &parent);
  void OnCurrentItemChanged(const QModelIndex &current_index);
  void OnOpenItemContextMenu(const QPoint &tree_local_mouse_pos);
  void OnIconsChanged(const MediaManager &media_manager);
  void OnPopOutPressed(void);

 signals:
  void HistoricalEntitySelected(VariantEntity entity);
  void SelectedItemChanged(const QModelIndex &index);

  // TODO(pag): IndexChanged should close the widget if it is a pinned info
  //            explorer.

  // TODO(pag): Change the window title based on `entity`.
};

}  // namespace mx::gui
