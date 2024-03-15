// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QStyledItemDelegate>

namespace mx::gui {

class ConfigEditorDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  static ConfigEditorDelegate *Create(QWidget *parent);
  virtual ~ConfigEditorDelegate(void) override;

  virtual QWidget *createEditor(QWidget *parent,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const override;

  virtual void setEditorData(QWidget *editor,
                             const QModelIndex &index) const override;

  virtual void setModelData(QWidget *editor, QAbstractItemModel *model,
                            const QModelIndex &index) const override;

  ConfigEditorDelegate(const ConfigEditorDelegate &) = delete;
  ConfigEditorDelegate &operator=(const ConfigEditorDelegate &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  ConfigEditorDelegate(QWidget *parent);
};

}  // namespace mx::gui
