// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Index.h>

#include <QWidget>

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

 protected:
  virtual void leaveEvent(QEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void InitializeWidgets(mx::Index index,
                         mx::FileLocationCache file_location_cache,
                         RawEntityId entity_id);
};

}  // namespace mx::gui
