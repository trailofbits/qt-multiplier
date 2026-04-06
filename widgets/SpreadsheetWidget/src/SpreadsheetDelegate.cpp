// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Widgets/SpreadsheetDelegate.h>

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>
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
    QFont font = theme->Font();
    QFontMetricsF fm(font);

    bool selected = (opt.state & QStyle::State_Selected);

    // Clip to cell rect to prevent painting over adjacent cells.
    painter->save();
    painter->setClipRect(opt.rect);

    QPointF pos(opt.rect.left() + 2, opt.rect.top() + fm.ascent());

    for (Token tok : range) {
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
            pos.setX(pos.x() + fm.horizontalAdvance(QLatin1Char(' ')) *
                     static_cast<qreal>(tab_width));
          } else if (ch == QLatin1Char(' ')) {
            pos.setX(pos.x() + fm.horizontalAdvance(ch));
          }
        }
      } else {
        // Draw the whole token string at once for proper kerning.
        QRectF text_rect(pos.x(), pos.y() - fm.ascent(),
                         opt.rect.right() - pos.x(), fm.height());
        painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter,
                          text);
        pos.setX(pos.x() + fm.horizontalAdvance(text));
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

  // Bool and QString: fall through to the default delegate.
  QStyledItemDelegate::paint(painter, option, index);
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
  QRect rect = option.rect;
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
