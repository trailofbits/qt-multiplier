/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetTokenTree.h"

#include <multiplier/Token.h>

namespace mx::gui {

void GetTokenTree(
    QPromise<IDatabase::TokenTreeResult> &token_tree_promise,
    const Index &index, RawEntityId entity_id) {

  VariantEntity ent = index.entity(entity_id);
  if (std::holds_alternative<NotAnEntity>(ent)) {
    token_tree_promise.addResult(RPCErrorCode::InvalidEntityID);

  } else if (std::holds_alternative<File>(ent)) {
    token_tree_promise.addResult(TokenTree::from(std::get<File>(ent)));

  } else if (std::holds_alternative<Fragment>(ent)) {
    token_tree_promise.addResult(TokenTree::from(std::get<Fragment>(ent)));

  } else if (auto frag = Fragment::containing(ent)) {
    token_tree_promise.addResult(TokenTree::from(frag.value()));

  } else if (auto file = File::containing(ent)) {
    token_tree_promise.addResult(TokenTree::from(file.value()));

  // TODO(pag): Support token trees for types? That would go in mx-api.
  } else {
    token_tree_promise.addResult(RPCErrorCode::UnsupportedTokenTreeEntityType);
  }
}

}  // namespace mx::gui
