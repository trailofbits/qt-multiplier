/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TokenLabel.h"

#include <multiplier/GUI/TokenPainter.h>

namespace mx::gui {

struct TokenLabel::PrivateData final {
  std::unique_ptr<TokenPainter> token_painter;
  TokenRange tokens;
};

TokenLabel::~TokenLabel() {}

void TokenLabel::paintEvent(QPaintEvent *) {
  QStyleOptionViewItem option;
  option.widget = this;
  option.rect = rect();

  QPainter painter(this);
  d->token_painter->Paint(&painter, option, d->tokens);
}

TokenLabel::TokenLabel(TokenRange tokens, QWidget *parent)
    : ITokenLabel(parent),
      d(new PrivateData) {

  d->tokens = std::move(tokens);

  connect(&ThemeManager::Get(), &ThemeManager::ThemeChanged, this,
          &TokenLabel::OnThemeChange);

  OnThemeChange(palette(), ThemeManager::Get().GetCodeViewTheme());
}


void TokenLabel::OnThemeChange(const QPalette &,
                               const CodeViewTheme &code_view_theme) {

  TokenPainterConfiguration painter_config(code_view_theme);
  d->token_painter = std::make_unique<TokenPainter>(painter_config);
}

}  // namespace mx::gui
