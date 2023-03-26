/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorer.h"
#include "TextBasedReferenceExplorer.h"
#include "GraphicalReferenceExplorer.h"

#include <QVBoxLayout>
#include <QSplitter>
#include <QApplication>
#include <QScreen>

namespace mx::gui {

struct ReferenceExplorer::PrivateData final {
  TextBasedReferenceExplorer *text_view{nullptr};
  GraphicalReferenceExplorer *graphical_view{nullptr};
};

ReferenceExplorer::~ReferenceExplorer() {}

IReferenceExplorerModel *ReferenceExplorer::Model() {
  return d->graphical_view->Model();
}

ReferenceExplorer::ReferenceExplorer(IReferenceExplorerModel *model,
                                     QWidget *parent)
    : IReferenceExplorer(parent),
      d(new PrivateData) {

  InitializeWidgets(model);
}

void ReferenceExplorer::InitializeWidgets(IReferenceExplorerModel *model) {
  d->text_view = new TextBasedReferenceExplorer(model, this);


  connect(d->text_view, &TextBasedReferenceExplorer::SelectedItemChanged, this,
          &IReferenceExplorer::SelectedItemChanged);

  connect(d->text_view, &TextBasedReferenceExplorer::ItemActivated, this,
          &IReferenceExplorer::ItemActivated);

  d->graphical_view = new GraphicalReferenceExplorer(model, this);

  connect(d->graphical_view, &TextBasedReferenceExplorer::SelectedItemChanged,
          this, &IReferenceExplorer::SelectedItemChanged);

  connect(d->graphical_view, &TextBasedReferenceExplorer::ItemActivated, this,
          &IReferenceExplorer::ItemActivated);

  setContentsMargins(0, 0, 0, 0);

  auto splitter = new QSplitter(Qt::Vertical, this);
  splitter->setContentsMargins(0, 0, 0, 0);
  splitter->addWidget(d->graphical_view);
  splitter->addWidget(d->text_view);

  const auto &primary_screen = *QGuiApplication::primaryScreen();
  auto screen_height = primary_screen.virtualSize().height();

  auto treeview_size = (screen_height / 3) * 2;
  auto textview_size = screen_height - treeview_size;

  splitter->setSizes({treeview_size, textview_size});

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(splitter);
  setLayout(layout);
}

}  // namespace mx::gui
