/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>
#include <QAbstractItemModel>

#include <multiplier/Types.h>

namespace mx {
class Index;
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

class ICodeModel;

//! A model proxy factory that implements highlighting
class IMacroExplorer : public QWidget {
  Q_OBJECT

 public:
  //! Creates a new instance
  static IMacroExplorer *Create(const Index &index,
                                const FileLocationCache &file_cache,
                                QWidget *parent = nullptr);

  //! Constructor
  IMacroExplorer(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IMacroExplorer() override = default;

  //! Disabled copy constructor
  IMacroExplorer(const IMacroExplorer &) = delete;

  //! Disabled copy assignment operator
  IMacroExplorer &operator=(const IMacroExplorer &) = delete;

  //! Create an `ICodeModel` connected with the `IMacroExplorer`, so that the
  //! `ICodeModel` can notify registered code views to expand macros and
  //! re-render.
  virtual ICodeModel *
  CreateCodeModel(const FileLocationCache &file_location_cache,
                  const Index &index, const bool &remap_related_entity_id_role,
                  QObject *parent = nullptr) = 0;

 public slots:
  virtual void AddMacro(RawEntityId macro_id, RawEntityId token_id) = 0;
};

}  // namespace mx::gui
