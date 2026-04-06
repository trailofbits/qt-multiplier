// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetDelegate.h>

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointF>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextOption>

#include <multiplier/Frontend/Token.h>
#include <multiplier/Frontend/TokenKind.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

Q_DECLARE_METATYPE(mx::Token)
Q_DECLARE_METATYPE(mx::TokenRange)

namespace mx::gui {

SpreadsheetDelegate::SpreadsheetDelegate(IThemePtr theme_, unsigned tab_width_,
                                         QObject *parent)
    : QStyledItemDelegate(parent),
      theme(std::move(theme_)),
      tab_width(tab_width_) {}

SpreadsheetDelegate::~SpreadsheetDelegate(void) {}

void SpreadsheetDelegate::SetTheme(IThemePtr new_theme) {
  theme = std::move(new_theme);
}

void SpreadsheetDelegate::SetTabWidth(unsigned tw) {
  tab_width = tw;
}

void SpreadsheetDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  QVariant raw = index.data(SpreadsheetRoles::RawValueRole);

  // Token cells: render each token with its syntax-highlighted color.
  if (raw.canConvert<TokenRange>() && theme) {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    auto *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter,
                         opt.widget);

    // Check for a model-provided background color.
    QVariant bg_var = index.data(Qt::BackgroundRole);
    if (bg_var.isValid() && bg_var.canConvert<QColor>()) {
      painter->fillRect(opt.rect, bg_var.value<QColor>());
    }

    auto range = raw.value<TokenRange>();

    // Collect tokens, stripping trailing whitespace-only tokens.
    std::vector<Token> tokens;
    for (Token tok : range) {
      tokens.push_back(std::move(tok));
    }
    while (!tokens.empty() &&
           tokens.back().kind() == TokenKind::WHITESPACE) {
      tokens.pop_back();
    }

    QFont font = theme->Font();
    QFontMetricsF fm(font);

    bool selected = (opt.state & QStyle::State_Selected);

    // Clip to cell rect to prevent painting over adjacent cells.
    painter->save();
    painter->setClipRect(opt.rect);

    QPointF pos(opt.rect.left() + 2, opt.rect.top() + fm.ascent());

    for (const auto &tok : tokens) {
      auto cs = theme->TokenColorAndStyle(tok);

      if (!cs.foreground_color.isValid()) {
        cs.foreground_color = theme->DefaultForegroundColor();
      }

      if (selected) {
        cs.foreground_color = opt.palette.color(QPalette::HighlightedText);
      }

      font.setBold(cs.bold);
      font.setItalic(cs.italic);
      font.setUnderline(cs.underline);
      painter->setFont(font);
      painter->setPen(cs.foreground_color);
      fm = QFontMetricsF(font);

      auto tok_data = tok.data();
      QString text = QString::fromUtf8(
          tok_data.data(), static_cast<qsizetype>(tok_data.size()));

      if (tok.kind() == TokenKind::WHITESPACE) {
        // Process whitespace for layout without drawing.
        for (auto ch : text) {
          if (ch == QLatin1Char('\n')) {
            pos.setX(opt.rect.left() + 2);
            pos.setY(pos.y() + fm.height());
          } else if (ch == QLatin1Char('\t')) {
            auto sr = painter->boundingRect(
                QRectF(0, 0, 9999, fm.height()),
                Qt::AlignLeft, QStringLiteral(" "));
            pos.setX(pos.x() + sr.width() *
                     static_cast<qreal>(tab_width));
          } else if (ch == QLatin1Char(' ')) {
            auto sr = painter->boundingRect(
                QRectF(0, 0, 9999, fm.height()),
                Qt::AlignLeft, QStringLiteral(" "));
            pos.setX(pos.x() + sr.width());
          }
        }
      } else {
        // Draw token text, handling embedded newlines by splitting
        // into segments and drawing each one.
        qsizetype start = 0;
        qsizetype len = text.size();
        for (qsizetype i = 0; i <= len; ++i) {
          bool is_end = (i == len);
          bool is_newline = (!is_end &&
                             (text[i] == QLatin1Char('\n') ||
                              text[i] == QLatin1Char('\r')));
          if (is_end || is_newline) {
            if (i > start) {
              QString segment = text.mid(start, i - start);
              painter->drawText(pos, segment);
              auto br = painter->boundingRect(
                  QRectF(0, 0, 9999, fm.height()),
                  Qt::AlignLeft, segment);
              pos.setX(pos.x() + br.width());
            }
            if (is_newline) {
              pos.setX(opt.rect.left() + 2);
              pos.setY(pos.y() + fm.height());
              // Skip \r\n as a pair.
              if (text[i] == QLatin1Char('\r') && i + 1 < len &&
                  text[i + 1] == QLatin1Char('\n')) {
                ++i;
              }
            }
            start = i + 1;
          }
        }
      }
    }
    painter->restore();
    return;
  }

  // Single Token: same treatment.
  if (raw.canConvert<Token>() && theme) {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    auto *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter,
                         opt.widget);

    auto tok = raw.value<Token>();
    auto cs = theme->TokenColorAndStyle(tok);
    if (!cs.foreground_color.isValid()) {
      cs.foreground_color = theme->DefaultForegroundColor();
    }

    QFont font = theme->Font();
    font.setBold(cs.bold);
    font.setItalic(cs.italic);
    painter->save();
    painter->setFont(font);
    painter->setPen(cs.foreground_color);
    QRect text_rect = style->subElementRect(
        QStyle::SE_ItemViewItemText, &opt, opt.widget);
    auto tok_data = tok.data();
    painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter,
                      QString::fromUtf8(tok_data.data(),
                                        static_cast<qsizetype>(tok_data.size())));
    painter->restore();
    return;
  }

  // FormulaCell: render the cached result (or error) text.
  if (raw.canConvert<FormulaCell>()) {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    auto fc = raw.value<FormulaCell>();
    if (!fc.error_message.isEmpty()) {
      opt.palette.setColor(QPalette::Text, QColor(Qt::red));
      opt.palette.setColor(QPalette::HighlightedText, QColor(Qt::red));
    }

    QStyledItemDelegate::paint(painter, opt, index);
    return;
  }

  // Bool cells: draw a centered checkbox.
  if (raw.userType() == QMetaType::Bool) {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    auto *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter,
                         opt.widget);

    QStyleOptionButton check_opt;
    check_opt.state = QStyle::State_Enabled;
    if (raw.toBool()) {
      check_opt.state |= QStyle::State_On;
    } else {
      check_opt.state |= QStyle::State_Off;
    }

    // Center the checkbox in the cell.
    QRect check_rect = style->subElementRect(
        QStyle::SE_CheckBoxIndicator, &check_opt, opt.widget);
    int x = opt.rect.x() + (opt.rect.width() - check_rect.width()) / 2;
    int y = opt.rect.y() + (opt.rect.height() - check_rect.height()) / 2;
    check_opt.rect = QRect(QPoint(x, y), check_rect.size());

    style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check_opt,
                         painter, opt.widget);
    return;
  }

  // QString: fall through to the default delegate.
  QStyledItemDelegate::paint(painter, option, index);
}

QSize SpreadsheetDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
  QVariant raw = index.data(SpreadsheetRoles::RawValueRole);

  if ((raw.canConvert<TokenRange>() || raw.canConvert<Token>()) && theme) {
    QFont font = theme->Font();
    QFontMetricsF fm(font);

    // Use a dummy painter for accurate boundingRect measurements.
    QPixmap dummy(1, 1);
    QPainter p(&dummy);
    p.setFont(font);

    qreal width = 0;
    qreal x = 0;
    int lines = 1;

    if (raw.canConvert<TokenRange>()) {
      std::vector<Token> tokens;
      for (Token tok : raw.value<TokenRange>()) {
        tokens.push_back(std::move(tok));
      }
      while (!tokens.empty() &&
             tokens.back().kind() == TokenKind::WHITESPACE) {
        tokens.pop_back();
      }

      for (const auto &tok : tokens) {
        auto tok_data = tok.data();
        QString text = QString::fromUtf8(
            tok_data.data(), static_cast<qsizetype>(tok_data.size()));

        if (tok.kind() == TokenKind::WHITESPACE) {
          for (auto ch : text) {
            if (ch == QLatin1Char('\n')) {
              width = std::max(width, x);
              x = 0;
              ++lines;
            } else if (ch == QLatin1Char('\t')) {
              auto space_rect = p.boundingRect(
                  QRectF(0, 0, 9999, fm.height()),
                  Qt::AlignLeft, QStringLiteral(" "));
              x += space_rect.width() * static_cast<qreal>(tab_width);
            } else {
              auto ch_rect = p.boundingRect(
                  QRectF(0, 0, 9999, fm.height()),
                  Qt::AlignLeft, QString(ch));
              x += ch_rect.width();
            }
          }
        } else {
          auto cs = theme->TokenColorAndStyle(tok);
          font.setBold(cs.bold);
          font.setItalic(cs.italic);
          p.setFont(font);
          // Measure each line segment of the token.
          qsizetype start = 0;
          qsizetype tlen = text.size();
          for (qsizetype i = 0; i <= tlen; ++i) {
            bool is_end = (i == tlen);
            bool is_nl = (!is_end && (text[i] == QLatin1Char('\n') ||
                                      text[i] == QLatin1Char('\r')));
            if (is_end || is_nl) {
              if (i > start) {
                auto rect = p.boundingRect(
                    QRectF(0, 0, 9999, fm.height()),
                    Qt::AlignLeft, text.mid(start, i - start));
                x += rect.width();
              }
              if (is_nl) {
                width = std::max(width, x);
                x = 0;
                ++lines;
                if (text[i] == QLatin1Char('\r') && i + 1 < tlen &&
                    text[i + 1] == QLatin1Char('\n')) {
                  ++i;
                }
              }
              start = i + 1;
            }
          }
          font = theme->Font();
          p.setFont(font);
        }
      }
    } else {
      auto tok = raw.value<Token>();
      auto tok_data = tok.data();
      auto rect = p.boundingRect(
          QRectF(0, 0, 9999, fm.height()),
          Qt::AlignLeft,
          QString::fromUtf8(tok_data.data(),
                            static_cast<qsizetype>(tok_data.size())));
      x = rect.width();
    }

    width = std::max(width, x);
    int h = static_cast<int>(std::ceil(fm.height() *
                static_cast<qreal>(lines))) + 4;
    int w = static_cast<int>(std::ceil(width)) + 8;
    return QSize(w, h);
  }

  return QStyledItemDelegate::sizeHint(option, index);
}

QWidget *SpreadsheetDelegate::createEditor(
    QWidget *parent, const QStyleOptionViewItem &,
    const QModelIndex &index) const {

  QVariant raw = index.data(SpreadsheetRoles::RawValueRole);

  // Bool cells: toggled via checkbox, not text-editable.
  if (raw.userType() == QMetaType::Bool) {
    return nullptr;
  }

  // Use QPlainTextEdit so Shift+Enter inserts newlines.
  auto *editor = new QPlainTextEdit(parent);
  editor->setTabChangesFocus(true);
  editor->setLineWrapMode(QPlainTextEdit::NoWrap);
  return editor;
}

void SpreadsheetDelegate::setEditorData(QWidget *editor,
                                        const QModelIndex &index) const {
  if (auto *te = qobject_cast<QPlainTextEdit *>(editor)) {
    te->setPlainText(index.data(Qt::EditRole).toString());
    te->selectAll();
  } else {
    QStyledItemDelegate::setEditorData(editor, index);
  }
}

void SpreadsheetDelegate::setModelData(QWidget *editor,
                                       QAbstractItemModel *model,
                                       const QModelIndex &index) const {
  if (auto *te = qobject_cast<QPlainTextEdit *>(editor)) {
    model->setData(index, te->toPlainText(), Qt::EditRole);
  } else {
    QStyledItemDelegate::setModelData(editor, model, index);
  }
}

void SpreadsheetDelegate::updateEditorGeometry(
    QWidget *editor, const QStyleOptionViewItem &option,
    const QModelIndex &) const {
  // Start at the cell, extend to viewport edge, and give enough
  // height for multiline editing (Shift+Enter).
  QRect rect = option.rect;
  if (option.widget) {
    int viewport_right = option.widget->width();
    rect.setRight(std::max(rect.right(), viewport_right - 2));
  }
  rect.setHeight(std::max(rect.height(), 80));
  editor->setGeometry(rect);
}

bool SpreadsheetDelegate::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    auto *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
      if (ke->modifiers() & Qt::ShiftModifier) {
        return false;
      }
      emit commitData(qobject_cast<QWidget *>(object));
      emit closeEditor(qobject_cast<QWidget *>(object));
      return true;
    }
    if (ke->key() == Qt::Key_Escape) {
      emit closeEditor(qobject_cast<QWidget *>(object));
      return true;
    }
  }
  return QStyledItemDelegate::eventFilter(object, event);
}

}  // namespace mx::gui
