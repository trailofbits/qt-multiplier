/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/IListGenerator.h>

#include <QList>

namespace mx::gui {

class IGenerateTreeRunnable;
class ThemeManager;

//! Implements the IReferenceExplorerModel interface
class ListGeneratorModel Q_DECL_FINAL : public IModel {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Location object
    EntityIdRole = IModel::MultiplierUserRole,

    //! Returns whether or not this row is a duplicate of another.
    IsDuplicate
  };

  //! Constructor
  ListGeneratorModel(QObject *parent = nullptr);

  //! Destructor
  virtual ~ListGeneratorModel(void);

  //! Install a new generator to back the data of this model.
  void InstallGenerator(IListGeneratorPtr generator_);

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
  void OnNewListItems(uint64_t version_number, RawEntityId,
                      QList<IGeneratedItemPtr> child_items,
                      unsigned);

  //! Processes the entire data batch queue
  void ProcessDataBatchQueue(void);

  //! Called when the tree title has been resolved.
  void OnNameResolved(void);

 public slots:
  void CancelRunningRequest(void);

  void OnThemeChanged(const ThemeManager &theme);
 
 signals:
  //! Emitted when a new request is started
  void RequestStarted(void);

  //! Emitted when a request has finished
  void RequestFinished(void);

  //! Emitted when the tree's name has change.
  void NameChanged(QString new_name);
};

}  // namespace mx::gui