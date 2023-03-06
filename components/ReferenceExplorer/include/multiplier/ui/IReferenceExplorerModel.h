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

    //! Returns the referenced entity id as a RawEntityId value
    ReferencedEntityIdRole,

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
    QString path;

    //! An optional line number
    unsigned line{0};

    //! An optional column number
    unsigned column{0};
  };

  //! Factory method
  static IReferenceExplorerModel *
  Create(mx::Index index, mx::FileLocationCache file_location_cache,
         QObject *parent = nullptr);

  //! Adds a new entity object under the given parent
  virtual bool AppendEntityObject(RawEntityId entity_id,
                                  const QModelIndex &parent) = 0;

  //! Expands the specified entity
  virtual void ExpandEntity(const QModelIndex &index) = 0;

  //! Returns true if an alternative root is being used
  virtual bool HasAlternativeRoot() const = 0;

  //! Sets the given item as the new root
  virtual void SetRoot(const QModelIndex &index) = 0;

  //! Restores the default root item
  virtual void SetDefaultRoot() = 0;

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
