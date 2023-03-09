/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <filesystem>
#include <multiplier/Index.h>

#include <QDataStream>
#include <QAbstractItemModel>
#include <QMetaType>

namespace mx::gui {

//! A model for the reference explorer widget
class IReferenceExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:

  //! Expansion modes.
  enum ExpansionMode {
    //! A node whose current expansion mode has already been activated. This
    //! is used to prevent us from repeatedly expanding the same node.
    AlreadyExpanded,

    //! Expand showing the call hierarchy.
    CallHierarchyMode
  };

  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    LocationRole = Qt::UserRole + 1,

    //! Returns the default expansion mode for this node's children.
    DefaultExpansionMode,

    //! Tells us whether or not this node has been expanded.
    HasBeenExpanded,

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

    friend QDataStream &operator<<(QDataStream &stream, const Location &self);

    // NOTE(pag): May throw.
    friend QDataStream &operator>>(QDataStream &stream, Location &self);
  };

  //! Factory method
  static IReferenceExplorerModel *
  Create(mx::Index index, mx::FileLocationCache file_location_cache,
         QObject *parent = nullptr);

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

 public slots:

  //! Adds a new entity object under the given parent
  virtual void AppendEntityById(
      RawEntityId entity_id, ExpansionMode import_mode,
      const QModelIndex &parent) = 0;

  //! Adds a new entity object under the given parent
  virtual void AppendEntityObject(
      VariantEntity entity, ExpansionMode import_mode,
      const QModelIndex &parent) = 0;

 private:

  //! Disabled copy constructor
  IReferenceExplorerModel(const IReferenceExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IReferenceExplorerModel &operator=(const IReferenceExplorerModel &) = delete;
};

}  // namespace mx::gui

//! Allows mx::gui::IReferenceExplorerModel::Location values to fit inside QVariant objects
Q_DECLARE_METATYPE(mx::gui::IReferenceExplorerModel::Location);
Q_DECLARE_METATYPE(mx::gui::IReferenceExplorerModel::ExpansionMode);
