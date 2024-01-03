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

#include "ThemedItemModel.h"

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

ThemedItemDelegate::~ThemedItemDelegate(void) {}

ThemedItemDelegate::ThemedItemDelegate(
    IThemePtr theme_, const std::optional<std::string> &whitespace_replacement_,
    unsigned tab_width, QObject *parent)
    : QStyledItemDelegate(parent),
      theme(std::move(theme_)),
      theme_font(theme->Font()),
      font_metrics(theme_font),
      line_height(font_metrics.height()),
      space_width(font_metrics.horizontalAdvance(QChar::Space)),
      tab_width(space_width * static_cast<qreal>(tab_width)),
      theme_foreground_color(theme->DefaultForegroundColor()),
      theme_background_color(theme->DefaultBackgroundColor()),
      theme_highlight_color(theme->CurrentLineBackgroundColor()),
      model(new ThemedItemModel(this)),
      whitespace_replacement(whitespace_replacement_) {}

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
    TokenRange toks, const QColor &foreground_override) const {
  Reset();
  QPointF pos = GetRectPosition(option.rect);
  for (auto token : toks) {
    PaintToken(painter, option, std::move(token), foreground_override, pos);
  }
}

template <typename Painter>
void ThemedItemDelegate::PaintToken(
    Painter *painter, const QStyleOptionViewItem &option,
    Token token, const QColor &foreground_override, QPointF &pos_inout) const {

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

  // Possibly override the foreground color.
  if (foreground_override.isValid() &&
      color_and_style.foreground_color == theme_foreground_color) {
    color_and_style.foreground_color = foreground_override;
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
  auto is_selected = (option.state & QStyle::State_Selected) &&
                     option.showDecorationSelected;

  QColor background_color;
  QColor foreground_color;
  
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
    foreground_color = ITheme::ContrastingColor(background_color);

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
  
  if (TokenRange tokens = IModel::TokensToDisplay(index)) {
    painter->save();
    painter->fillRect(option.rect, background_color);
    PaintTokens(painter, option, std::move(tokens), foreground_color);
    painter->restore();
    return;
  }

  // This is pretty evil. The idea is that we want to inject our own model in
  // front of `index`s model to enforce our own coloring behavior.
  auto indexed_model = const_cast<QAbstractItemModel *>(index.model());
  if (indexed_model != model->sourceModel()) {
    model->setSourceModel(indexed_model);
  }

  if (!foreground_color.isValid()) {
    foreground_color = ITheme::ContrastingColor(background_color);
  }

  model->background_color = background_color;
  model->foreground_color = foreground_color;

  QStyleOptionViewItem opt = option;
  opt.index = model->mapFromSource(index);
  opt.backgroundBrush = background_color;

  // // Force our color in the case of the highlighting too.
  if (opt.showDecorationSelected) {
    opt.palette.setColor(QPalette::Inactive, QPalette::HighlightedText, foreground_color);
    opt.palette.setColor(QPalette::Active, QPalette::HighlightedText, foreground_color);
    opt.palette.setColor(QPalette::Normal, QPalette::HighlightedText, foreground_color);

    opt.palette.setColor(QPalette::Inactive, QPalette::Highlight, background_color);
    opt.palette.setColor(QPalette::Active, QPalette::Highlight, background_color);
    opt.palette.setColor(QPalette::Normal, QPalette::Highlight, background_color);
  }

  this->QStyledItemDelegate::paint(painter, opt, opt.index);
}

QSize ThemedItemDelegate::sizeHint(
    const QStyleOptionViewItem &option, const QModelIndex &index) const {
  TokenRange tokens = IModel::TokensToDisplay(index);
  if (!tokens) {
    return this->QStyledItemDelegate::sizeHint(option, index);
  }

  QStyleOptionViewItem opt(option);
  initStyleOption(&opt, index);
  QStyle *style = opt.widget ? opt.widget->style() : qApp->style();

  QPointF pos = GetRectPosition(option.rect);
  QRectF empty_rect(pos.x(), pos.y(), space_width, line_height);
  MeasuringPainter painter(empty_rect);

  QColor dummy_foreground;
  PaintTokens(&painter, option, std::move(tokens), dummy_foreground);
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
