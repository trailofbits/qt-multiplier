/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <multiplier/ui/IGlobalHighlighter.h>

namespace mx::gui {

class GlobalHighlighter final : public IGlobalHighlighter {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~GlobalHighlighter() override;

  //! \copybrief IGlobalHighlighter::CreateModelProxy
  virtual QAbstractItemModel *
  CreateModelProxy(QAbstractItemModel *source_model,
                   int entity_id_data_role) override;

  //! \copybrief IGlobalHighlighter::AddEntity
  virtual void SetEntityColor(RawEntityId entity_id,
                              const QColor &color) override;

  //! \copybrief IGlobalHighlighter::RemoveEntity
  virtual void RemoveEntity(RawEntityId entity_id) override;

  //! \copybrief IGlobalHighlighter::Clear
  virtual void Clear() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  GlobalHighlighter(const Index &index,
                    const FileLocationCache &file_location_cache,
                    QWidget *parent);

  void StartRequest(const RawEntityId &entity_id);
  void CancelRequest();

 private slots:
  //! Called when the entity name resolution has finished
  void EntityListFutureStatusChanged();

 signals:
  //! Signals the proxy models that the highlight set has changed
  void
  EntityHighlightListChanged(const EntityHighlightList &entity_highlight_list);

  friend class IGlobalHighlighter;
};

}  // namespace mx::gui
