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

struct FilterSettingsWidget::PrivateData final {
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
  setContentsMargins(0, 0, 0, 0);

  auto new_layout = new QHBoxLayout();
  new_layout->setContentsMargins(0, 0, 0, 0);

  d->checkbox_list.clear();

  QModelIndex root_index;
  int column_count = std::max(0, d->model->columnCount(root_index));

  new_layout->addWidget(new QLabel(tr("Filter: ")));

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
    new_layout->addWidget(checkbox);
  }

  new_layout->addStretch();
  setLayout(new_layout);

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
