/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>

#include <QAbstractItemModel>

namespace mx::gui {

//! A tree model that displays entity information
class IInformationExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Factory method
  static IInformationExplorerModel *
  Create(Index index, FileLocationCache file_location_cache,
         QObject *parent = nullptr);

  //! Constructor
  IInformationExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IInformationExplorerModel() override = default;

  //! Populates the model with the information for the given entity
  virtual void RequestEntityInformation(const RawEntityId &entity_id) = 0;

  //! Disabled copy constructor
  IInformationExplorerModel(const IInformationExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IInformationExplorerModel &
  operator=(const IInformationExplorerModel &) = delete;
};

}  // namespace mx::gui
