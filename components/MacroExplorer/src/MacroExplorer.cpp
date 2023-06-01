/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "MacroExplorer.h"
#include "MacroExplorerItem.h"

#include <multiplier/Entities/MacroExpansion.h>
#include <multiplier/File.h>
#include <multiplier/Index.h>
#include <multiplier/TokenTree.h>
#include <multiplier/ui/ICodeModel.h>

#include <QDebug>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>

#include <unordered_map>
#include <vector>

namespace mx::gui {

struct MacroExplorer::PrivateData final : public TokenTreeVisitor {
  Index index;
  FileLocationCache file_location_cache;

  QVBoxLayout *scroll_layout{nullptr};

  // List of macro definitions or expansions/substitutions to expand.
  std::unordered_map<RawEntityId, MacroExplorerItem *> items;
  std::vector<MacroExplorerItem *> ordered_items;

  virtual ~PrivateData(void) = default;

  inline PrivateData(const Index &index_,
                     const FileLocationCache &file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_) {}

  // Return `true` if the input substitution should be expanded or not.
  bool should_expand(const MacroSubstitution &sub) const final {
    if (items.find(sub.id().Pack()) != items.end()) {
      return true;
    }

    if (auto exp = MacroExpansion::from(sub)) {
      if (auto def = exp->definition()) {
        return items.find(def->id().Pack()) != items.end();
      }
    }

    return false;
  }
};

ICodeModel *MacroExplorer::CreateCodeModel(
    const FileLocationCache &file_location_cache,
    const Index &index, QObject *parent) {
  ICodeModel *model = ICodeModel::Create(file_location_cache, index, parent);
  connect(this, &MacroExplorer::ExpandMacros,
          model, &ICodeModel::OnExpandMacros);
  return model;
}

void MacroExplorer::AlwaysExpandMacro(const DefineMacroDirective &def) {
  auto eid = def.id().Pack();
  if (d->items.find(eid) != d->items.end()) {
    return;
  }

  std::string_view def_name = def.name().data();

  MacroExplorerItem *item = new MacroExplorerItem(
      eid,
      QString::fromUtf8(
          def_name.data(), static_cast<qsizetype>(def_name.size())),
      std::nullopt,
      this);

  d->items.emplace(eid, item);
  d->ordered_items.push_back(item);

  UpdateList();
  emit ExpandMacros(d.get());
}

void MacroExplorer::ExpandSpecificMacro(const DefineMacroDirective &def,
                                        const MacroExpansion &exp) {

  auto eid = exp.id().Pack();
  if (d->items.find(eid) != d->items.end()) {
    return;
  }

  std::string_view def_name = def.name().data();

  std::optional<QString> loc_name;
  for (Token use_tok : exp.generate_use_tokens()) {
    Token file_tok = TokenRange(use_tok).file_tokens().front();
    if (!file_tok) {
      continue;
    }

    std::optional<File> file = File::containing(file_tok);
    if (!file) {
      continue;
    }

    QString loc;
    for (std::filesystem::path path : file->paths()) {
      loc = QString::fromStdString(path.filename().generic_string());
      break;
    }

    if (loc.isEmpty()) {
      continue;
    }

    if (auto line_col = file_tok.location(d->file_location_cache)) {
      loc += ":" + QString::number(line_col->first) + ":" +
             QString::number(line_col->second);
    }

    loc_name = std::move(loc);
    break;
  }

  auto item = new MacroExplorerItem(
      eid,
      QString::fromUtf8(
          def_name.data(), static_cast<qsizetype>(def_name.size())),
      loc_name,
      this);

  d->items.emplace(eid, item);
  d->ordered_items.push_back(item);

  UpdateList();
  emit ExpandMacros(d.get());
}

void MacroExplorer::ExpandSpecificSubstitution(const Token &use_tok,
                                               const MacroSubstitution &sub) {
  auto eid = sub.id().Pack();
  if (d->items.find(eid) != d->items.end()) {
    return;
  }

  std::string_view def_name = use_tok.data();

  std::optional<QString> loc_name;
  Token file_tok = TokenRange(use_tok).file_tokens().front();
  while (file_tok) {

    std::optional<File> file = File::containing(file_tok);
    if (!file) {
      break;
    }

    QString loc;
    for (std::filesystem::path path : file->paths()) {
      loc = QString::fromStdString(path.filename().generic_string());
      break;
    }

    if (loc.isEmpty()) {
      break;
    }

    if (auto line_col = file_tok.location(d->file_location_cache)) {
      loc += ":" + QString::number(line_col->first) + ":" +
             QString::number(line_col->second);
    }

    loc_name = std::move(loc);
    break;
  }

  auto item = new MacroExplorerItem(
      eid,
      QString::fromUtf8(
          def_name.data(), static_cast<qsizetype>(def_name.size())),
      loc_name,
      this);

  d->items.emplace(eid, item);
  d->ordered_items.push_back(item);

  UpdateList();
  emit ExpandMacros(d.get());
}

void MacroExplorer::RemoveMacro(RawEntityId macro_id) {
  auto it = d->items.find(macro_id);
  if (it == d->items.end()) {
    return;
  }

  MacroExplorerItem *item = it->second;
  d->items.erase(it);
  auto rit = std::remove(d->ordered_items.begin(), d->ordered_items.end(),
                         item);
  d->ordered_items.erase(rit, d->ordered_items.end());

  UpdateList();
  emit ExpandMacros(d.get());
}

void MacroExplorer::UpdateList(void) {

  QLayoutItem *child = nullptr;
  while ((child = d->scroll_layout->takeAt(0)) != nullptr) {
    if (QWidget *widget = child->widget()) {
      widget->setParent(nullptr);
    }
    delete child;   // delete the layout item
  }

  for (auto item : d->ordered_items) {
    d->scroll_layout->addWidget(item);
  }

  d->scroll_layout->addStretch();
}

void MacroExplorer::AddMacro(RawEntityId macro_id,
                             RawEntityId at_token_id) {
  VariantEntity macro_ent = d->index.entity(macro_id);
  if (!std::holds_alternative<Macro>(macro_ent)) {
    return;
  }

  Macro macro = std::move(std::get<Macro>(macro_ent));

  VariantEntity token_ent = d->index.entity(at_token_id);

  // Assume that token is nested inside of `macro`, or an expansion of `macro`.
  // If this is the case, then go and
  if (std::holds_alternative<Token>(token_ent)) {
    Token token = std::move(std::get<Token>(token_ent));
    for (Macro containing_macro : Macro::containing(token)) {
      if (auto exp = MacroExpansion::from(containing_macro)) {
        if (auto def = exp->definition()) {
          if (def.value() == macro) {
            ExpandSpecificMacro(def.value(), exp.value());
            return;
          }
        }
      } else if (auto sub = MacroSubstitution::from(containing_macro)) {
        if (sub == macro) {
          ExpandSpecificSubstitution(token, sub.value());
          return;
        }
      }
    }

  } else if (std::holds_alternative<NotAnEntity>(token_ent)) {
    if (auto def = DefineMacroDirective::from(macro)) {
      AlwaysExpandMacro(def.value());
    }
  } else {
    return;
  }
}

MacroExplorer::~MacroExplorer(void) {}

MacroExplorer::MacroExplorer(
    const Index &index, const FileLocationCache &file_location_cache,
    QWidget *parent)
    : IMacroExplorer(parent),
      d(new PrivateData(index, file_location_cache)) {

  auto scroll_area = new QScrollArea(this);
  scroll_area->setContentsMargins(0, 0, 0, 0);
  scroll_area->setWidgetResizable(true);

  d->scroll_layout = new QVBoxLayout();
  d->scroll_layout->setContentsMargins(0, 0, 0, 0);

  auto widget = new QWidget(this);
  widget->setLayout(d->scroll_layout);

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  scroll_area->setWidget(widget);
  layout->addWidget(scroll_area);
  setLayout(layout);
}

}  // namespace mx::gui
