// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/CallHierarchyPlugin.h>

#include <multiplier/GUI/ActionRegistry.h>
#include <multiplier/GUI/Context.h>
#include <multiplier/GUI/IModel.h>

#include "CallHierarchyGenerator.h"

namespace mx::gui {
namespace {

static const QKeySequence kKeySeqX("X");

static QString ActionName(const VariantEntity &) {
  return QObject::tr("Show Call Hierarchy");
}

}  // namespace

struct CallHierarchyPlugin::PrivateData {
  const Context &context;

  TriggerHandle popup_reference_explorer_trigger;

  inline PrivateData(const Context &context_)
      : context(context_),
        popup_reference_explorer_trigger(context.ActionRegistry().Find(
            "com.trailofbits.action.OpenReferenceExplorer")) {}
};

CallHierarchyPlugin::~CallHierarchyPlugin(void) {}

CallHierarchyPlugin::CallHierarchyPlugin(
    const Context &context, QObject *parent)
    : IReferenceExplorerPlugin(context, parent),
      d(new PrivateData(context)) {}

std::optional<NamedAction> CallHierarchyPlugin::ActOnMainWindowSecondaryClick(
    QMainWindow *, const QModelIndex &index) {

  VariantEntity entity = IModel::EntitySkipThroughTokens(index);
  if (std::holds_alternative<NotAnEntity>(entity)) {
    return std::nullopt;
  }

  return NamedAction{
    .name = ActionName(entity),
    .action = d->popup_reference_explorer_trigger,
    .data = QVariant::fromValue(CallHierarchyGenerator::Create(
        d->context.Index(),
        d->context.FileLocationCache(),
        std::move(entity)))
  };
}

// Allow a main window plugin to act on a key sequence.
std::optional<NamedAction> CallHierarchyPlugin::ActOnMainWindowKeyPress(
    QMainWindow *window, const QKeySequence &keys,
    const QModelIndex &index) {

  if (keys != kKeySeqX) {
    return std::nullopt;
  }

  return ActOnMainWindowSecondaryClick(window, index);
}

}  // namespace mx::gui
