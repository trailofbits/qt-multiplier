/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/CodeViewTheme.h>

#include <memory>
#include <multiplier/Token.h>
#include <optional>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyleOptionViewItem>

namespace mx::gui {

struct TokenPainterConfiguration {
  CodeViewTheme theme;

  std::optional<QString> whitespace_replacement;

  std::size_t tab_width{4};

  inline TokenPainterConfiguration(const CodeViewTheme &theme_)
      : theme(theme_) {}
};

class TokenPainter {
 public:
  ~TokenPainter(void);

  explicit TokenPainter(const TokenPainterConfiguration &config);

  TokenPainter &operator=(TokenPainter &&) noexcept;
  TokenPainter(TokenPainter &&) noexcept;

  //! Paints the delegate to screen
  void Paint(QPainter *painter, const QStyleOptionViewItem &option,
             const TokenRange &tokens) const;

  //! Paints the delegate to screen
  void Paint(QPainter *painter, const QStyleOptionViewItem &option,
             const Token &token) const;

  //! Returns the size hint for the specified token range.
  QSize SizeHint(const QStyleOptionViewItem &option,
                 const TokenRange &tokens) const;

  //! Returns the size hint for the specified token
  QSize SizeHint(const QStyleOptionViewItem &option,
                 const Token &token) const;

  //! Given that we've painted `tokens` into a QRect, go and figure out what
  //! token was clicked.
  std::optional<Token> TokenAtPosition(const QRect &rect, const QPoint &pos,
                                       const TokenRange &tokens) const;

  //! Given that we've painted `token` into a QRect, go and figure out what
  //! token was clicked.
  std::optional<Token> TokenAtPosition(const QRect &rect, const QPoint &pos,
                                       const Token &token) const;

  const TokenPainterConfiguration &Configuration(void) const;

 private:
  TokenPainter(void) = delete;

  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
