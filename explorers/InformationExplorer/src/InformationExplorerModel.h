/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QObject>
#include <QString>

#include <multiplier/GUI/IModel.h>
#include <multiplier/GUI/IDatabase.h>

#include <unordered_map>
#include <vector>
#include <unordered_set>

namespace mx {
class Index;
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

//! Implemented the model for the tree view in the information explorer.
class InformationExplorerModel Q_DECL_FINAL
    : public IModel,
      public IDatabase::RequestEntityInformationReceiver {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a QString containing the file path and cursor position
    LocationRole = IModel::MultiplierUserRole,

    //! Returns the RawEntityId value
    EntityIdRole,

    //! Returns the TokenRange data
    TokenRangeRole,

    //! Returns true if tokens should never be painted for an index
    ForceTextPaintRole,

    //! Returns a boolean that tells the view whether to auto-expand
    AutoExpandRole
  };

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

  //! Destructor
  virtual ~InformationExplorerModel(void) override;

  //! Constructor
  InformationExplorerModel(const Index &index,
                           const FileLocationCache &file_location_cache,
                           QObject *parent);

  Index GetIndex() const;
  FileLocationCache GetFileLocationCache() const;
  void RequestEntityInformation(const RawEntityId &entity_id);
  RawEntityId GetCurrentEntityID(void) const;
  std::optional<QString> GetCurrentEntityName() const;

  //! Creates a new Qt model index
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the parent of the given model index
  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the amount of columns in the model
  //! \return Zero if the parent index is not valid. One otherwise (file name)
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the index data for the specified role
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;

 private slots:
  //! Called whenever the info database request future changes status
  void InfoFutureResultStateChanged(void);

  //! Called whenever the name database request future changes status
  void NameFutureResultStateChanged(void);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Cancels any active request
  void CancelRunningRequest(void);

  //! Receives data batches from the IDatabase class
  void OnDataBatch(DataBatch data_batch) Q_DECL_FINAL;

  void ClearTree(void);

  //! Imports the given entity information data
  void ImportEntityInformation(EntityInformation &entity_information);

 private slots:
  //! Processes all the batches in the queue at fixed interval
  void ProcessDataBatchQueue(void);
};

}  // namespace mx::gui

//! Used to store internal sorting data in a QVariant object
Q_DECLARE_METATYPE(mx::gui::InformationExplorerModel::RawLocation);
