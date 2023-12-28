/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "GetEntityName.h"

#include <multiplier/GUI/Util.h>

#include <multiplier/Frontend/Token.h>
#include <multiplier/Frontend/TokenKind.h>

namespace mx::gui {

namespace {

const std::size_t kBatchSize{512};

}  // namespace

void GetEntityList(QPromise<bool> &result_promise, const Index &index,
                   IDatabase::QueryEntitiesReceiver *receiver,
                   const QString &string,
                   const IDatabase::QueryEntitiesMode &query_mode) {

  IDatabase::QueryEntitiesReceiver::DataBatch data_batch;
  std::string std_string = string.toStdString();

  for (NamedEntity named_entity : index.query_entities(std_string)) {
    if (result_promise.isCanceled()) {
      return;
    }

    if (std::holds_alternative<NamedDecl>(named_entity)) {
      NamedDecl &named_decl = std::get<NamedDecl>(named_entity);

      std::string_view decl_name = named_decl.name();
      if (decl_name.empty()) {
        continue;
      }

      auto skip_entity{true};
      if (query_mode == IDatabase::QueryEntitiesMode::ExactMatch) {
        skip_entity = decl_name != std_string;
      } else {
        skip_entity = decl_name.find(std_string) == std::string::npos;
      }

      if (skip_entity) {
        continue;
      }

      Token name_token = named_decl.token();
      if (name_token.data() != decl_name) {
        continue;
      }

      data_batch.push_back(std::move(name_token));

    } else if (std::holds_alternative<DefineMacroDirective>(named_entity)) {
      DefineMacroDirective &macro =
          std::get<DefineMacroDirective>(named_entity);

      Token name_token = macro.name();
      std::string_view macro_name = name_token.data();
      if (macro_name.empty()) {
        continue;
      }

      auto skip_entity{true};
      if (query_mode == IDatabase::QueryEntitiesMode::ExactMatch) {
        skip_entity = macro_name != std_string;
      } else {
        skip_entity = macro_name.find(std_string) == std::string::npos;
      }

      if (skip_entity) {
        continue;
      }

      data_batch.emplace_back(std::move(name_token));
    
    } else if (std::holds_alternative<File>(named_entity)) {
      File &file = std::get<File>(named_entity);
      for (std::filesystem::path path : file.paths()) {
        auto path_str = path.generic_string();
        auto skip_entity{true};
        if (query_mode == IDatabase::QueryEntitiesMode::ExactMatch) {
          skip_entity = path_str != std_string;
        } else {
          skip_entity = path_str.find(std_string) == std::string::npos;
        }

        if (skip_entity) {
          continue;
        }

        UserToken path_tok;
        path_tok.category = TokenCategory::FILE_NAME;
        path_tok.kind = TokenKind::HEADER_NAME;
        path_tok.related_entity = file;
        path_tok.data = std::move(path_str);

        std::vector<CustomToken> toks;
        toks.emplace_back(std::move(path_tok));

        data_batch.emplace_back(TokenRange::create(std::move(toks)).front());
        break;
      }
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
