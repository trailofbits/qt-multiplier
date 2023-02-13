/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QStyledItemDelegate>

namespace mx::gui {

class ReferenceExplorerItemDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  ReferenceExplorerItemDelegate(QObject *parent = nullptr);
  virtual ~ReferenceExplorerItemDelegate() override;

  virtual QSize sizeHint(const QStyleOptionViewItem &option,
                         const QModelIndex &index) const override;

  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option,
                     const QModelIndex &index) const override;

  ReferenceExplorerItemDelegate(const ReferenceExplorerItemDelegate &) = delete;

  ReferenceExplorerItemDelegate &
  operator=(const ReferenceExplorerItemDelegate &) = delete;

 protected:
  virtual bool editorEvent(QEvent *event, QAbstractItemModel *model,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
