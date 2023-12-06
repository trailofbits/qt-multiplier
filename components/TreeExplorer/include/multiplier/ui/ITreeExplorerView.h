/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IGlobalHighlighter.h>
#include <multiplier/ui/IGeneratorModel.h>

#include <QAbstractItemModel>
#include <QWidget>

namespace mx::gui {

//! The tree explorer widget. This works for generic trees supporting
//! incremental expansion.
class ITreeExplorerView : public QWidget {
  Q_OBJECT

 public:
  //! Factory method
  static ITreeExplorerView *Create(IGeneratorModel *model,
                                   IGlobalHighlighter *global_highlighter,
                                   QWidget *parent = nullptr);

  //! Constructor
  inline ITreeExplorerView(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~ITreeExplorerView() override = default;

  //! Disabled copy constructor
  ITreeExplorerView(const ITreeExplorerView &) = delete;

  //! Disabled copy assignment operator
  ITreeExplorerView &operator=(const ITreeExplorerView &) = delete;

 signals:
  //! Emitted when the selected item has changed
  void SelectedItemChanged(const QModelIndex &index);

  //! Emitted when an item has been activated using the dedicated button
  void ItemActivated(const QModelIndex &index);

  //! Emitted when the selected item should be extracted in its own view
  void ExtractSubtree(const QModelIndex &index);
};

}  // namespace mx::gui
