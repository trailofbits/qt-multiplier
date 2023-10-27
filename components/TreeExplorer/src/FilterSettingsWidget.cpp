/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FilterSettingsWidget.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>

namespace mx::gui {

struct FilterSettingsWidget::PrivateData final {
  QAbstractItemModel *model{nullptr};
  int num_columns{0};
  std::vector<QCheckBox *> column_checks;

  std::vector<bool> State(void) const {
    std::vector<bool> checks;
    for (auto check : column_checks) {
      if (check) {
        checks.push_back(check->isChecked());
      } else {
        checks.push_back(false);
      }
    }
    return checks;
  }
};

FilterSettingsWidget::FilterSettingsWidget(QAbstractItemModel *model, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {
  InstallModel(model);
  ResetSearchSettings();
}

FilterSettingsWidget::~FilterSettingsWidget() {}

//! Initializes the widget from the model.
void FilterSettingsWidget::InstallModel(QAbstractItemModel *model) {
  d->model = model;
  connect(model, &QAbstractItemModel::modelReset,
          this, &FilterSettingsWidget::OnModelReset);

  OnModelReset();
}

void FilterSettingsWidget::OnModelReset(void) {
  QModelIndex root_index;
  d->num_columns = std::max(0, d->model->columnCount(root_index));

  auto old_layout = layout();
  InitializeWidgets();

  if (old_layout) {
    old_layout->deleteLater();
  }
}

//! Returns true if the Nth column should be filtered.
bool FilterSettingsWidget::FilterByColumn(int n) const {
  if (0 > n) {
    return false;
  }

  auto i = static_cast<unsigned>(n);
  if (i >= d->column_checks.size()) {
    return false;
  }

  QCheckBox *check = d->column_checks[i];
  if (!check) {
    return false;
  }

  return check->isChecked();
}

void FilterSettingsWidget::InitializeWidgets() {
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  auto has_any = false;
  d->column_checks.clear();

  for (auto i = 0; i < d->num_columns; ++i) {
    QVariant data = d->model->headerData(i, Qt::Horizontal, Qt::DisplayRole);
    QString label = data.toString();
    if (!label.size()) {
      d->column_checks.push_back(nullptr);
      continue;
    }

    if (!has_any) {
      layout->addWidget(new QLabel(tr("Filter: ")));
      has_any = true;
    }

    auto check = new QCheckBox(label);
    check->setChecked(true);
    layout->addWidget(check);

    connect(check, &QCheckBox::stateChanged, this,
            &FilterSettingsWidget::OnStateChange);
    d->column_checks.push_back(check);
  }

  layout->addStretch();
  setLayout(layout);
}

void FilterSettingsWidget::OnStateChange(int) {
  emit FilterParametersChanged(d->State());
}

void FilterSettingsWidget::Activate() {
  show();
  emit FilterParametersChanged(d->State());
}

void FilterSettingsWidget::Deactivate() {
  hide();
  ResetSearchSettings();
  emit FilterParametersChanged(d->State());
}

void FilterSettingsWidget::ResetSearchSettings() {
  for (auto check : d->column_checks) {
    if (check) {
      check->setChecked(true);
    }
  }
}

}  // namespace mx::gui
