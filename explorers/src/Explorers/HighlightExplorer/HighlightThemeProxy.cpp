// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "HighlightThemeProxy.h"

#include <multiplier/Frontend/Token.h>
#include <multiplier/Index.h>

namespace mx::gui {

HighlightThemeProxy::~HighlightThemeProxy(void) {}

ITheme::ColorAndStyle HighlightThemeProxy::TokenColorAndStyle(
    const ITheme &, ITheme::ColorAndStyle cs, const Token &token) const {

  auto eid = token.related_entity_id().Pack();
  if (auto color_it = color_map.find(eid); color_it != color_map.end()) {
    cs.foreground_color = color_it->second.first;
    cs.background_color = color_it->second.second;
  }
  return cs;
}

std::optional<QColor> HighlightThemeProxy::EntityBackgroundColor(
    const ITheme &, std::optional<QColor> theme_color,
    const VariantEntity &entity) const {
  auto eid = EntityId(entity).Pack();
  if (auto color_it = color_map.find(eid); color_it != color_map.end()) {
    return color_it->second.second;
  }
  return theme_color;
}

void HighlightThemeProxy::SendUpdate(void) {
  emit ThemeProxyChanged();
}

}  // namespace mx::gui
