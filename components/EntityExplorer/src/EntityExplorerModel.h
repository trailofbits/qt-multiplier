/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IEntityExplorerModel.h>
#include <multiplier/ui/IDatabase.h>

#include <memory>

namespace mx::gui {

//! Main class implementing the IEntityExplorerModel interface
class EntityExplorerModel final : public IEntityExplorerModel,
                                  public IDatabase::QueryEntitiesReceiver {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~EntityExplorerModel() override;

  //! \copybrief IEntityExplorerModel::SetSortingMethod
  virtual void SetSortingMethod(const SortingMethod &sorting_method) override;

  //! \copybrief IEntityExplorerModel::SetFilterRegularExpression
  virtual void
  SetFilterRegularExpression(const QRegularExpression &regex) override;

  //! \copybrief IEntityExplorerModel::SetTokenCategoryFilter
  virtual void SetTokenCategoryFilter(
      const std::optional<TokenCategorySet> &opt_token_category_set) override;

  //! Returns the amount of rows in the model
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the item data role value for the specified model index
  virtual QVariant data(const QModelIndex &index,
                        int role = Qt::DisplayRole) const override;

  //! Returns a model index for the specified item
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent model index for the specified item
  //! \return Always returns an invalid model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount of column under the given parent
  //! \return Always returns 1
  virtual int columnCount(const QModelIndex &parent) const override;

 public slots:
  //! Starts a new search request
  virtual void Search(const QString &name, const bool &exact_name) override;

  //! Cancels the active search, if any
  virtual void CancelSearch() override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  EntityExplorerModel(const Index &index,
                      const FileLocationCache &file_location_cache,
                      QObject *parent);

  //! Receives data batches from the IDatabase class
  virtual void
  OnDataBatch(IDatabase::QueryEntitiesReceiver::DataBatch data_batch) override;

  //! Regenerates the rows based on the active filters
  void GenerateRows(void);

  //! Sort the rows.
  void SortRows(void);

  friend class IEntityExplorerModel;
};

}  // namespace mx::gui
