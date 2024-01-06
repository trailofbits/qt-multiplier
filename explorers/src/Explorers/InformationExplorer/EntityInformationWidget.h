/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include "EntityInformationRunnable.h"

QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE

namespace mx::gui {

class ConfigManager;
class EntityInformationModel;
class HistoryWidget;

//! A component that wraps an InformationExplorer widget with its model
class EntityInformationWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Constructor
  EntityInformationWidget(const ConfigManager &config_manager,
                          bool enable_history, QWidget *parent = nullptr);

  //! Destructor
  virtual ~EntityInformationWidget(void);

 public slots:
  //! Requests the internal model to display the specified entity
  void DisplayEntity(const QVariant &entity);

 signals:
  //! Forwards the internal InformationExplorer::SelectedItemChanged signal
  void SelectedItemChanged(const QModelIndex &current_index);

  // TODO(pag): IndexChanged should close the widget if it is a pinned info
  //            explorer.

  // TODO(pag): `DisplayEntity` should take the list of plugins to run.

  // TODO(pag): Change the window title based on `entity`.
};

}  // namespace mx::gui
