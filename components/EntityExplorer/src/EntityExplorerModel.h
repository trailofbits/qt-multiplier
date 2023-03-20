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

  //! Returns the amount of rows in the model
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the item data role value for the specified model index
  virtual QVariant data(const QModelIndex &index,
                        int role = Qt::DisplayRole) const override;

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

  virtual void
  OnDataBatch(IDatabase::QueryEntitiesReceiver::DataBatch data_batch) override;

 private slots:
  //! Called when the database query future changes status
  void RequestFutureStatusChanged();

  friend class IEntityExplorerModel;
};

}  // namespace mx::gui
