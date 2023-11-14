/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Index.h>

#include <QAbstractItemModel>

#include "ITreeGenerator.h"

namespace mx::gui {

//! A model for the reference explorer widget
class ITreeExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    EntityIdRole = Qt::UserRole + 1,

    //! Returns the token range associated with a given `QModelIndex`. This is
    //! for styled displaying.
    TokenRangeRole,

    //! Returns whether or not this row has been expanded.
    CanBeExpanded,

    //! Returns whether or not this row is a duplicate of another.
    IsDuplicate,
  };

  //! Factory method.
  static ITreeExplorerModel *Create(QObject *parent = nullptr);

  //! Constructor
  ITreeExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~ITreeExplorerModel() override = default;

  //! Install a new generator to back the data of this model.
  virtual void InstallGenerator(std::shared_ptr<ITreeGenerator> generator_) = 0;

  //! Expand starting at the model index, going up to `depth` levels deep.
  virtual void Expand(const QModelIndex &, unsigned depth) = 0;

  //! Find the original version of an item.
  virtual QModelIndex Deduplicate(const QModelIndex &) = 0;

 public slots:
  //! Cancels any running request
  virtual void CancelRunningRequest() = 0;

 private:
  //! Disabled copy constructor
  ITreeExplorerModel(const ITreeExplorerModel &) = delete;

  //! Disabled copy assignment operator
  ITreeExplorerModel &operator=(const ITreeExplorerModel &) = delete;

 signals:
  //! Emitted when a new request is started
  void RequestStarted();

  //! Emitted when a request has finished
  void RequestFinished();

  //! Emitted when the tree's name has change.
  void TreeNameChanged(const QString &new_name);
};

}  // namespace mx::gui
