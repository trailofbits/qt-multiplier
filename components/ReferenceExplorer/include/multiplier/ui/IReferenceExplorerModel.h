/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <filesystem>
#include <multiplier/Index.h>

#include <QAbstractItemModel>

namespace mx::gui {

//! A model for the reference explorer widget
class IReferenceExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    LocationRole = Qt::UserRole + 1,

    //! Returns the internal node identifier
    InternalIdentifierRole,

    //! Returns the entity id as a RawEntityId value
    EntityIdRole,

    //! Returns the fragment id as a RawEntityId value
    FragmentIdRole,

    //! Returns the file id as a RawEntityId value
    FileIdRole,

    //! Returns the line number id as an unsigned value
    LineNumberRole,

    //! Returns the column number id as an unsigned value
    ColumnNumberRole,
  };

  //! Location information, containing path + line and column
  struct Location final {
    RawEntityId file_id{kInvalidEntityId};

    //! File path
    std::filesystem::path path;

    //! An optional line number
    unsigned line{0};

    //! An optional column number
    unsigned column{0};
  };

  //! Factory method
  static IReferenceExplorerModel *
  Create(mx::Index index, mx::FileLocationCache file_location_cache,
         QObject *parent = nullptr);

  //! Entity object type, see IReferenceExplorerModel::AppendEntityObject
  enum class EntityObjectType {
    CallHierarchy,
  };

  //! Adds a new entity object under the given parent
  virtual bool
  AppendEntityObject(RawEntityId entity_id, EntityObjectType type,
                     const QModelIndex &parent,
                     std::optional<std::size_t> opt_ttl = std::nullopt) = 0;

  //! Constructor
  IReferenceExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IReferenceExplorerModel() override = default;

  //! Disabled copy constructor
  IReferenceExplorerModel(const IReferenceExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IReferenceExplorerModel &operator=(const IReferenceExplorerModel &) = delete;
};

}  // namespace mx::gui

//! Allows mx::gui::IReferenceExplorerModel::Location values to fit inside QVariant objects
Q_DECLARE_METATYPE(mx::gui::IReferenceExplorerModel::Location);
