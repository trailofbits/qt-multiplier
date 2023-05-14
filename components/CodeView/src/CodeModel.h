/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>

#include <multiplier/Index.h>

#include <memory>
#include <variant>
#include <unordered_map>

namespace mx::gui {

//! Main implementation for the ICodeModel interface
class CodeModel final : public ICodeModel {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~CodeModel() override;

  //! \copybrief ICodeModel::GetEntity
  virtual std::optional<RawEntityId> GetEntity(void) const override;

  //! \copybrief ICodeModel::SetEntity
  virtual void SetEntity(RawEntityId id) override;

  //! \copybrief ICodeModel::IsReady
  virtual bool IsReady() const override;

  //! Creates a new Qt model index
  virtual QModelIndex index(int row, int column,
                            const QModelIndex &parent) const override;

  //! Returns the parent of the given model index
  virtual QModelIndex parent(const QModelIndex &child) const override;

  //! Returns the amount of rows in the given parent item
  virtual int rowCount(const QModelIndex &parent) const override;

  //! Returns the amount of columns for the given parent item
  virtual int columnCount(const QModelIndex &parent) const override;

  //! Returns the data for the specified role
  virtual QVariant data(const QModelIndex &index, int role) const override;

 protected:
  //! Constructor
  CodeModel(const FileLocationCache &file_location_cache, const Index &index,
            QObject *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Aborts the active request
  void CancelRunningRequest();

 private slots:
  //! Called when the async database request is ready
  void FutureResultStateChanged(void);

  friend class ICodeModel;
};

}  // namespace mx::gui
