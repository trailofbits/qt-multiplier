/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/CodeWidget.h>

#include <QPalette>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

#include "CodeModel.h"

namespace mx::gui {

struct CodeWidget::PrivateData {
  CodeModel *model{nullptr};
  QPlainTextEdit *view{nullptr};
  IThemePtr theme;
};

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(const ConfigManager &config_manager, QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  d->model = new CodeModel(this);
  d->view = new QPlainTextEdit(this);
  d->view->setReadOnly(true);
  d->view->setOverwriteMode(false);
  d->view->setTextInteractionFlags(Qt::TextSelectableByMouse);
  d->view->setContextMenuPolicy(Qt::CustomContextMenu);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->view, 1);
  layout->addStretch();

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  auto &media_manager = config_manager.MediaManager();
  auto &theme_manager = config_manager.ThemeManager();

  OnThemeChanged(theme_manager);
  OnIconsChanged(media_manager);

  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &CodeWidget::OnIndexChanged);

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &CodeWidget::OnThemeChanged);

  connect(&media_manager, &MediaManager::IconsChanged,
          this, &CodeWidget::OnIconsChanged);
}

void CodeWidget::SetTokenTree(TokenTree tree) {
  d->view->setDocument(d->model->Set(std::move(tree), d->theme.get()));
}

void CodeWidget::OnIndexChanged(const ConfigManager &config_manager) {
  d->model->Reset();

  (void) config_manager;
}

void CodeWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  d->theme = theme_manager.Theme();

  d->view->setFont(d->theme->Font());

  auto theme = d->theme.get();
  auto palette = d->view->palette();
  palette.setColor(QPalette::Window, theme->DefaultBackgroundColor());
  palette.setColor(QPalette::WindowText, theme->DefaultForegroundColor());
  palette.setColor(QPalette::Base, theme->DefaultBackgroundColor());
  palette.setColor(QPalette::Text, theme->DefaultForegroundColor());
  palette.setColor(QPalette::AlternateBase, theme->DefaultBackgroundColor());
  d->view->setPalette(palette);

  d->model->ChangeTheme(theme);
}

void CodeWidget::OnIconsChanged(const MediaManager &media_manager) {
  (void) media_manager;
}

}  // namespace mx::gui
