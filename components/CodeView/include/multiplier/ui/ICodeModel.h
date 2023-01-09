/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Types.h>
#include <QObject>
#include <QVariant>

namespace mx {
class FileLocationCache;
class Index;
}  // namespace mx
namespace mx::gui {

struct CodeModelIndex {
  int row{};
  int token_index{};
};

class ICodeModel : public QObject {
  Q_OBJECT

 public:
  // Other compatible roles:
  // Qt::DisplayRole
  enum {
    TokenCategoryRole = Qt::UserRole,
    TokenRawEntityIdRole,
  };

  static ICodeModel *Create(const FileLocationCache &file_location_cache,
                            const Index &index, QObject *parent = nullptr);

  virtual const FileLocationCache &GetFileLocationCache() const = 0;
  virtual Index &GetIndex() = 0;

  virtual void SetFile(PackedFileId file_id) = 0;

  virtual int RowCount() const = 0;
  virtual int TokenCount(int row) const = 0;

  virtual QVariant Data(const CodeModelIndex &index,
                        int role = Qt::DisplayRole) const = 0;

  ICodeModel(QObject *parent) : QObject(parent) {}
  virtual ~ICodeModel() override = default;

  ICodeModel(const ICodeModel &) = delete;
  ICodeModel &operator=(const ICodeModel &) = delete;

 signals:
  void ModelAboutToBeReset();
  void ModelReset();
};

}  // namespace mx::gui
