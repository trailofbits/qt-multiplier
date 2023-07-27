/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QObject>
#include <QString>

#include <multiplier/ui/IInformationExplorerModel.h>
#include <multiplier/ui/IDatabase.h>

#include <unordered_map>
#include <vector>
#include <unordered_set>

namespace mx::gui {

//! Implements the IInformationExplorerModel interface
class InformationExplorerModel final
    : public IInformationExplorerModel,
      public IDatabase::RequestEntityInformationReceiver {
  Q_OBJECT

 public:
  //! Data for RawLocationRole
  //! \todo An mx_common header-only library with these types?
  struct RawLocation final {
    //! File path
    QString path;

    //! Line number
    unsigned line_number{};

    //! Column number
    unsigned column_number{};
  };

  //! Additional internal item data roles for this model
  enum ItemDataRole {
    //! Returns true if tokens should never be painted for an index
    ForceTextPaintRole = Qt::UserRole + 100,

    //! Returns a boolean that tells the view whether to auto-expand
    AutoExpandRole
  };

  //! \copybrief IInformationExplorerModel::GetIndex
  virtual Index GetIndex() const override;

  //! \copybrief IInformationExplorerModel::GetFileLocationCache
  virtual FileLocationCache GetFileLocationCache() const override;

  //! \copybrief IInformationExplorerModel::RequestEntityInformation
  virtual void RequestEntityInformation(const RawEntityId &entity_id) override;

  //! \copybrief IInformationExplorerModel::GetCurrentEntityID
  virtual RawEntityId GetCurrentEntityID(void) const override;

  //! Destructor
  virtual ~InformationExplorerModel(void) override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns in the model
  //! \return Zero if the parent index is not valid. One otherwise (file name)
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the index data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

 private slots:
  //! Called whenever the database request future changes status
  void FutureResultStateChanged();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  InformationExplorerModel(Index index, FileLocationCache file_location_cache,
                           QObject *parent);

  //! Cancels any active request
  void CancelRunningRequest();

  //! Receives data batches from the IDatabase class
  virtual void OnDataBatch(DataBatch data_batch) override;

  void ClearTree(void);

  //! Imports the given entity information data
  void ImportEntityInformation(EntityInformation &entity_information);

 private slots:
  //! Processes all the batches in the queue at fixed interval
  void ProcessDataBatchQueue();

  friend class IInformationExplorerModel;
};

}  // namespace mx::gui

//! Used to store internal sorting data in a QVariant object
Q_DECLARE_METATYPE(mx::gui::InformationExplorerModel::RawLocation);
