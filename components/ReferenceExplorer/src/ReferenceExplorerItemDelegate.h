/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/CodeViewTheme.h>

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

namespace mx::gui {

//! An item delegate used to paint tokens in the ReferenceExplorerView
class ReferenceExplorerItemDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  //! Constructor
  explicit ReferenceExplorerItemDelegate(const CodeViewTheme &theme,
                                         QObject *parent = nullptr);

  //! Destructor
  virtual ~ReferenceExplorerItemDelegate(void) override;

  //! Sets the specified theme, refreshing the view
  void SetTheme(const CodeViewTheme &theme);

  //! Sets the specified tab stop distance, refreshing the view
  void SetTabWidth(std::size_t width);

  //! Sets the replacement for whitespace.
  //! \note Useful if you want to change how whitespace is rendered
  void SetWhitespaceReplacement(QString data);

  //! Clears the whitespace replacement, restoring the original characters
  void ClearWhitespaceReplacement(void);

  //! Paints the delegate to screen
  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
                     const QModelIndex &index) const override;

  //! Returns the size hint for the specified model index
  virtual QSize sizeHint(const QStyleOptionViewItem &option,
                         const QModelIndex &index) const override;

 public slots:
  //! Called by the theme manager when the theme is changed
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

 protected:
  //! Triggered when the user tries to edit the QTreeView item
  virtual bool editorEvent(QEvent *event, QAbstractItemModel *model,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
