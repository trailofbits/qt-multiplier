/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Widgets/CodeWidget.h>
#include <multiplier/Index.h>

namespace mx::gui {

class ConfigManager;
class HistoryWidget;
class MediaManager;

//! A component that wraps an InformationExplorer widget with its model
class CodePreviewWidget Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  static const QString kModelId;

  //! Constructor
  CodePreviewWidget(const ConfigManager &config_manager,
                    const CodeWidget::SceneOptions &scene_options,
                    bool enable_history, QWidget *parent = nullptr);

  //! Destructor
  virtual ~CodePreviewWidget(void);

  //! Requests the internal model to display the specified entity
  void DisplayEntity(VariantEntity entity,
                     bool is_explicit_request, bool add_to_history);

 private slots:
  void OnChangeSync(int state);
  void OnIconsChanged(const MediaManager &media_manager);
  void OnPopOutPressed(void);
 
 public slots:
  // Invoked when the set of macros to be expanded changes.
  void OnExpandMacros(const QSet<RawEntityId> &macros_to_expand);

  // Invoked when the set of entities to be renamed changes.
  void OnRenameEntities(const QMap<RawEntityId, QString> &new_entity_names);

  // Invoked when we want to scroll to a specific entity.
  void OnGoToEntity(const VariantEntity &entity, bool take_focus);

 signals:
  void HistoricalEntitySelected(VariantEntity entity);
  void SelectedItemChanged(const QModelIndex &index);
};

}  // namespace mx::gui
