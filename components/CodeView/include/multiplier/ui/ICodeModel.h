/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/File.h>
#include <multiplier/Index.h>

#include <QAbstractTableModel>

namespace mx {

class ICodeModel : public QAbstractTableModel {
  Q_OBJECT

 public:
  enum class TokenClass {
    kUnknown,
    kIdentifier,
    kMacroName,
    kKeyword,
    kObjectiveCKeyword,
    kPreProcessorKeyword,
    kBuiltinTypeName,
    kPunctuation,
    kLiteral,
    kComment
  };

  static ICodeModel *Create(const FileLocationCache &file_location_cache,
                            Index index, QObject *parent = nullptr);

  virtual const FileLocationCache &GetFileLocationCache() const = 0;
  virtual Index &GetIndex() = 0;

  virtual void SetFile(const Index &index, RawEntityId file_id) = 0;

  ICodeModel(QObject *parent) : QAbstractTableModel(parent) {}
  virtual ~ICodeModel() override = default;

  ICodeModel(const ICodeModel &) = delete;
  ICodeModel &operator=(const ICodeModel &) = delete;
};

}  // namespace mx
