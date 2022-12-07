/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodeModel.h"

namespace mx::gui {

struct CodeModel::PrivateData final {
  PrivateData(const FileLocationCache &file_location_cache_, Index index_)
      : file_location_cache(file_location_cache_),
        index(std::move(index_)) {}

  const FileLocationCache &file_location_cache;
  Index index;
};

CodeModel::~CodeModel() {}

const FileLocationCache &CodeModel::GetFileLocationCache() const {
  return d->file_location_cache;
}

Index &CodeModel::GetIndex() {
  return d->index;
}

void CodeModel::SetFile(const Index &index, RawEntityId file_id) {
  emit beginResetModel();

  emit endResetModel();
}

int CodeModel::rowCount(const QModelIndex &parent) const {
  return 0;
}

int CodeModel::columnCount(const QModelIndex &parent) const {
  return 0;
}

QVariant CodeModel::data(const QModelIndex &index, int role) const {
  return QVariant();
}

CodeModel::CodeModel(const FileLocationCache &file_location_cache, Index index,
                     QObject *parent)
    : ICodeModel(parent),
      d(new PrivateData(file_location_cache, index)) {}

}  // namespace mx::gui
