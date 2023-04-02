/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityName.h"

#include <multiplier/ui/Util.h>

#include <multiplier/Entities/Token.h>
#include <multiplier/Entities/TokenKind.h>

namespace mx::gui {

namespace {

const std::size_t kBatchSize{512};

}  // namespace

void GetEntityList(QPromise<bool> &result_promise, const Index &index,
                   IDatabase::QueryEntitiesReceiver *receiver, QString name,
                   bool exact_name) {

  IDatabase::QueryEntitiesReceiver::DataBatch data_batch;
  std::string name_as_std_string = name.toStdString();

  for (NamedEntity named_entity : index.query_entities(name_as_std_string)) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (std::holds_alternative<mx::NamedDecl>(named_entity)) {
      const auto &named_decl = std::get<mx::NamedDecl>(named_entity);
      std::string_view decl_name = named_decl.name();
      if (decl_name.empty() ||
          (exact_name && decl_name != name_as_std_string)) {
        continue;
      }

      Token name_token = named_decl.token();
      if (name_token.data() != decl_name) {
        continue;
      }

      auto fragment = Fragment::containing(named_decl);

      data_batch.push_back(IDatabase::EntityQueryResult{
          fragment,
          File::containing(fragment),
          std::move(name_token),
          std::move(named_decl),
      });

    } else if (std::holds_alternative<mx::DefineMacroDirective>(named_entity)) {
      const auto &macro = std::get<mx::DefineMacroDirective>(named_entity);

      Token name_token = macro.name();
      std::string_view macro_name = name_token.data();
      if (macro_name.empty() ||
          (exact_name && macro_name != name_as_std_string)) {
        continue;
      }

      auto fragment = Fragment::containing(macro);

      data_batch.push_back(IDatabase::EntityQueryResult{
          fragment,
          File::containing(fragment),
          std::move(name_token),
          std::move(macro),
      });
    }

    if (data_batch.size() >= kBatchSize) {
      receiver->OnDataBatch(std::move(data_batch));
      data_batch.clear();
    }
  }

  if (!data_batch.empty()) {
    receiver->OnDataBatch(std::move(data_batch));
  }

  result_promise.addResult(true);
}

}  // namespace mx::gui
