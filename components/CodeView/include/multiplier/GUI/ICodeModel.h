/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Types.h>
#include <multiplier/GUI/IModel.h>

#include <utility>

namespace mx {
class FileLocationCache;
class Index;
class Token;
class TokenRange;
class TokenTreeVisitor;
}  // namespace mx

namespace mx::gui {

//! An item-based model for the ICodeView widget
class ICodeModel : public IModel {
  Q_OBJECT

 public:
  enum class ModelState {
    Uninitialized,
    UpdateInProgress,
    UpdateFailed,
    UpdateCancelled,
    Ready,
  };

  //! Custom data roles
  enum {
    //! Token category, used for syntax coloring
    TokenCategoryRole = MultiplierUserRole,

    //! Tells us the `ModelState` of the model.
    ModelStateRole,

    //! The RawEntityId value for the specified model index
    TokenIdRole,

    //! A line number integer for the specified model index
    LineNumberRole,

    //! The RawEntityId value associated with the "related entity" of this
    //! token.
    RelatedEntityIdRole,

    //! The real related entity id associated with this token. In the case of
    //! code previews, we hide the related entity ID, and return the token ID
    //! instead. But internal to the `CodeView`, we like to be able to access
    //! the real related entity ID so that we can highlight other uses.
    RealRelatedEntityIdRole,

    //! Returns true if this is part of a macro expansion
    IsMacroExpansionRole,
  };

  //! Factory function
  static ICodeModel *
  Create(const FileLocationCache &file_location_cache, const Index &index,
         const bool &remap_related_entity_id_role, QObject *parent = nullptr);

  static std::optional<std::pair<RawEntityId, RawEntityId>>
  MacroExpansionPoint(const QModelIndex &index);

  //! Asks the model to fetch the specified entity.
  virtual void SetEntity(RawEntityId id) = 0;

  //! Returns true if the model is not currently running any operation
  virtual bool IsReady() const = 0;

  //! Constructor
  using IModel::IModel;

  //! Destructor
  virtual ~ICodeModel() override = default;

  //! Disabled copy constructor
  ICodeModel(const ICodeModel &) = delete;

  //! Disabled copy assignment operator
  ICodeModel &operator=(const ICodeModel &) = delete;
 
 signals:
  //! Just before model will be loaded, this tells us the location of the
  //! entity corresponding to the last call to `SetEntity`.
  void EntityLocation(RawEntityId id, unsigned line, unsigned col);

 public slots:
  //! Tells this code view to use the `TokenTreeVisitor` to expand some
  //! macros.
  virtual void OnExpandMacros(const TokenTreeVisitor *);
};

}  // namespace mx::gui
