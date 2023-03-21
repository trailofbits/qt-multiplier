/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "EntityExplorerItemDelegate.h"

#include <multiplier/ui/CodeViewTheme.h>
#include <multiplier/ui/IEntityExplorerModel.h>

#include <multiplier/Token.h>

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

#include <type_traits>

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
};

}  // namespace

struct EntityExplorerItemDelegate::PrivateData {
  CodeViewTheme theme;
  QFont font;
  QFontMetricsF font_metrics;
  std::optional<QString> whitespace;
  std::string token_data;

  int num_printed_since_space{0};
  qreal tab_width{4};

  static QFont CreateFont(const CodeViewTheme &theme_) {
    QFont font(theme_.font_name);
    font.setStyleHint(QFont::TypeWriter);
    return font;
  }

  inline PrivateData(const CodeViewTheme &theme_)
      : theme(theme_),
        font(CreateFont(theme)),
        font_metrics(font) {}

  // Return the data of `tok`, but possibly adjusted for whitespace.
  const std::string &Characters(const Token &tok);

  // Paint a token. Specialized for a "measuring" painter.
  template <typename Painter>
  void PaintToken(Painter *painter, const QStyleOptionViewItem &option,
                  const Token &tok, QPointF &pos_inout);
};

//! Generate the characters of `data`. If `whitespace` has a value, then we
//! encode any sequence of whitespace into a single space token. Also in this
//! special mode, leading and trailing whitespace are not generated.
const std::string &
EntityExplorerItemDelegate::PrivateData::Characters(const Token &tok) {
  token_data.clear();
  auto token_chars = tok.data();

  if (whitespace.has_value()) {
    for (char ch : token_chars) {
      switch (ch) {
        case ' ':
        case '\t':
        case '\n':
          if (num_printed_since_space) {
            token_data.push_back(' ');
            num_printed_since_space = 0;
          }
          continue;
        case '\r': continue;
        default:
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

template <typename Painter>
void EntityExplorerItemDelegate::PrivateData::PaintToken(
    Painter *painter, const QStyleOptionViewItem &option, const Token &token,
    QPointF &pos_inout) {

  num_printed_since_space = 0;  // Reset.

  TokenCategory token_category = token.category();
  painter->setPen(theme.ForegroundColor(token_category));

  CodeViewTheme::Style style = theme.TextStyle(token_category);

  font.setItalic(style.italic);
  font.setUnderline(style.underline);
  font.setStrikeOut(style.strikeout);
  font.setWeight(style.bold ? QFont::DemiBold : QFont::Normal);
  painter->setFont(font);

  QColor bg = theme.BackgroundColor(token_category);
  QTextOption to(option.displayAlignment);

  for (QChar ch : QString::fromStdString(Characters(token))) {
    QRectF glyph_rect(0.0, 0.0, font_metrics.horizontalAdvance(ch),
                      font_metrics.height());

    // NOTE(pag): Fixup prior to `->fillRect`, as the measuring painter will
    //            need to see how we calculate tab width.
    if (ch == QChar::Tabulation) {
      glyph_rect.setWidth(font_metrics.horizontalAdvance(QChar::Space) *
                          tab_width);
    }

    glyph_rect.moveTo(pos_inout);

    if ((option.state & QStyle::State_Selected) == 0) {
      painter->fillRect(glyph_rect, bg);
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

EntityExplorerItemDelegate::EntityExplorerItemDelegate(
    const CodeViewTheme &theme, QObject *parent)
    : QStyledItemDelegate(parent),
      d(new PrivateData(theme)) {}

EntityExplorerItemDelegate::~EntityExplorerItemDelegate(void) {}

void EntityExplorerItemDelegate::SetTheme(const CodeViewTheme &theme) {
  d->theme = theme;
  d->font = PrivateData::CreateFont(theme);
  d->font_metrics = QFontMetricsF(d->font);
}

void EntityExplorerItemDelegate::SetTabWidth(std::size_t width) {
  d->tab_width = static_cast<qreal>(width);
}

void EntityExplorerItemDelegate::SetWhitespaceReplacement(QString data) {
  ClearWhitespaceReplacement();
  d->whitespace = std::move(data);
}

void EntityExplorerItemDelegate::ClearWhitespaceReplacement(void) {
  d->whitespace.reset();
}

void EntityExplorerItemDelegate::paint(QPainter *painter,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const {
  if (!index.isValid()) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  QVariant val = index.data(IEntityExplorerModel::TokenRole);
  if (!val.isValid()) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  auto is_range = val.canConvert<TokenRange>();
  auto is_tok = val.canConvert<Token>();
  if (!is_range && !is_tok) {
    this->QStyledItemDelegate::paint(painter, option, index);
    return;
  }

  painter->save();

  // Make sure that the system selection highlight is honored
  if ((option.state & QStyle::State_Selected) != 0) {
    painter->fillRect(option.rect, option.palette.highlight());
  } else {
    painter->fillRect(option.rect, d->theme.default_background_color);
  }

  painter->setRenderHint(QPainter::Antialiasing, true);

  QPointF pos = option.rect.toRectF().topLeft();

  if (is_range) {
    for (Token tok : qvariant_cast<TokenRange>(val)) {
      d->PaintToken(painter, option, tok, pos);
    }

  } else {
    d->PaintToken(painter, option, qvariant_cast<Token>(val), pos);
  }

  painter->restore();
}

QSize EntityExplorerItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const {
  if (!index.isValid()) {
    return this->QStyledItemDelegate::sizeHint(option, index);
  }

  QVariant val = index.data(Qt::DisplayRole);
  if (!val.isValid()) {
    return this->QStyledItemDelegate::sizeHint(option, index);
  }

  auto is_range = val.canConvert<TokenRange>();
  auto is_tok = val.canConvert<Token>();
  if (!is_range && !is_tok) {
    return this->QStyledItemDelegate::sizeHint(option, index);
  }

  QPointF pos = option.rect.toRectF().topLeft();
  QRectF empty_rect(pos.x(), pos.y(),
                    d->font_metrics.horizontalAdvance(QChar::Space),
                    d->font_metrics.height());
  MeasuringPainter painter(empty_rect);

  if (is_range) {
    for (Token tok : qvariant_cast<TokenRange>(val)) {
      d->PaintToken(&painter, option, tok, pos);
    }
  } else {
    d->PaintToken(&painter, option, qvariant_cast<Token>(val), pos);
  }

  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  QSize contents_size =
      QSizeF(painter.area.width(), painter.area.height()).toSize();
  QStyle *style = opt.widget ? opt.widget->style() : qApp->style();
  return style->sizeFromContents(QStyle::ContentsType::CT_ItemViewItem, &opt,
                                 contents_size, opt.widget);
}

}  // namespace mx::gui
