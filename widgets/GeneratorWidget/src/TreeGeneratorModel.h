/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/ITreeGenerator.h>

#include <QVector>

namespace mx::gui {

class IGenerateTreeRunnable;
class ThemeManager;

//! Implements the IReferenceExplorerModel interface
class TreeGeneratorModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    EntityIdRole = IModel::MultiplierUserRole,

    //! Returns whether or not this row has been expanded.
    CanBeExpanded,

    //! Returns whether or not this row is a duplicate of another.
    IsDuplicate,
  };

  //! Constructor
  TreeGeneratorModel(const QString &model_id, QObject *parent = nullptr);

  //! Destructor
  virtual ~TreeGeneratorModel(void);

  //! Install a new generator to back the data of this model.
  void InstallGenerator(ITreeGeneratorPtr generator_);

  //! Expand starting at the model index, going up to `depth` levels deep.
  void Expand(const QModelIndex &, unsigned depth);

  //! Find the original version of an item.
  QModelIndex Deduplicate(const QModelIndex &);

  //! Creates a new Qt model index
  QModelIndex index(
      int row, int column, const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the parent of the given model index
  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the amount of columns in the model
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the index data for the specified role
  //! \todo Fix role=TokenCategoryRole support
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;

  //! Returns the column names for the tree view header
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const Q_DECL_FINAL;

 private:
  void RunExpansionThread(IGenerateTreeRunnable *runnable);

 private slots:

  //! Notify us when there's a batch of new data to update.
  void AddData(uint64_t version_number, uint64_t parent_item_id,
               QVector<IGeneratedItemPtr> child_items,
               unsigned remaining_depth);

  //! Processes the entire data batch queue.
  void ProcessData(void);

  //! When an expansion thread is done sending data, we get this.
  void OnRequestFinished(void);

 public slots:
  void CancelRunningRequest(void);
 
 signals:
  //! Emitted when a new request is started
  void RequestStarted(void);

  //! Emitted when a request has finished
  void RequestFinished(void);
};

}  // namespace mx::gui