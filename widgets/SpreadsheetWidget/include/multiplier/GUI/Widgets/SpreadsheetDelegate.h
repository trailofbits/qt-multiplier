/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QHash>
#include <QPair>
#include <QStyledItemDelegate>

#include <multiplier/GUI/Interfaces/ITheme.h>

namespace mx::gui { class ConfigManager; }

namespace mx::gui {

// Item delegate for the spreadsheet view. Renders Token / TokenRange cells
// with per-token syntax highlighting via the current theme. FormulaCell
// values display their cached result. Bool cells are rendered as checkboxes.
// QString cells use a QPlainTextEdit editor supporting multiline (Shift+Enter).
class SpreadsheetDelegate Q_DECL_FINAL : public QStyledItemDelegate {
  Q_OBJECT

  IThemePtr theme;
  unsigned tab_width{4};
  ConfigManager *config_manager_{nullptr};
  mutable QHash<int, QPair<QString, quint64>> doc_title_cache;

 public:
  explicit SpreadsheetDelegate(IThemePtr theme_, unsigned tab_width_ = 4,
                               QObject *parent = nullptr);
  virtual ~SpreadsheetDelegate(void);

  void SetTheme(IThemePtr new_theme);
  void SetTabWidth(unsigned tw);
  void SetConfigManager(ConfigManager *cm) { config_manager_ = cm; }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const Q_DECL_FINAL;

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const Q_DECL_FINAL;

  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                        const QModelIndex &index) const Q_DECL_FINAL;

  void setEditorData(QWidget *editor,
                     const QModelIndex &index) const Q_DECL_FINAL;

  void setModelData(QWidget *editor, QAbstractItemModel *model,
                    const QModelIndex &index) const Q_DECL_FINAL;

  void updateEditorGeometry(QWidget *editor,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const Q_DECL_FINAL;

  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) Q_DECL_FINAL;

  bool eventFilter(QObject *object, QEvent *event) Q_DECL_FINAL;
};

}  // namespace mx::gui
