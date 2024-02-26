// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>

QT_BEGIN_NAMESPACE
class QEvent;
class QFocusEvent;
class QMouseEvent;
class QResizeEvent;
class QTableView;
QT_END_NAMESPACE

namespace mx::gui {

class ConfigManager;
class ExpandedMacrosModel;
class MediaManager;

class MacroExplorer Q_DECL_FINAL : public IWindowWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~MacroExplorer(void);

  MacroExplorer(const ConfigManager &config_manager,
                ExpandedMacrosModel *model,
                QWidget *parent = nullptr);
 
 private:
  void UpdateItemButtons(void);
  void OnContextMenu(const QPoint &pos, const QModelIndex &index);

 protected:
  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_FINAL;
  void resizeEvent(QResizeEvent *) Q_DECL_FINAL;
  void focusOutEvent(QFocusEvent *) Q_DECL_FINAL;

 private slots:
  void OnIconsChanged(const MediaManager &media_manager);
  void OnOpenButtonPressed(void);
  void OnCloseButtonPressed(void);
};

}  // namespace mx::gui
