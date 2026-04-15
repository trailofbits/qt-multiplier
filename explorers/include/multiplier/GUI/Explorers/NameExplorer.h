// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <memory>

#include <QMap>
#include <QString>

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/Types.h>

namespace mx::gui {

class ConfigManager;

class NameExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

 public:
  virtual ~NameExplorer(void);

  explicit NameExplorer(ConfigManager &config, IWindowManager *parent = nullptr);

  void ActOnContextMenu(IWindowManager *manager, QMenu *menu,
                        const QModelIndex &index) Q_DECL_FINAL;

  // Return the current rename map (for initial sync after signal wiring).
  QMap<mx::RawEntityId, QString> currentRenames(void) const;

 signals:
  void RenameEntities(const QMap<mx::RawEntityId, QString> &renames);

 protected:
  bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

 private slots:
  void OnIndexChanged(const ConfigManager &config_manager);
  void OnDeleteSelected(void);

 private:
  void UpdateItemButtons(void);

  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
