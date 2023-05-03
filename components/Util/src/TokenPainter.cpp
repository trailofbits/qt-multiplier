/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/TokenPainter.h>

#include <multiplier/Entities/TokenKind.h>
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QTextOption>
#include <string>

namespace mx::gui {
namespace {

QPointF GetRectPosition(const QRectF &rect) {
  return rect.topLeft();
}

#ifdef __APPLE__

QPointF GetRectPosition(const QRect &rect) {
  return GetRectPosition(rect.toRectF());
}

#endif

static QFont CreateFont(const CodeViewTheme &theme_) {
  QFont font(theme_.font_name);
  font.setStyleHint(QFont::TypeWriter);
  return font;
}

static std::string WhitespaceReplacement(const TokenPainterConfiguration &c) {
  if (c.whitespace_replacement) {
    return c.whitespace_replacement->toStdString();
  } else {
    return {};
  }
}

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

struct TokenPainter::PrivateData {
  TokenPainterConfiguration config;

  QFont font;
  QFontMetricsF font_metrics;
  const qreal line_height;
  const qreal space_width;
  const qreal tab_width;
  const std::string whitespace_replacement;

  // Local state to help when printing stuff.
  std::string token_data;
  int num_printed_since_space{0};

  explicit PrivateData(const TokenPainterConfiguration &config_);

  // Return the data of `tok`, but possibly adjusted for whitespace.
  std::string_view Characters(const Token &tok);

  // Paint a token. Specialized for a "measuring" painter.
  template <typename Painter>
  void PaintToken(Painter *painter, const QStyleOptionViewItem &option,
                  const Token &tok, QPointF &pos_inout);

  void Reset(void) {
    num_printed_since_space = 0;
    token_data.clear();
  }
};

TokenPainter::PrivateData::PrivateData(const TokenPainterConfiguration &config_)
    : config(config_),
      font(CreateFont(config.theme)),
      font_metrics(font),
      line_height(font_metrics.height()),
      space_width(font_metrics.horizontalAdvance(QChar::Space)),
      tab_width(space_width * static_cast<qreal>(config.tab_width)),
      whitespace_replacement(WhitespaceReplacement(config)) {}

//! Generate the characters of `data`. If `whitespace` has a value, then we
//! encode any sequence of whitespace into a single space token. Also in this
//! special mode, leading and trailing whitespace are not generated.
std::string_view TokenPainter::PrivateData::Characters(const Token &tok) {
  token_data.clear();
  auto token_chars = tok.data();

  if (config.whitespace_replacement.has_value()) {
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
            token_data.insert(token_data.end(), whitespace_replacement.begin(),
                              whitespace_replacement.end());
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

template <typename Painter>
void TokenPainter::PrivateData::PaintToken(Painter *painter,
                                           const QStyleOptionViewItem &option,
                                           const Token &token,
                                           QPointF &pos_inout) {

  TokenCategory token_category = token.category();
  painter->setPen(config.theme.ForegroundColor(token_category));

  CodeViewTheme::Style style = config.theme.TextStyle(token_category);

  font.setItalic(style.italic);
  font.setUnderline(style.underline);
  font.setStrikeOut(style.strikeout);
  font.setWeight(style.bold ? QFont::DemiBold : QFont::Normal);
  painter->setFont(font);

  QColor bg = config.theme.BackgroundColor(token_category);

  // Figure out a background color for if this node is selected.
  QColor highlight_color = config.theme.selected_line_background_color;
  if (!highlight_color.isValid() && option.widget) {
    highlight_color = option.widget->palette().highlight().color();
  }
  if (!highlight_color.isValid()) {
    highlight_color = qApp->palette().highlight().color();
  }

  QTextOption to(option.displayAlignment);

  std::string_view tok_data_utf8 = Characters(token);
  if (tok_data_utf8.empty()) {
    return;
  }

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

TokenPainter::~TokenPainter(void) {}

TokenPainter::TokenPainter(const TokenPainterConfiguration &config)
    : d(new PrivateData(config)) {}

TokenPainter &TokenPainter::operator=(TokenPainter &&that) noexcept {
  TokenPainter self(std::forward<TokenPainter>(that));
  d.swap(self.d);
  return *this;
}

TokenPainter::TokenPainter(TokenPainter &&that) noexcept
    : d(std::move(that.d)) {}

//! Paints the delegate to screen
void TokenPainter::Paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const TokenRange &tokens) const {
  d->Reset();

  QPointF pos = GetRectPosition(option.rect);
  painter->save();
  for (Token token : tokens) {
    d->PaintToken(painter, option, token, pos);
  }
  painter->restore();
}

//! Paints the delegate to screen
void TokenPainter::Paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const Token &token) const {
  d->Reset();

  QPointF pos = GetRectPosition(option.rect);
  painter->save();
  d->PaintToken(painter, option, token, pos);
  painter->restore();
}

//! Returns the size hint for the specified token range.
QSize TokenPainter::SizeHint(const QStyleOptionViewItem &option,
                             const TokenRange &tokens) const {
  d->Reset();

  QPointF pos = GetRectPosition(option.rect);
  QRectF empty_rect(pos.x(), pos.y(), d->space_width, d->line_height);
  MeasuringPainter painter(empty_rect);

  for (Token token : tokens) {
    d->PaintToken(&painter, option, token, pos);
  }

  return QSizeF(painter.area.width(), painter.area.height()).toSize();
}

//! Returns the size hint for the specified token
QSize TokenPainter::SizeHint(const QStyleOptionViewItem &option,
                             const Token &token) const {
  d->Reset();

  QPointF pos = GetRectPosition(option.rect);
  QRectF empty_rect(pos.x(), pos.y(), d->space_width, d->line_height);
  MeasuringPainter painter(empty_rect);

  d->PaintToken(&painter, option, token, pos);

  return QSizeF(painter.area.width(), painter.area.height()).toSize();
}

//! Given that we've painted `tokens` into a QRect, go and figure out what
//! token was clicked.
std::optional<Token>
TokenPainter::TokenAtPosition(const QRect &visual_rect, const QPoint &query_pos,
                              const TokenRange &tokens) const {
  if (!visual_rect.contains(query_pos)) {
    return std::nullopt;
  }

  d->Reset();

  QStyleOptionViewItem option;
  option.rect = visual_rect;

  QPointF query_pos_f(static_cast<qreal>(query_pos.x()),
                      static_cast<qreal>(query_pos.y()));
  QPointF pos = visual_rect.topLeft();
  QRectF empty_rect(pos.x(), pos.y(), d->space_width, d->line_height);
  MeasuringPainter painter(empty_rect);

  for (Token token : tokens) {
    d->PaintToken(&painter, option, token, pos);
    if (painter.area.contains(query_pos_f)) {
      return token;
    }
  }
  return std::nullopt;
}

//! Given that we've painted `tokens` into a QRect, go and figure out what
//! token was clicked.
std::optional<Token> TokenPainter::TokenAtPosition(const QRect &visual_rect,
                                                   const QPoint &query_pos,
                                                   const Token &token) const {

  if (!visual_rect.contains(query_pos)) {
    return std::nullopt;
  }

  d->Reset();

  QStyleOptionViewItem option;
  option.rect = visual_rect;

  QPointF query_pos_f(static_cast<qreal>(query_pos.x()),
                      static_cast<qreal>(query_pos.y()));
  QPointF pos = visual_rect.topLeft();
  QRectF empty_rect(pos.x(), pos.y(), d->space_width, d->line_height);
  MeasuringPainter painter(empty_rect);

  d->PaintToken(&painter, option, token, pos);
  if (!painter.area.contains(query_pos_f)) {
    return std::nullopt;
  }

  return token;
}

const TokenPainterConfiguration &TokenPainter::Configuration(void) const {
  return d->config;
}

}  // namespace mx::gui
