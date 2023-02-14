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

//! A model index used to reference a single token
struct CodeModelIndex final {
  int row{};
  int token_index{};
};

//! A code model interface inspired by Qt, used by the ICodeView widget
class ICodeModel : public QObject {
  Q_OBJECT

 public:
  //! Compatible data roles that can be used in addition to Qt::DisplayRole
  enum {
    //! Token category, used for syntax coloring
    TokenCategoryRole = Qt::UserRole + 1,

    //! The RawEntityId value for the specified model index
    TokenRawEntityIdRole,

    //! A line number integer for the specified model index
    LineNumberRole,

    //! Returns the group id (if any) of the specified model index
    TokenGroupIdRole,

    //! The RawEntityId value associated with the "related entity" of this token.
    TokenRelatedEntityIdRole,
  };

  //! Factory function
  static ICodeModel *Create(const FileLocationCache &file_location_cache,
                            const Index &index, QObject *parent = nullptr);

  //! Returns the internal mx::FileLocationCache object
  virtual const FileLocationCache &GetFileLocationCache() const = 0;

  //! Returns the internal mx::Index object
  virtual Index &GetIndex() = 0;

  //! Asks the model to fetch the specified file
  virtual void SetFile(PackedFileId file_id) = 0;

  //! How many rows are accessible from this model
  virtual int RowCount() const = 0;

  //! How many tokens are accessible on the specified column
  virtual int TokenCount(int row) const = 0;

  //! Returns the data role contents for the specified model index
  virtual QVariant Data(const CodeModelIndex &index,
                        int role = Qt::DisplayRole) const = 0;

  //! Constructor
  ICodeModel(QObject *parent) : QObject(parent) {}

  //! Destructor
  virtual ~ICodeModel() override = default;

  ICodeModel(const ICodeModel &) = delete;
  ICodeModel &operator=(const ICodeModel &) = delete;

 signals:
  //! This signal is emitted before a model is reset
  void ModelAboutToBeReset();

  //! This signal is emitted at the end of the model reset process
  void ModelReset();
};

}  // namespace mx::gui

//! Allows mx::gui::CodeModelIndex values to fit inside QVariant objects
Q_DECLARE_METATYPE(mx::gui::CodeModelIndex);
