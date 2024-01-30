// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QSet>
#include <QVector>

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/Entity.h>

namespace mx::gui {

class CodeExplorer Q_DECL_FINAL : public IMainWindowPlugin {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CodeExplorer(void);

  CodeExplorer(ConfigManager &config_manager,
               IWindowManager *parent = nullptr);

  void ActOnPrimaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  void ActOnContextMenu(
      IWindowManager *manager, QMenu *menu,
      const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnKeyPress(
    IWindowManager *, const QKeySequence &keys,
    const QModelIndex &index) Q_DECL_FINAL;

 private:
  void OpenEntity(const VariantEntity &entity, bool add_to_history);

 private slots:
  void OnImplicitPreviewEntity(const QVariant &data);
  void OnExplicitPreviewEntity(const QVariant &data);
  void OnOpenEntity(const QVariant &data);
  void OnPreviewEntity(const QVariant &data, bool is_explicit);
  void OnPinnedPreviewEntity(const QVariant &data);

  void OnExpandMacro(const QVariant &data);

  void OnRenameEntity(QVector<RawEntityId> entity_ids,
                      QString new_name);
  void OnGoToHistoricalItem(const QVariant &data);
  void OnHistoricalPreviewedEntitySelected(const QVariant &data);
  void OnToggleBrowseMode(const QVariant &data);

 signals:

  // Invoked when the set of macros to be expanded changes.
  void ExpandMacros(const QSet<RawEntityId> &macros_to_expand);
};

}  // namespace mx::gui
