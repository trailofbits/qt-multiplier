/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "MacroExplorer.h"
#include "MacroExplorerItem.h"

#include <multiplier/Frontend/DefineMacroDirective.h>
#include <multiplier/Frontend/File.h>
#include <multiplier/Frontend/MacroExpansion.h>
#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/Index.h>
#include <multiplier/ui/ICodeModel.h>

#include <QDebug>
#include <QLabel>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>

#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace mx::gui {

struct MacroExplorer::PrivateData final : public TokenTreeVisitor {
  Index index;
  FileLocationCache file_location_cache;

  QVBoxLayout *scroll_layout{nullptr};

  // List of macro definitions or expansions/substitutions to expand.
  std::unordered_map<RawEntityId, MacroExplorerItem *> items;
  std::vector<MacroExplorerItem *> ordered_items;

  mutable std::shared_mutex lock;

  virtual ~PrivateData(void) = default;

  inline PrivateData(const Index &index_,
                     const FileLocationCache &file_location_cache_)
      : index(index_),
        file_location_cache(file_location_cache_) {}

  // Return `true` if the input substitution should be expanded or not.
  virtual bool should_expand(const MacroSubstitution &sub) const override {
    std::shared_lock<std::shared_mutex> locker(lock);

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
    const FileLocationCache &file_location_cache, const Index &index,
    const bool &remap_related_entity_id_role, QObject *parent) {
  ICodeModel *model = ICodeModel::Create(file_location_cache, index,
                                         remap_related_entity_id_role, parent);
  model->OnExpandMacros(d.get());
  connect(this, &MacroExplorer::ExpandMacros, model,
          &ICodeModel::OnExpandMacros);
  return model;
}

namespace {

static std::optional<QString> GetLocation(Token tok,
                                          const FileLocationCache &loc_cache) {
  Token file_tok = TokenRange(tok).file_tokens().front();
  if (!file_tok) {
    return std::nullopt;
  }

  std::optional<File> file = File::containing(file_tok);
  if (!file) {
    return std::nullopt;
  }

  QString loc;
  for (std::filesystem::path path : file->paths()) {
    loc = QString::fromStdString(path.filename().generic_string());
    break;
  }

  if (loc.isEmpty()) {
    return std::nullopt;
  }

  if (auto line_col = file_tok.location(loc_cache)) {
    loc += ":" + QString::number(line_col->first) + ":" +
           QString::number(line_col->second);
  }

  return loc;
}

}  // namespace

void MacroExplorer::AlwaysExpandMacro(const DefineMacroDirective &def) {
  std::unique_lock<std::shared_mutex> locker(d->lock);

  auto eid = def.id().Pack();
  if (d->items.find(eid) != d->items.end()) {
    return;
  }

  std::string_view def_name = def.name().data();

  MacroExplorerItem *item = new MacroExplorerItem(
      eid, true,
      QString::fromUtf8(def_name.data(),
                        static_cast<qsizetype>(def_name.size())),
      GetLocation(def.name(), d->file_location_cache), this);

  d->items.emplace(eid, item);
  d->ordered_items.push_back(item);

  UpdateList();

  locker.unlock();
  emit ExpandMacros(d.get());
}

void MacroExplorer::ExpandSpecificMacro(const DefineMacroDirective &def,
                                        const MacroExpansion &exp) {
  std::unique_lock<std::shared_mutex> locker(d->lock);

  auto eid = exp.id().Pack();
  if (d->items.find(eid) != d->items.end()) {
    return;
  }

  std::string_view def_name = def.name().data();

  std::optional<QString> loc_name;
  for (Token use_tok : exp.generate_use_tokens()) {
    loc_name = GetLocation(use_tok, d->file_location_cache);
    if (loc_name.has_value()) {
      break;
    }
  }

  auto item = new MacroExplorerItem(
      eid, false,
      QString::fromUtf8(def_name.data(),
                        static_cast<qsizetype>(def_name.size())),
      loc_name, this);

  d->items.emplace(eid, item);
  d->ordered_items.push_back(item);

  UpdateList();

  locker.unlock();
  emit ExpandMacros(d.get());
}

void MacroExplorer::ExpandSpecificSubstitution(const Token &use_tok,
                                               const MacroSubstitution &sub) {
  std::unique_lock<std::shared_mutex> locker(d->lock);
  auto eid = sub.id().Pack();
  if (d->items.find(eid) != d->items.end()) {
    return;
  }

  std::string_view def_name = use_tok.data();
  auto item = new MacroExplorerItem(
      eid, false,
      QString::fromUtf8(def_name.data(),
                        static_cast<qsizetype>(def_name.size())),
      GetLocation(use_tok, d->file_location_cache), this);

  d->items.emplace(eid, item);
  d->ordered_items.push_back(item);

  UpdateList();

  locker.unlock();
  emit ExpandMacros(d.get());
}

void MacroExplorer::RemoveMacro(RawEntityId macro_id) {
  std::unique_lock<std::shared_mutex> locker(d->lock);
  auto it = d->items.find(macro_id);
  if (it == d->items.end()) {
    return;
  }

  MacroExplorerItem *item = it->second;
  d->items.erase(it);
  auto rit =
      std::remove(d->ordered_items.begin(), d->ordered_items.end(), item);
  d->ordered_items.erase(rit, d->ordered_items.end());

  UpdateList();

  locker.unlock();
  emit ExpandMacros(d.get());
}

void MacroExplorer::OnThemeChange(const QPalette &,
                                  const CodeViewTheme &code_view_theme) {
  QFont font(code_view_theme.font_name);
  setFont(font);
}

// NOTE(pag): `d->lock` is held in exclusive mode.
void MacroExplorer::UpdateList(void) {

  QLayoutItem *child = nullptr;
  while ((child = d->scroll_layout->takeAt(0)) != nullptr) {
    if (QWidget *widget = child->widget()) {
      widget->setParent(nullptr);
    }
    delete child;  // delete the layout item
  }

  for (auto item : d->ordered_items) {
    d->scroll_layout->addWidget(item);
  }

  d->scroll_layout->addStretch();
}

void MacroExplorer::AddMacro(RawEntityId macro_id, RawEntityId token_id) {
  VariantEntity macro_ent = d->index.entity(macro_id);
  VariantEntity token_ent = d->index.entity(token_id);
  if (!std::holds_alternative<Macro>(macro_ent) ||
      !std::holds_alternative<Token>(token_ent)) {
    return;
  }

  Macro macro = std::move(std::get<Macro>(macro_ent));
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
    } else if (auto def = DefineMacroDirective::from(containing_macro)) {
      if (def->name() == token) {
        AlwaysExpandMacro(def.value());
        return;
      }
    }
  }
}

MacroExplorer::~MacroExplorer(void) {}

MacroExplorer::MacroExplorer(const Index &index,
                             const FileLocationCache &file_location_cache,
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

  connect(&IThemeManager::Get(), &IThemeManager::ThemeChanged, this,
          &MacroExplorer::OnThemeChange);
}

}  // namespace mx::gui
