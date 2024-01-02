/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/FilterSettingsWidget.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>

#include <algorithm>

namespace mx::gui {
namespace {

static void ClearLayout(QLayout *layout);

static void ClearLayoutItem(QLayoutItem *item) {
  if (item) {
    if (item->layout() != 0) {
      ClearLayout(item->layout());

    } else if (item->widget()) {
      item->widget()->deleteLater();
    }

    delete item;
  }
}

void ClearLayout(QLayout *layout) {
  while (auto child = layout->takeAt(0)) {
    ClearLayoutItem(child);
  }
}
}  // namespace

struct FilterSettingsWidget::PrivateData final {
  QHBoxLayout *layout{nullptr};
  QAbstractItemModel *model{nullptr};
  std::vector<QCheckBox *> checkbox_list;
};

FilterSettingsWidget::FilterSettingsWidget(QAbstractItemModel *model,
                                           QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->model = model;
  connect(model, &QAbstractItemModel::modelReset, this,
          &FilterSettingsWidget::OnModelReset);


  d->layout = new QHBoxLayout();
  d->layout->setContentsMargins(0, 0, 0, 0);
  setContentsMargins(0, 0, 0, 0);
  setLayout(d->layout);

  OnModelReset();
}

FilterSettingsWidget::~FilterSettingsWidget(void) {}

std::vector<bool> FilterSettingsWidget::GetColumnFilterStateList() {
  std::vector<bool> state_list;

  for (const auto &checkbox : d->checkbox_list) {
    state_list.push_back(checkbox->isChecked());
  }

  return state_list;
}

void FilterSettingsWidget::OnModelReset(void) {
  InitializeWidgets();
}

void FilterSettingsWidget::InitializeWidgets(void) {

  ClearLayout(d->layout);

  d->layout->addWidget(new QLabel(tr("Filter: ")));
  d->checkbox_list.clear();

  QModelIndex root_index;
  int column_count = std::max(0, d->model->columnCount(root_index));

  for (auto i = 0; i < column_count; ++i) {
    QString column_name;

    if (auto column_name_var =
            d->model->headerData(i, Qt::Horizontal, Qt::DisplayRole);
        column_name_var.isValid()) {
      column_name = column_name_var.toString();
    }

    if (column_name.isEmpty()) {
      column_name = tr("Column #") + QString::number(i);
      qDebug() << "Warning: column" << i << "has no headerData value";
    }

    auto checkbox = new QCheckBox(column_name);
    checkbox->setChecked(true);

    connect(checkbox, &QCheckBox::stateChanged, this,
            &FilterSettingsWidget::OnCheckboxStateChange);

    d->checkbox_list.push_back(checkbox);
    d->layout->addWidget(checkbox);
  }

  d->layout->addStretch();
  EmitColumnFilterStateListChanged();
}

void FilterSettingsWidget::OnCheckboxStateChange(int) {
  EmitColumnFilterStateListChanged();
}

void FilterSettingsWidget::Activate(void) {
  ResetCheckboxes();
  show();
}

void FilterSettingsWidget::Deactivate(void) {
  ResetCheckboxes();
  hide();
}

void FilterSettingsWidget::ResetCheckboxes(void) {
  for (auto checkbox : d->checkbox_list) {
    checkbox->setChecked(true);
  }

  EmitColumnFilterStateListChanged();
}

void FilterSettingsWidget::EmitColumnFilterStateListChanged() {
  emit ColumnFilterStateListChanged(GetColumnFilterStateList());
}

}  // namespace mx::gui
