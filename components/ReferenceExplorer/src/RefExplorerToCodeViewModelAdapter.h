/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>
#include <multiplier/ui/IReferenceExplorerModel.h>

#include <multiplier/Entities/TokenCategory.h>

#include <memory>

namespace mx::gui {

//! A code view model that displays data for the reference explorer
class RefExplorerToCodeViewModelAdapter final : public ICodeModel {
  Q_OBJECT

 public:
  //! Additional item data roles
  //! \note Make sure this is not overlapping the ICodeModel roles
  enum ItemDataRole {
    OriginalModelIndex = Qt::UserRole + 100,
    IsExpandButton
  };

  //! Constructor
  RefExplorerToCodeViewModelAdapter(IReferenceExplorerModel *model,
                                    QObject *parent);

  //! Destructor
  virtual ~RefExplorerToCodeViewModelAdapter() override;

  //! \copybrief ICodeModel::GetFileLocationCache
  //! \todo This method needs to be removed from the interface
  [[noreturn]] virtual const FileLocationCache &
  GetFileLocationCache() const override;

  //! \copybrief ICodeModel::GetIndex
  //! \todo This method needs to be removed from the interface
  [[noreturn]] virtual Index &GetIndex() override;

  //! \copybrief ICodeModel::GetEntity
  //! \todo This method needs to be removed from the interface
  [[noreturn]] virtual std::optional<RawEntityId> GetEntity() const override;

  //! \copybrief ICodeModel::SetEntity
  //! \todo This method needs to be removed from the interface
  [[noreturn]] virtual void SetEntity(RawEntityId id) override;

  //! \copybrief ICodeModel::RowCount
  virtual Count RowCount() const override;

  //! \copybrief ICodeModel::TokenCount
  virtual Count TokenCount(Count row) const override;

  //! \copybrief ICodeModel::Data
  virtual QVariant Data(const CodeModelIndex &index,
                        int role = Qt::DisplayRole) const override;

  //! \copybrief ICodeModel::IsReady
  virtual bool IsReady() const override;

  //! Token data that represents all the references
  struct Context final {
    //! Token data for the code view
    struct Token final {
      QString data;
      TokenCategory category{TokenCategory::UNKNOWN};
      std::uint64_t id{};
    };

    //! A group of tokens that represent a row
    using TokenList = std::vector<Token>;

    //! Contains both the token list and the original QModelIndex
    struct Row final {
      TokenList token_list;
      QModelIndex original_model_index;
    };

    //! A list of row objects for the code view
    using RowList = std::vector<Row>;

    //! All the data imported from the original ref explorer model
    RowList row_list;
  };

  //! Imports the given model by generating tokens for the code view
  static void
  ImportReferenceExplorerModel(Context &context,
                               const IReferenceExplorerModel *model);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 private slots:
  //! Used to invalidate the code view tokens in response to model changes
  void OnModelChange();
};

}  // namespace mx::gui
