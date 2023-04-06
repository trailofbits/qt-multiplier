/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
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

//! A code view model that adapts the normal code model so that all related
//! entity IDs are actually the normal token ids.
class CodePreviewModelAdapter final : public ICodeModel {
  Q_OBJECT

 public:
  //! Constructor
  CodePreviewModelAdapter(ICodeModel *model, QObject *parent);

  //! Destructor
  virtual ~CodePreviewModelAdapter(void) = default;

  //! Returns the internal mx::FileLocationCache object
  virtual const FileLocationCache &GetFileLocationCache(void) const Q_DECL_FINAL;

  //! Returns the internal mx::Index object
  virtual Index &GetIndex() Q_DECL_FINAL;

  //! Asks the model for the currently showing entity. This is usually a file
  //! id or a fragment id.
  virtual std::optional<RawEntityId> GetEntity(void) const Q_DECL_FINAL;

  //! Asks the model to fetch the specified entity.
  virtual void SetEntity(RawEntityId id) Q_DECL_FINAL;

  //! How many rows are accessible from this model
  virtual int RowCount() const Q_DECL_FINAL;

  //! How many tokens are accessible on the specified column
  virtual int TokenCount(int row) const Q_DECL_FINAL;

  //! Returns the data role contents for the specified model index
  virtual QVariant Data(const CodeModelIndex &index,
                        int role) const Q_DECL_FINAL;

  //! Returns true if this model is ready.
  virtual bool IsReady() const Q_DECL_FINAL;

 private:
  ICodeModel * const next;
};

}  // namespace mx::gui
