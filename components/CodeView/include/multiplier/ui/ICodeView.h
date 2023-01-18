/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/CodeViewTheme.h>
#include <multiplier/ui/ICodeModel.h>

#include <optional>

#include <QWidget>

namespace mx {
class Token;
class TokenRange;
}  // namespace mx
namespace mx::gui {

class ICodeView : public QWidget {
  Q_OBJECT

 public:
  static ICodeView *Create(ICodeModel *model, QWidget *parent = nullptr);
  virtual void setTheme(const CodeViewTheme &theme) = 0;

  ICodeView(QWidget *parent) : QWidget(parent) {}
  virtual ~ICodeView() override = default;

  virtual std::optional<int>
  GetFileTokenCursorPosition(RawEntityId file_token_id) const = 0;

  virtual std::optional<int>
  GetTokenCursorPosition(const Token &token) const = 0;

  virtual std::optional<int>
  GetStartTokenRangeCursorPosition(const TokenRange &token_range) const = 0;

  virtual int GetCursorPosition() const = 0;
  virtual bool
  SetCursorPosition(int start,
                    std::optional<int> opt_end = std::nullopt) const = 0;

  virtual QString Text() const = 0;
  virtual void SetWordWrapping(bool enabled) = 0;

  virtual bool ScrollToFileToken(RawEntityId file_token_id) const = 0;
  virtual bool ScrollToToken(const Token &token) const = 0;
  virtual bool ScrollToTokenRange(const TokenRange &token_range) const = 0;

  ICodeView(const ICodeView &) = delete;
  ICodeView &operator=(const ICodeView &) = delete;

 signals:
  void TokenClicked(const CodeModelIndex &index, Qt::MouseButtons mouse_buttons,
                    bool double_click);

  void TokenHovered(const CodeModelIndex &index);
};

}  // namespace mx::gui
