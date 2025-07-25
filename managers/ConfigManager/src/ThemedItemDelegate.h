/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Frontend/Token.h>
#include <multiplier/GUI/Interfaces/ITheme.h>

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QRect>
#include <QRectF>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTextOption>

namespace mx::gui {

class ThemedItemModel;

//! An item delegate used to paint tokens in the ReferenceExplorerView
class ThemedItemDelegate Q_DECL_FINAL : public QStyledItemDelegate {
  Q_OBJECT

 public:
  const IThemePtr theme;
  const QFont theme_font;
  const QFontMetricsF font_metrics;

  // Need to be computed with a `QPainter`. The `QFontMetricsF` generally tends
  // to under-approximate these.
  qreal line_height;
  qreal space_width;
  qreal tab_width;

  const QColor theme_foreground_color;
  const QColor theme_background_color;
  const QColor theme_highlight_color;

  std::unique_ptr<ThemedItemModel> model;

  // TODO(pag): Think about if this even makes sense, especially with respect
  //            to the model's `Qt::DisplayRole`, as that isn't necessarily
  //            subjet to whitespace replacement.
  const std::optional<std::string> whitespace_replacement;

  // Local state to help when printing stuff.
  mutable std::string token_data;
  mutable int num_printed_since_space{0};

  //! Constructor
  ThemedItemDelegate(
      IThemePtr theme_, const std::optional<std::string> &whitespace_replacement_,
      unsigned tab_width = 4u, QObject *parent = nullptr);

  //! Destructor
  virtual ~ThemedItemDelegate(void);

  //! Paints the delegate to screen
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const Q_DECL_FINAL;

  // //! Returns the size hint for the specified model index
  // QSize sizeHint(const QStyleOptionViewItem &option,
  //                const QModelIndex &index) const Q_DECL_FINAL;

  //! Triggered when the user tries to edit the QTreeView item
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) Q_DECL_FINAL;

  //! Paint a token. Specialized for a "measuring" painter.
  template <typename Painter>
  void PaintTokens(Painter *painter, const QStyleOptionViewItem &option,
                   TokenRange toks, const QColor &background_color,
                   const QColor &foreground_override) const;

  //! Paint a token. Specialized for a "measuring" painter.
  template <typename Painter>
  void PaintToken(Painter *painter, const QStyleOptionViewItem &option,
                  Token tok, const QColor &background_color,
                  const QColor &foreground_override, QPointF &pos_inout) const;

  //! Paint some text. Specialized for a "measuring" painter.
  template <typename Painter>
  void PaintText(
      Painter *painter, const QStyleOptionViewItem &option,
      const QString &text, const ITheme::ColorAndStyle &color_and_style,
      QPointF &pos_inout) const;

  //! Return the data of `tok`, but possibly adjusted for whitespace.
  std::string_view Characters(const Token &tok) const;

  //! Reset the internal state.
  void Reset(void) const {
    num_printed_since_space = 0;
    token_data.clear();
  }
};

}  // namespace mx::gui
