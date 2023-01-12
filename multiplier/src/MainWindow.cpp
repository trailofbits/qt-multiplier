// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "MainWindow.h"

#include <QTreeView>

#include <multiplier/Index.h>
#include <multiplier/ui/IFileTreeModel.h>

namespace mx::gui {

namespace {

const std::string kDatabasePath{"/Users/alessandro/Downloads/pcre2.db"};

}

struct MainWindow::PrivateData final {
  mx::Index index;
};

MainWindow::MainWindow() : QMainWindow(nullptr), d(new PrivateData) {
  d->index = mx::EntityProvider::from_database(kDatabasePath);
  auto model = IFileTreeModel::Create(d->index, this);

  auto tree_view = new QTreeView();
  tree_view->setModel(model);
  tree_view->expandRecursively(QModelIndex());
  tree_view->setHeaderHidden(true);

  setCentralWidget(tree_view);
}

MainWindow::~MainWindow() {}

}  // namespace mx::gui
