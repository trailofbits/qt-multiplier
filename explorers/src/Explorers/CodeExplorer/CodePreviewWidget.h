/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include <multiplier/GUI/Interfaces/IWindowWidget.h>
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
  static const QString kModelPrefix;

  //! Constructor
  CodePreviewWidget(const ConfigManager &config_manager,
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

 signals:
  void HistoricalEntitySelected(VariantEntity entity);
  void SelectedItemChanged(const QModelIndex &index);
};

}  // namespace mx::gui
