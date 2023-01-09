/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>

#include <multiplier/File.h>
#include <multiplier/Index.h>

#include <memory>

namespace mx::gui {

class CodeModel final : public ICodeModel {
  Q_OBJECT

 public:
  virtual ~CodeModel() override;

  virtual const FileLocationCache &GetFileLocationCache() const override;
  virtual Index &GetIndex() override;

  virtual void SetFile(PackedFileId file_id) override;

  virtual int RowCount() const override;
  virtual int TokenCount(int row) const override;

  virtual QVariant Data(const CodeModelIndex &index,
                        int role = Qt::DisplayRole) const override;

 protected:
  CodeModel(const FileLocationCache &file_location_cache, const Index &index,
            QObject *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 private slots:
  void FutureResultStateChanged();

  friend class ICodeModel;
};

}  // namespace mx::gui
