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
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextOption>

#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/Index.h>

#include "ThemedItemModel.h"

namespace mx::gui {
namespace {

static const QString kSpace = " ";

class MeasuringPainter {
 public:
  QRectF area;

  inline MeasuringPainter(const QRectF &area_) : area(area_) {}

  inline void setPen(const QColor &) {}
  inline void setFont(const QFont &) {}

  inline void fillRect(const QRectF &rect, const QColor &) {
    area = area.united(rect);
  }

  inline void drawText(const QRectF &rect, const QString &, const QTextOption &) {
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

static const QTextOption kTextOption(Qt::AlignLeft);

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
      whitespace_replacement(whitespace_replacement_) {

  // Use a "dummy" painter to try to better estimate some baseline sizes.
  qreal max_char_width = font_metrics.maxWidth();
  QPixmap dummy_pixmap(static_cast<int>(max_char_width * 4),
                       static_cast<int>(line_height * 4));
  QPainter p(&dummy_pixmap);

  QFont bold_italic_font = theme_font;
  bold_italic_font.setWeight(QFont::DemiBold);
  bold_italic_font.setItalic(true);
  p.setFont(bold_italic_font);

  auto r = p.boundingRect(QRectF(max_char_width, line_height,
                                 max_char_width * 3, line_height * 3),
                          " ", kTextOption);
  
  space_width = std::max(space_width, std::ceil(r.width()));
  line_height = std::max(line_height, std::ceil(r.height()));
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
    TokenRange toks, const QColor &background_color,
    const QColor &foreground_color) const {
  Reset();
  QPointF pos = GetRectPosition(option.rect);
  for (auto token : toks) {
    PaintToken(painter, option, std::move(token), background_color,
               foreground_color, pos);
  }
}

template <typename Painter>
void ThemedItemDelegate::PaintToken(
    Painter *painter, const QStyleOptionViewItem &option,
    Token token, const QColor &background_color,
    const QColor &foreground_color, QPointF &pos_inout) const {

  std::string_view tok_data_utf8 = Characters(token);
  QString tok_data = QString::fromUtf8(
      tok_data_utf8.data(), static_cast<qsizetype>(tok_data_utf8.size()));

  auto color_and_style = theme->TokenColorAndStyle(token);
  if (option.state & QStyle::State_Selected) {
    color_and_style.foreground_color = foreground_color;
    color_and_style.background_color = background_color;
  }

  if (!color_and_style.foreground_color.isValid()) {
    color_and_style.foreground_color = theme_foreground_color;
  }

  if (!color_and_style.background_color.isValid()) {
    color_and_style.background_color = theme_background_color;
  }

  PaintText(painter, option, tok_data, color_and_style, pos_inout);
}

//! Paint some text. Specialized for a "measuring" painter.
template <typename Painter>
void ThemedItemDelegate::PaintText(
    Painter *painter, const QStyleOptionViewItem &option,
    const QString &tok_data, const ITheme::ColorAndStyle &color_and_style,
    QPointF &pos_inout) const {

  if (tok_data.isEmpty()) {
    return;
  }

  painter->setPen(color_and_style.foreground_color);

  auto font = theme_font;
  font.setItalic(color_and_style.italic);
  font.setUnderline(color_and_style.underline);
  font.setStrikeOut(color_and_style.strikeout);
  font.setWeight(color_and_style.bold ? QFont::DemiBold : QFont::Normal);
  painter->setFont(font);

  QTextOption to(option.displayAlignment);
  QRectF baseline_rect(0, 0, 2 * font_metrics.maxWidth() * tok_data.size(),
                       2 * font_metrics.height());
  auto base_rect = painter->boundingRect(baseline_rect, kSpace);
  auto rect = painter->boundingRect(baseline_rect, tok_data);

  rect.moveTo(pos_inout);
  painter->fillRect(rect, color_and_style.background_color);
  painter->drawText(rect, tok_data, to);

  pos_inout = rect.bottomRight() - QPointF(0, base_rect.height());

  // for (QChar ch : tok_data) {
  //   QRectF glyph_rect(0.0, 0.0, font_metrics.horizontalAdvance(ch),
  //                     font_metrics.height());

  //   // NOTE(pag): Fixup prior to `->fillRect`, as the measuring painter will
  //   //            need to see how we calculate tab width.
  //   if (ch == QChar::Tabulation) {
  //     glyph_rect.setWidth(tab_width);
  //   }

  //   glyph_rect.moveTo(pos_inout);

  //   painter->fillRect(glyph_rect, color_and_style.background_color);

  //   switch (ch.unicode()) {
  //     case QChar::Tabulation:
  //     case QChar::Space:
  //     case QChar::Nbsp: pos_inout.setX(glyph_rect.right()); break;
  //     case QChar::ParagraphSeparator:
  //     case QChar::LineFeed:
  //     case QChar::LineSeparator:
  //       pos_inout.setX(option.rect.x());
  //       pos_inout.setY(glyph_rect.bottom());
  //       break;
  //     case QChar::CarriageReturn: continue;
  //     default:
  //       painter->drawText(glyph_rect, ch, to);
  //       pos_inout.setX(glyph_rect.right());
  //       break;
  //   }
  // }
}

void ThemedItemDelegate::paint(QPainter *painter,
                               const QStyleOptionViewItem &option,
                               const QModelIndex &index) const {

  ITheme::ColorAndStyle cs = {};
  cs.foreground_color = theme_foreground_color;
  cs.background_color = theme_background_color;
  
  // Highlighted background color.
  if ((option.state & QStyle::State_Selected) &&
      option.showDecorationSelected) {
    cs.background_color = theme_highlight_color;
    cs.foreground_color = ITheme::ContrastingColor(cs.background_color);

  // Entity-specific background, apply to the whole QItem.
  } else if (auto entity_bg
                 = theme->EntityBackgroundColor(IModel::EntitySkipThroughTokens(index))) {

    cs.background_color = entity_bg.value();
    cs.foreground_color = ITheme::ContrastingColor(cs.background_color);
  }
  
  painter->save();
  painter->fillRect(option.rect, cs.background_color);

  if (TokenRange tokens = IModel::TokensToDisplay(index)) {
    PaintTokens(painter, option, std::move(tokens), cs.background_color,
                cs.foreground_color);
  } else {
    QPointF pos = GetRectPosition(option.rect);
    PaintText(painter, option, index.data(Qt::DisplayRole).toString(),
              cs, pos);
  }
  painter->restore();
}

// QSize ThemedItemDelegate::sizeHint(
//     const QStyleOptionViewItem &option, const QModelIndex &index) const {

//   QStyleOptionViewItem opt(option);
//   initStyleOption(&opt, index);
//   QStyle *style = opt.widget ? opt.widget->style() : qApp->style();

//   QPointF pos = GetRectPosition(option.rect);
//   QRectF empty_rect(pos.x(), pos.y(), space_width, line_height);
//   MeasuringPainter painter(empty_rect);

//   QColor dummy_color;
//   if (TokenRange tokens = IModel::TokensToDisplay(index)) {
//     PaintTokens(&painter, option, std::move(tokens), dummy_color, dummy_color);
//   } else {
//     PaintText(&painter, option, index.data(Qt::DisplayRole).toString(),
//               {}, pos);
//   }

//   return style
//       ->sizeFromContents(
//           QStyle::ContentsType::CT_ItemViewItem,
//           &opt,
//           QSizeF(painter.area.width(), painter.area.height()).toSize(),
//           opt.widget)
//       .grownBy(QMargins(0, 0, static_cast<int>(space_width), 0));
// }

bool ThemedItemDelegate::editorEvent(QEvent *, QAbstractItemModel *,
                                     const QStyleOptionViewItem &,
                                     const QModelIndex &) {
  return false;
}

}  // namespace mx::gui
