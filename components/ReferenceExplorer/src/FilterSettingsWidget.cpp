/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FilterSettingsWidget.h"

#include <multiplier/ui/Assert.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>

namespace mx::gui {

struct FilterSettingsWidget::PrivateData final {
  QCheckBox *file_name_filter_check{nullptr};
  QCheckBox *entity_name_filter_check{nullptr};
  QCheckBox *breadcrumbs_filter_check{nullptr};
  QCheckBox *entity_id_filter_check{nullptr};
};

FilterSettingsWidget::FilterSettingsWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets();
  ResetSearchSettings();
}

FilterSettingsWidget::~FilterSettingsWidget() {}

bool FilterSettingsWidget::FilterByFileName() const {
  return d->file_name_filter_check->isChecked();
}

bool FilterSettingsWidget::FilterByEntityName() const {
  return d->entity_name_filter_check->isChecked();
}

bool FilterSettingsWidget::FilterByBreadcrumbs() const {
  return d->breadcrumbs_filter_check->isChecked();
}

bool FilterSettingsWidget::FilterByEntityID() const {
  return d->entity_id_filter_check->isChecked();
}

void FilterSettingsWidget::InitializeWidgets() {
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  layout->addWidget(new QLabel(tr("Entity:")));

  d->entity_name_filter_check = new QCheckBox(tr("Name"));
  layout->addWidget(d->entity_name_filter_check);

  connect(d->entity_name_filter_check, &QCheckBox::stateChanged, this,
          &FilterSettingsWidget::FilterParametersChanged);

  d->breadcrumbs_filter_check = new QCheckBox(tr("Breadcrumbs"));
  layout->addWidget(d->breadcrumbs_filter_check);

  connect(d->breadcrumbs_filter_check, &QCheckBox::stateChanged, this,
          &FilterSettingsWidget::FilterParametersChanged);

  d->file_name_filter_check = new QCheckBox(tr("File name"));
  layout->addWidget(d->file_name_filter_check);

  connect(d->file_name_filter_check, &QCheckBox::stateChanged, this,
          &FilterSettingsWidget::FilterParametersChanged);

  d->entity_id_filter_check = new QCheckBox(tr("ID"));
  layout->addWidget(d->entity_id_filter_check);

  connect(d->entity_id_filter_check, &QCheckBox::stateChanged, this,
          &FilterSettingsWidget::FilterParametersChanged);

  layout->addStretch();
  setLayout(layout);
}

void FilterSettingsWidget::Activate() {
  show();

  emit FilterParametersChanged();
}

void FilterSettingsWidget::Deactivate() {
  hide();

  ResetSearchSettings();

  emit FilterParametersChanged();
}

void FilterSettingsWidget::ResetSearchSettings() {
  d->file_name_filter_check->setChecked(true);
  d->entity_name_filter_check->setChecked(true);
  d->breadcrumbs_filter_check->setChecked(true);
  d->entity_id_filter_check->setChecked(false);
}

}  // namespace mx::gui
