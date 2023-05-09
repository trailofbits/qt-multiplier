/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <QIdentityProxyModel>

namespace mx::gui {

//! A model proxy used to signal views which tokens to highlight
class HighlightingModelProxy final : public QIdentityProxyModel {
  Q_OBJECT

 public:
  HighlightingModelProxy(QAbstractItemModel *source_model,
                         int entity_id_data_role);

  virtual ~HighlightingModelProxy() override;

  QVariant data(const QModelIndex &index, int role) const override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public slots:
  //! Triggers a model reset without a source model reindex
  void
  OnEntityHighlightListChange(const EntityHighlightList &entity_highlight_list);
};

}  // namespace mx::gui
