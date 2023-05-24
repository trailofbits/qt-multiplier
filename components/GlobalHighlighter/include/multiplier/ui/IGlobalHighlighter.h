/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>

#include <QWidget>
#include <QAbstractItemModel>

namespace mx::gui {

//! A model proxy factory that implements highlighting
class IGlobalHighlighter : public QWidget {
  Q_OBJECT

 public:
  //! Creates a new instance
  static IGlobalHighlighter *Create(const Index &index,
                                    const FileLocationCache &file_cache,
                                    QWidget *parent = nullptr);

  //! Creates a new proxy model controlled by the highlighter
  virtual QAbstractItemModel *CreateModelProxy(QAbstractItemModel *source_model,
                                               int entity_id_data_role) = 0;

  //! Constructor
  IGlobalHighlighter(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IGlobalHighlighter() override = default;

  //! Disabled copy constructor
  IGlobalHighlighter(const IGlobalHighlighter &) = delete;

  //! Disabled copy assignment operator
  IGlobalHighlighter &operator=(const IGlobalHighlighter &) = delete;

 public slots:
  //! Adds (or updates) the specified highlight
  virtual void SetEntityColor(const RawEntityId &entity_id,
                              const QColor &color) = 0;

  //! Removes the given entity to the highlight list
  virtual void RemoveEntity(const RawEntityId &entity_id) = 0;

  //! Clears the highlight list
  virtual void Clear() = 0;
};

}  // namespace mx::gui
