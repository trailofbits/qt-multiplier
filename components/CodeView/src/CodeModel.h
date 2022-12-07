/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ICodeModel.h>

#include <memory>

namespace mx {

class CodeModel final : public ICodeModel {
  Q_OBJECT

 public:
  virtual ~CodeModel() override;

  virtual const FileLocationCache &GetFileLocationCache() const override;
  virtual Index &GetIndex() override;

  virtual void SetFile(const Index &index, RawEntityId file_id) override;

  virtual int
  rowCount(const QModelIndex &parent = QModelIndex()) const override;
  virtual int
  columnCount(const QModelIndex &parent = QModelIndex()) const override;
  virtual QVariant data(const QModelIndex &index,
                        int role = Qt::DisplayRole) const override;

 protected:
  CodeModel(const FileLocationCache &file_location_cache, Index index,
            QObject *parent);

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  friend class ICodeModel;
};

}  // namespace mx
