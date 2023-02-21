// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Index.h>

#include <QWidget>
#include <QMimeData>

namespace mx::gui {

class QuickReferenceExplorer final : public QWidget {
  Q_OBJECT

 public:
  QuickReferenceExplorer(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         RawEntityId entity_id, QWidget *parent = nullptr);

  virtual ~QuickReferenceExplorer() override;

  QuickReferenceExplorer(const QuickReferenceExplorer &) = delete;
  QuickReferenceExplorer &operator=(const QuickReferenceExplorer &) = delete;

 signals:
  void SaveAll(QMimeData *mime_data, const bool &as_new_tab);

 protected:
  virtual void keyPressEvent(QKeyEvent *event) override;
  virtual void resizeEvent(QResizeEvent *event) override;
  virtual void showEvent(QShowEvent *event) override;
  virtual void closeEvent(QCloseEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void InitializeWidgets(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         RawEntityId entity_id);

  void UpdateButtonPositions();
  void EmitSaveSignal(const bool &as_new_tab);

 private slots:
  void OnApplicationStateChange(Qt::ApplicationState state);
  void OnSaveAllToActiveRefExplorerButtonPress();
  void OnSaveAllToNewRefExplorerButtonPress();
};

}  // namespace mx::gui
