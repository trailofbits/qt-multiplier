/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Index.h>

#include <QAbstractItemModel>
#include <QString>

namespace mx::gui {

//! A model for the reference explorer widget
class IReferenceExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    LocationRole = Qt::UserRole + 1,

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

    //! Returns the token category
    TokenCategoryRole,
  };

  //! Reference type
  enum class ReferenceType {
    Callers,
  };

  //! Factory method
  static IReferenceExplorerModel *
  Create(const Index &index, const FileLocationCache &file_location_cache,
         QObject *parent = nullptr);

  //! Resets the model, setting the given entity as root
  virtual void SetEntity(const RawEntityId &entity_id,
                         const ReferenceType &reference_type) = 0;

  //! Constructor
  IReferenceExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IReferenceExplorerModel() override = default;

 public slots:
  //! Cancels any running request
  virtual void CancelRunningRequest() = 0;

 private:
  //! Disabled copy constructor
  IReferenceExplorerModel(const IReferenceExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IReferenceExplorerModel &operator=(const IReferenceExplorerModel &) = delete;

 signals:
  //! Emitted when a new request is started
  void RequestStarted();

  //! Emitted when a request has finished
  void RequestFinished();
};

}  // namespace mx::gui
