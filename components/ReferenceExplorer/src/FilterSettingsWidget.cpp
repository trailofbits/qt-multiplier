/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "FilterSettingsWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QCheckBox>

#include <multiplier/ui/Assert.h>

namespace mx::gui {

struct FilterSettingsWidget::PrivateData final {
  QRadioButton *none_path_filter_radio{nullptr};
  QRadioButton *name_path_filter_radio{nullptr};
  QRadioButton *full_path_filter_radio{nullptr};

  QCheckBox *entity_name_filter_check{nullptr};
  QCheckBox *entity_id_filter_check{nullptr};
};

FilterSettingsWidget::FilterSettingsWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  InitializeWidgets();
  ResetSearchSettings();
}

FilterSettingsWidget::~FilterSettingsWidget() {}

FilterSettingsWidget::PathFilterType
FilterSettingsWidget::GetPathFilterType() const {
  if (d->none_path_filter_radio->isChecked()) {
    return PathFilterType::None;

  } else if (d->name_path_filter_radio->isChecked()) {
    return PathFilterType::FileName;

  } else if (d->full_path_filter_radio->isChecked()) {
    return PathFilterType::FullPath;

  } else {
    Assert(false, "Invalid state");
    return PathFilterType::None;
  }
}

bool FilterSettingsWidget::FilterByEntityName() const {
  return d->entity_name_filter_check->isChecked();
}

bool FilterSettingsWidget::FilterByEntityID() const {
  return d->entity_id_filter_check->isChecked();
}

void FilterSettingsWidget::InitializeWidgets() {
  setContentsMargins(0, 0, 0, 0);

  auto layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);

  // Entity settings
  layout->addWidget(new QLabel(tr("Entity:")));

  d->entity_name_filter_check = new QCheckBox(tr("Name"));
  layout->addWidget(d->entity_name_filter_check);

  connect(d->entity_name_filter_check, &QCheckBox::stateChanged, this,
          &FilterSettingsWidget::FilterParametersChanged);

  d->entity_id_filter_check = new QCheckBox(tr("ID"));
  layout->addWidget(d->entity_id_filter_check);

  connect(d->entity_id_filter_check, &QCheckBox::stateChanged, this,
          &FilterSettingsWidget::FilterParametersChanged);

  // Separator
  auto separator = new QFrame(this);
  separator->setFrameShape(QFrame::VLine);
  layout->addWidget(separator);

  // Path settings
  layout->addWidget(new QLabel(tr("Path:")));

  d->none_path_filter_radio = new QRadioButton(tr("None"));
  layout->addWidget(d->none_path_filter_radio);

  connect(d->none_path_filter_radio, &QRadioButton::toggled, this,
          &FilterSettingsWidget::FilterParametersChanged);

  d->name_path_filter_radio = new QRadioButton(tr("File name"));
  layout->addWidget(d->name_path_filter_radio);

  connect(d->name_path_filter_radio, &QRadioButton::toggled, this,
          &FilterSettingsWidget::FilterParametersChanged);

  d->full_path_filter_radio = new QRadioButton(tr("Full path"));
  layout->addWidget(d->full_path_filter_radio);

  connect(d->full_path_filter_radio, &QRadioButton::toggled, this,
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
  d->none_path_filter_radio->setChecked(true);
  d->name_path_filter_radio->setChecked(false);
  d->full_path_filter_radio->setChecked(false);

  d->entity_name_filter_check->setChecked(true);
  d->entity_id_filter_check->setChecked(false);
}

}  // namespace mx::gui
