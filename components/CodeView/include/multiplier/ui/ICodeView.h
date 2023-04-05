/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/CodeViewTheme.h>
#include <multiplier/ui/ICodeModel.h>

#include <QWidget>

#include <optional>

namespace mx {
class Token;
class TokenRange;
}  // namespace mx

namespace mx::gui {

//! A code view widget that will display the contents of an ICodeModel model
class ICodeView : public QWidget {
  Q_OBJECT

 public:
  //! \brief Generic token action structure.
  //! Custom actions should be implemented in the event receiver
  struct TokenAction final {
    enum class Type {
      //! Bound to command+click
      Primary,

      //! Bound to right-click
      Secondary,

      //! When the mouse is hovering on a token
      Hover,

      //! Keyboard button, when the text cursor is on a token
      Keyboard,
    };

    //! Action type
    Type type{Type::Primary};

    //! Keyboard button data
    struct KeyboardButton final {
      //! See Qt::Key
      int key{};

      //! True if the shift modifier was held down
      bool shift_modifier{};

      //! \brief True if the control modifier was held down
      //! On macOS, this button is the command key
      bool control_modifier{};
    };

    //! Keyboard button, if applicable. See Qt::Key
    std::optional<KeyboardButton> opt_keyboard_button;
  };

  //! Factory function
  static ICodeView *Create(ICodeModel *model, QWidget *parent = nullptr);

  //! Returns the internal code model
  virtual ICodeModel *Model() = 0;

  //! Sets the specified theme, refreshing the view
  virtual void SetTheme(const CodeViewTheme &theme) = 0;

  //! Sets the specified tab stop distance, refreshing the view
  virtual void SetTabWidth(std::size_t width) = 0;

  //! Constructor
  ICodeView(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~ICodeView() override = default;

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
  virtual bool ScrollToLineNumber(unsigned line) const = 0;

  //! Disable the copy constructor
  ICodeView(const ICodeView &) = delete;

  //! Disable copy assignment
  ICodeView &operator=(const ICodeView &) = delete;

 signals:
  //! This signal is emitted when a token action is performed
  void TokenTriggered(const TokenAction &token_action,
                      const CodeModelIndex &index);

  //! This signal is emitted when the cursor position has changed.
  void CursorMoved(const CodeModelIndex &index);

  //! Emitted when the document is changed in response to a model reset
  void DocumentChanged();
};

}  // namespace mx::gui
