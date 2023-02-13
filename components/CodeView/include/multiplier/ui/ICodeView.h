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

//! A code view widget that will display the contents of an ICodeModel model
class ICodeView : public QWidget {
  Q_OBJECT

 public:
  //! Factory function
  static ICodeView *Create(ICodeModel *model, QWidget *parent = nullptr);

  //! Sets the specified theme, refreshing the view
  virtual void SetTheme(const CodeViewTheme &theme) = 0;

  //! Sets the specified tab stop distance, refreshing the view
  virtual void SetTabWidth(std::size_t width) = 0;

  //! Constructor
  ICodeView(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~ICodeView() override = default;

  //! Returns the cursor position list for the specified entity id
  virtual std::optional<int>
  GetEntityCursorPosition(RawEntityId entity_id) const = 0;

  //! Returns the current cursor position
  virtual int GetCursorPosition() const = 0;

  //! Sets the new cursor position, returning false in case of an error
  virtual bool
  SetCursorPosition(int start,
                    std::optional<int> opt_end = std::nullopt) const = 0;

  //! Returns the current code view contents in plain text format
  virtual QString Text() const = 0;

  //! Enables or disables word wrapping
  virtual void SetWordWrapping(bool enabled) = 0;

  //! Scrolls the view to the specified entity id
  virtual bool ScrollToEntityId(RawEntityId entity_id) const = 0;

  //! Scrolls the view to the specified token
  virtual bool ScrollToToken(const Token &token) const = 0;

  //! Scrolls the view to the start of the specified token range
  virtual bool ScrollToTokenRange(const TokenRange &token_range) const = 0;

  //! Disable the copy constructor
  ICodeView(const ICodeView &) = delete;

  //! Disable copy assignment
  ICodeView &operator=(const ICodeView &) = delete;

 signals:
  //! This signal is emitted when a token is clicked
  void TokenClicked(const CodeModelIndex &index,
                    const Qt::MouseButton &mouse_button,
                    const Qt::KeyboardModifiers &modifiers, bool double_click);

  //! This signal is emitted whenever the mouse cursor is hovering on a token
  void TokenHovered(const CodeModelIndex &index);
};

}  // namespace mx::gui
