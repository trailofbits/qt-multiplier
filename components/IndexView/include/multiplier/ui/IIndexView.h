/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IFileTreeModel.h>

#include <QWidget>

namespace mx::gui {

//! A widget that displays the contents of an Index as a tree view
class IIndexView : public QWidget {
  Q_OBJECT

 public:
  //! Factory function
  static IIndexView *Create(IFileTreeModel *model, QWidget *parent = nullptr);

  //! Constructor
  IIndexView(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~IIndexView() override = default;

  //! Disable the copy constructor
  IIndexView(const IIndexView &) = delete;

  //! Disable copy assignment
  IIndexView &operator=(const IIndexView &) = delete;

 signals:
  //! Emitted when an item has been clicked or double clicked
  void ItemClicked(const QModelIndex &model_index, bool double_click);

  //! Emitted when a file has been clicked or double clicked
  void FileClicked(const PackedFileId &file_id, const std::string &file_name,
                   bool double_click);
};

}  // namespace mx::gui
