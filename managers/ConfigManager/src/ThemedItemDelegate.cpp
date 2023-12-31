/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ThemedItemDelegate.h"

#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QStyle>
#include <QStyleOptionViewItem>

#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/Index.h>

namespace mx::gui {
namespace {

class MeasuringPainter {
 public:
  QRectF area;

  inline MeasuringPainter(const QRectF &area_) : area(area_) {}

  inline void setPen(const QColor &) {}
  inline void setFont(const QFont &) {}

  inline void fillRect(const QRectF &rect, const QColor &) {
    area = area.united(rect);
  }

  inline void drawText(const QRectF &rect, QChar, const QTextOption &) {
    area = area.united(rect);
  }

  inline void save(void) {}
  inline void restore(void) {}
};

inline static QPointF GetRectPosition(const QRectF &rect) {
  return rect.topLeft();
}

#ifdef __APPLE__
inline static QPointF GetRectPosition(const QRect &rect) {
  return GetRectPosition(rect.toRectF());
}
#endif

inline static QColor PaletteBackgroundColor(const QPalette &palette) {
  return palette.color(QPalette::Normal, QPalette::Window);
}

}  // namespace

ThemedItemDelegate::~ThemedItemDelegate(void) {
  if (prev_delegate) {
    const_cast<QAbstractItemDelegate *>(prev_delegate)->deleteLater();
  }
}

//! Generate the characters of `data`. If `whitespace` has a value, then we
//! encode any sequence of whitespace into a single space token. Also in this
//! special mode, leading and trailing whitespace are not generated.
std::string_view ThemedItemDelegate::Characters(const Token &tok) const {
  token_data.clear();
  auto token_chars = tok.data();

  if (whitespace_replacement.has_value()) {
    for (char ch : token_chars) {
      switch (ch) {
        case '\\':
          if (tok.kind() != TokenKind::WHITESPACE) {
            goto non_whitespace;
          }
          [[clang::fallthrough]];
        case ' ':
        case '\t':
        case '\n':
          if (num_printed_since_space) {
            token_data.insert(token_data.end(), whitespace_replacement->begin(),
                              whitespace_replacement->end());
            num_printed_since_space = 0;
          }
          continue;
        case '\r': continue;
        default:
        non_whitespace:
          num_printed_since_space += 1;
          token_data.push_back(ch);
          continue;
      }
    }
  } else {
    token_data.insert(token_data.end(), token_chars.begin(), token_chars.end());
  }

  return token_data;
}

//! Paint a token. Specialized for a "measuring" painter.
template <typename Painter>
void ThemedItemDelegate::PaintTokens(
    Painter *painter, const QStyleOptionViewItem &option,
    TokenRange toks) const {
  Reset();
  QPointF pos = GetRectPosition(option.rect);
  painter->save();
  for (auto token : toks) {
    PaintToken(painter, option, std::move(token), pos);
  }
  painter->restore();
}

template <typename Painter>
void ThemedItemDelegate::PaintToken(Painter *painter,
                                    const QStyleOptionViewItem &option,
                                    Token token,
                                    QPointF &pos_inout) const {

  std::string_view tok_data_utf8 = Characters(token);
  if (tok_data_utf8.empty()) {
    return;
  }

  auto color_and_style = theme->TokenColorAndStyle(token);
  painter->setPen(color_and_style.foreground_color);

  auto font = theme_font;
  font.setItalic(color_and_style.italic);
  font.setUnderline(color_and_style.underline);
  font.setStrikeOut(color_and_style.strikeout);
  font.setWeight(color_and_style.bold ? QFont::DemiBold : QFont::Normal);
  painter->setFont(font);

  // Figure out a background color for this token if it is selected.
  auto highlight_color = color_and_style.background_color;
  if (color_and_style.background_color == theme_background_color) {
    highlight_color = theme_highlight_color;
    if (!highlight_color.isValid() && option.widget) {
      highlight_color = option.widget->palette().highlight().color();
    }
    if (!highlight_color.isValid()) {
      highlight_color = qApp->palette().highlight().color();
    }
  }

  QTextOption to(option.displayAlignment);

  QString tok_data = QString::fromUtf8(
      tok_data_utf8.data(), static_cast<qsizetype>(tok_data_utf8.size()));
  for (QChar ch : tok_data) {
    QRectF glyph_rect(0.0, 0.0, font_metrics.horizontalAdvance(ch),
                      font_metrics.height());

    // NOTE(pag): Fixup prior to `->fillRect`, as the measuring painter will
    //            need to see how we calculate tab width.
    if (ch == QChar::Tabulation) {
      glyph_rect.setWidth(tab_width);
    }

    glyph_rect.moveTo(pos_inout);

    if ((option.state & QStyle::State_Selected) == QStyle::State_Selected) {
      painter->fillRect(glyph_rect, highlight_color);
    } else {
      painter->fillRect(glyph_rect, color_and_style.background_color);
    }

    switch (ch.unicode()) {
      case QChar::Tabulation:
      case QChar::Space:
      case QChar::Nbsp: pos_inout.setX(glyph_rect.right()); break;
      case QChar::ParagraphSeparator:
      case QChar::LineFeed:
      case QChar::LineSeparator:
        pos_inout.setX(option.rect.x());
        pos_inout.setY(glyph_rect.bottom());
        break;
      case QChar::CarriageReturn: continue;
      default:
        painter->drawText(glyph_rect, ch, to);
        pos_inout.setX(glyph_rect.right());
        break;
    }
  }
}

void ThemedItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {
  TokenRange tokens = IModel::TokensToDisplay(index);
  if (!tokens) {
    if (prev_delegate) {
      prev_delegate->paint(painter, option, index);
    } else {
      this->QStyledItemDelegate::paint(painter, option, index);
    }
    return;
  }

  auto is_selected = (option.state & QStyle::State_Selected) != 0;

  QColor background_color;
  
  // Highlighted background color.
  if (is_selected) {
    background_color = theme_highlight_color;

    if (!background_color.isValid() && option.widget) {
      background_color = option.widget->palette().highlight().color();
    }

    if (!background_color.isValid()) {
      background_color = qApp->palette().highlight().color();
    }

  // Entity-specific background, apply to the whole QItem.
  } else if (auto entity_bg
                 = theme->EntityBackgroundColor(IModel::Entity(index))) {
    background_color = entity_bg.value();

  // Normal background color.
  } else {
    background_color = theme_background_color;

    if (!background_color.isValid() && option.widget) {
      background_color = PaletteBackgroundColor(option.widget->palette());
    }

    if (!background_color.isValid()) {
      background_color = PaletteBackgroundColor(qApp->palette());
    }
  }

  painter->fillRect(option.rect, QBrush(background_color));
  
  PaintTokens(painter, option, std::move(tokens));

  // The highlight color used by the theme is barely visible, force better
  // highlighting using the standard highlight color to draw a frame
  // around the item
  //
  // TODO(pag): Might be that the border we print here doesn't get "unprinted".
  if (is_selected) {
    auto original_pen = painter->pen();
    painter->setPen(QPen(option.palette.highlight().color()));
    painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
    painter->setPen(original_pen);
  }
}

QSize ThemedItemDelegate::sizeHint(
    const QStyleOptionViewItem &option, const QModelIndex &index) const {
  TokenRange tokens = IModel::TokensToDisplay(index);
  if (!tokens) {
    if (prev_delegate) {
      return prev_delegate->sizeHint(option, index);
    } else {
      return this->QStyledItemDelegate::sizeHint(option, index);
    }
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  QStyle *style = opt.widget ? opt.widget->style() : qApp->style();

  QPointF pos = GetRectPosition(option.rect);
  QRectF empty_rect(pos.x(), pos.y(), space_width, line_height);
  MeasuringPainter painter(empty_rect);

  PaintTokens(&painter, option, std::move(tokens));
  return style
      ->sizeFromContents(
          QStyle::ContentsType::CT_ItemViewItem,
          &opt,
          QSizeF(painter.area.width(), painter.area.height()).toSize(),
          opt.widget)
      .grownBy(QMargins(0, 0, static_cast<int>(space_width), 0));
}

bool ThemedItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                     const QStyleOptionViewItem &,
                                     const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
