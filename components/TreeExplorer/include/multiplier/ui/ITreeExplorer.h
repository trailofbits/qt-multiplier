/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemModel>
#include <QWidget>

namespace mx::gui {

class IGlobalHighlighter;
class ITreeExplorerModel;

//! The tree explorer widget. This works for generic trees supporting
//! incremental expansion.
class ITreeExplorer : public QWidget {
  Q_OBJECT

 public:
  //! Factory method
  static ITreeExplorer *
  Create(ITreeExplorerModel *model_, QWidget *parent = nullptr,
         IGlobalHighlighter *global_highlighter = nullptr);

  //! Constructor
  inline ITreeExplorer(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~ITreeExplorer() override = default;

  //! Returns the active model.
  virtual ITreeExplorerModel *Model() const = 0;

  //! Disabled copy constructor
  ITreeExplorer(const ITreeExplorer &) = delete;

  //! Disabled copy assignment operator
  ITreeExplorer &operator=(const ITreeExplorer &) = delete;

 signals:
  //! Emitted when the selected item has changed
  void SelectedItemChanged(const QModelIndex &index);

  //! Emitted when an item has been activated using the dedicated button
  void ItemActivated(const QModelIndex &index);

  //! Emitted when the selected item should be extracted in its own view
  void ExtractSubtree(const QModelIndex &index);
};

}  // namespace mx::gui
