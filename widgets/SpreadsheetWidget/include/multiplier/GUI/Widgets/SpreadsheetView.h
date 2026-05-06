/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QPointer>
#include <QSet>
#include <QTableView>

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QMenu;
class QModelIndex;
class QTextBrowser;
class QTimer;
QT_END_NAMESPACE

namespace mx::gui {

class ConfigManager;
class ProvenancePopup;

// A QTableView subclass providing spreadsheet-like interaction: keyboard
// shortcuts for clipboard operations, context menus for row/column
// manipulation, and extended cell-level selection.
class SpreadsheetView Q_DECL_FINAL : public QTableView {
  Q_OBJECT

  ConfigManager *config_manager_{nullptr};
  QSet<int> clickable_columns_;
  QPointer<ProvenancePopup> provenance_popup_;

 public:
  explicit SpreadsheetView(QWidget *parent = nullptr);

  void SetConfigManager(ConfigManager *cm) { config_manager_ = cm; }
  virtual ~SpreadsheetView(void);

  // Per-column clickable-tokens mode.
  bool IsColumnClickable(int col) const { return clickable_columns_.contains(col); }
  void SetColumnClickable(int col, bool clickable);

  // Update header/grid colors from theme.
  void ApplyThemeColors(const QColor &gutter_bg, const QColor &gutter_fg,
                        const QColor &grid_color);

  // Clipboard operations.
  void copy_selection(void);
  void paste_at_selection(void);
  void cut_selection(void);
  void delete_selection(void);

  // Export formats.
  void copy_as_markdown(void);
  void copy_as_html(void);
  void copy_as_csv(void);
  void copy_as_tsv(void);

 signals:
  void TokenClicked(const QModelIndex &index);
  void DocumentCellClicked(const QModelIndex &index);
  void AgentTaskRequested(const QString &prompt);
  void HarnessRequested(const QString &vulnerability_desc,
                        int row_index);
  void ProvenanceCellClicked(int64_t session_id, const QString &result_id,
                             const QStringList &follows);

 protected:
  void keyPressEvent(QKeyEvent *event) Q_DECL_FINAL;
  void mousePressEvent(QMouseEvent *event) Q_DECL_FINAL;
  bool viewportEvent(QEvent *event) Q_DECL_OVERRIDE;
  void updateGeometries(void) Q_DECL_OVERRIDE;

 private slots:
  void OnContextMenu(const QPoint &pos);
};

}  // namespace mx::gui
