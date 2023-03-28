/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QStyledItemDelegate>

namespace mx::gui {

//! A delegate used by the IReferenceExplorer widget to draw nodes
class ReferenceExplorerItemDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  //! Constructor
  ReferenceExplorerItemDelegate(QObject *parent = nullptr);

  //! Destructor
  virtual ~ReferenceExplorerItemDelegate() override;

  //! Helps Qt determine what's the ideal QTreeView item size
  virtual QSize sizeHint(const QStyleOptionViewItem &option,
                         const QModelIndex &index) const override;

  //! Draws the item data
  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
                     const QModelIndex &index) const override;

  //! Disabled copy constructor
  ReferenceExplorerItemDelegate(const ReferenceExplorerItemDelegate &) = delete;

  //! Disabled copy assignment operator
  ReferenceExplorerItemDelegate &
  operator=(const ReferenceExplorerItemDelegate &) = delete;

  //!
  static void DrawIcon(QPainter &painter, const int &size, const QString &text,
                       const QColor &bg_color);

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
