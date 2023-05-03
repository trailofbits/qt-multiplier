/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Types.h>
#include <multiplier/Index.h>

#include <QAbstractItemModel>

namespace mx::gui {

//! An item-based model for the ICodeView widget
class ICodeModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Custom data roles
  enum {
    //! Token category, used for syntax coloring
    TokenCategoryRole = Qt::UserRole + 1,

    //! The RawEntityId value for the specified model index
    TokenIdRole,

    //! A line number integer for the specified model index
    LineNumberRole,

    //! Returns the group id (if any) of the specified model index
    TokenGroupIdRole,

    //! The RawEntityId value associated with the "related entity" of this
    //! token.
    RelatedEntityIdRole,

    //! The real related entity id associated with this token. In the case of
    //! code previews, we hide the related entity ID, and return the token ID
    //! instead. But internal to the `CodeView`, we like to be able to access
    //! the real related entity ID so that we can highlight other uses.
    RealRelatedEntityIdRole,

    //! The raw form of the `StmtId` for the statement containing this token.
    EntityIdOfStmtContainingTokenRole,
  };

  //! Factory function
  static ICodeModel *Create(const FileLocationCache &file_location_cache,
                            const Index &index, QObject *parent = nullptr);

  //! Asks the model for the currently showing entity. This is usually a file
  //! id or a fragment id.
  virtual std::optional<RawEntityId> GetEntity() const = 0;

  //! Asks the model to fetch the specified entity.
  virtual void SetEntity(RawEntityId id) = 0;

  //! Returns true if the model is not currently running any operation
  virtual bool IsReady() const = 0;

  //! Constructor
  ICodeModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~ICodeModel() override = default;

  //! Disabled copy constructor
  ICodeModel(const ICodeModel &) = delete;

  //! Disabled copy assignment operator
  ICodeModel &operator=(const ICodeModel &) = delete;
};

}  // namespace mx::gui
