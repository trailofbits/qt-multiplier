// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QVector>

#include <multiplier/GUI/Interfaces/IMainWindowPlugin.h>
#include <multiplier/Types.h>

namespace mx {
enum class TokenCategory : unsigned char;
}  // namespace mx
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

  std::optional<NamedAction> ActOnSecondaryClick(
      IWindowManager *manager, const QModelIndex &index) Q_DECL_FINAL;

  std::optional<NamedAction> ActOnKeyPress(
    IWindowManager *, const QKeySequence &keys,
    const QModelIndex &index) Q_DECL_FINAL;

 private slots:
  void OnOpenEntity(const QVariant &data);
  void OnPreviewEntity(const QVariant &data);
  void OnPinnedPreviewEntity(const QVariant &data);

  void OnExpandMacro(RawEntityId entity_id);

  void OnRenameEntity(QVector<RawEntityId> entity_ids,
                      QString new_name);
};

}  // namespace mx::gui
