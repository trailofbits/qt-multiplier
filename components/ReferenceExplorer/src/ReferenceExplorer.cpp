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
                                     const Mode &mode, QWidget *parent,
                                     IGlobalHighlighter *global_highlighter)
    : IReferenceExplorer(parent),
      d(new PrivateData) {

  model->setParent(this);
  InitializeWidgets(model, mode, global_highlighter);
}

void ReferenceExplorer::InitializeWidgets(
    IReferenceExplorerModel *model, const Mode &mode,
    IGlobalHighlighter *global_highlighter) {

  d->text_view =
      new TextBasedReferenceExplorer(model, this, global_highlighter);

  connect(d->text_view, &TextBasedReferenceExplorer::SelectedItemChanged, this,
          &IReferenceExplorer::SelectedItemChanged);

  connect(d->text_view, &TextBasedReferenceExplorer::ItemActivated, this,
          &IReferenceExplorer::ItemActivated);

  d->graphical_view =
      new GraphicalReferenceExplorer(model, this, global_highlighter);

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

  switch (mode) {
    case Mode::TreeView: splitter->setSizes({screen_height, 0}); break;
    case Mode::TextView: splitter->setSizes({0, screen_height}); break;

    case Mode::Split:
      splitter->setSizes({screen_height / 2, screen_height / 2});
      break;
  }

  auto layout = new QVBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(splitter);
  setLayout(layout);
}

}  // namespace mx::gui
