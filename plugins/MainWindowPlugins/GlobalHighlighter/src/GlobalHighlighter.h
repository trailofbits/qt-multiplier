/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "Types.h"

#include <multiplier/ui/IGlobalHighlighter.h>
#include <multiplier/ui/IThemeManager.h>

namespace mx::gui {

class GlobalHighlighter final : public IGlobalHighlighter {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~GlobalHighlighter() override;

  //! \copybrief IGlobalHighlighter::CreateModelProxy
  virtual QAbstractProxyModel *
  CreateModelProxy(QAbstractItemModel *source_model,
                   int entity_id_data_role) override;

 public slots:
  //! \copybrief IGlobalHighlighter::AddEntity
  virtual void SetEntityColor(const RawEntityId &entity_id,
                              const QColor &color) override;

  //! \copybrief IGlobalHighlighter::RemoveEntity
  virtual void RemoveEntity(const RawEntityId &entity_id) override;

  //! \copybrief IGlobalHighlighter::Clear
  virtual void Clear() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  GlobalHighlighter(const Index &index,
                    const FileLocationCache &file_location_cache,
                    QWidget *parent);

  //! Starts a new Related Entities requests
  void StartRequest(const RawEntityId &entity_id);

  //! Cancels any active request
  void CancelRequest();

  //! Updates the on-screen item list
  void UpdateItemList();

 private slots:
  //! Called when the entity name resolution has finished
  void EntityListFutureStatusChanged();

  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

 signals:
  //! Signals the proxy models that the highlight set has changed
  void EntityColorMapChanged(const EntityColorMap &entity_highlight_list);

  friend class IGlobalHighlighter;
};

}  // namespace mx::gui
