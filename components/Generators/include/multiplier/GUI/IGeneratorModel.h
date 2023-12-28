/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <memory>
#include <multiplier/Types.h>
#include <multiplier/ui/IModel.h>

namespace mx::gui {

class ITreeGenerator;

//! A model for the reference explorer widget
class IGeneratorModel : public IModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    EntityIdRole = IModel::MultiplierUserRole,

    //! Returns the token range associated with a given `QModelIndex`. This is
    //! for styled displaying.
    TokenRangeRole,

    //! Returns whether or not this row has been expanded.
    CanBeExpanded,

    //! Returns whether or not this row is a duplicate of another.
    IsDuplicate,

    //! Returns the tree name
    TreeNameRole,
  };

  //! Factory method.
  static IGeneratorModel *Create(QObject *parent = nullptr);

  //! Constructor
  IGeneratorModel(QObject *parent) : IModel(parent) {}

  //! Destructor
  virtual ~IGeneratorModel();

  //! Install a new generator to back the data of this model.
  virtual void InstallGenerator(std::shared_ptr<ITreeGenerator> generator_) = 0;

  //! Expand starting at the model index, going up to `depth` levels deep.
  virtual void Expand(const QModelIndex &index, unsigned depth) = 0;

  //! Find the original version of an item.
  virtual QModelIndex Deduplicate(const QModelIndex &index) = 0;

 public slots:
  //! Cancels any running request
  virtual void CancelRunningRequest() = 0;

 private:
  //! Disabled copy constructor
  IGeneratorModel(const IGeneratorModel &) = delete;

  //! Disabled copy assignment operator
  IGeneratorModel &operator=(const IGeneratorModel &) = delete;

 signals:
  //! Emitted when a new request is started
  void RequestStarted();

  //! Emitted when a request has finished
  void RequestFinished();

  //! Emitted when the tree's name has change.
  void TreeNameChanged(QString new_name);
};

}  // namespace mx::gui
