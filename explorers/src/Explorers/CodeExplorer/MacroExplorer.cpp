// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.


#include "MacroExplorer.h"

#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>

#include "ExpandedMacrosModel.h"

namespace mx::gui {

class ExpandedMacrosModel;

MacroExplorer::~MacroExplorer(void) {}

MacroExplorer::MacroExplorer(const ConfigManager &config_manager,
                             ExpandedMacrosModel *model,
                             QWidget *parent)
    : IWindowWidget(parent),
      table(new QTableView(this)) {

  table->setAlternatingRowColors(false);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setTextElideMode(Qt::TextElideMode::ElideRight);

  // The auto scroll takes care of keeping the active item within the
  // visible viewport region. This is true for mouse clicks but also
  // keyboard navigation (i.e. arrow keys, page up/down, etc).
  table->setAutoScroll(true);

  // Smooth scrolling.
  table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  // Disallow multiple selection. If we have grouping by file enabled, then when
  // a user clicks on a file name, we instead jump down to the first entry
  // grouped under that file. This is to make using the up/down arrows easier.
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);

  // Stretch the last section.
  table->horizontalHeader()->setStretchLastSection(true);

  auto sort_model = new QSortFilterProxyModel(table);
  sort_model->setSourceModel(model);

  table->setModel(sort_model);
  table->setSortingEnabled(true);

  config_manager.InstallItemDelegate(table);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(table, 1);
  layout->addStretch();

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
  setWindowTitle(tr("Macro Explorer"));
}

}  // namespace mx::gui
