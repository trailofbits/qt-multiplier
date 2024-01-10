/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Widgets/CodeWidget.h>

#include <QHBoxLayout>
#include <QPalette>
#include <QPainter>
#include <QScrollBar>
#include <QSpacerItem>
#include <QVBoxLayout>

#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Managers/ThemeManager.h>

// #include "CodeModel.h"

namespace mx::gui {

struct CodeWidget::PrivateData {
  // CodeModel *model{nullptr};
  // QPlainTextEdit *view{nullptr};

  const TokenTree token_tree;
  IThemePtr theme;

  QScrollBar *horizontal_scrollbar{nullptr};
  QScrollBar *vertical_scrollbar{nullptr};

  inline PrivateData(const TokenTree &token_tree_)
      : token_tree(token_tree_) {}
};

CodeWidget::~CodeWidget(void) {}

CodeWidget::CodeWidget(const ConfigManager &config_manager,
                       const TokenTree &token_tree,
                       QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(token_tree)) {

  d->vertical_scrollbar = new QScrollBar(Qt::Vertical, this);
  d->vertical_scrollbar->setSingleStep(1);
  // connect(d->vertical_scrollbar, &QScrollBar::valueChanged, this,
  //         &TextView::onScrollBarValueChange);

  d->horizontal_scrollbar = new QScrollBar(Qt::Horizontal, this);
  d->horizontal_scrollbar->setSingleStep(1);
  // connect(d->horizontal_scrollbar, &QScrollBar::valueChanged, this,
  //         &TextView::onScrollBarValueChange);

  auto vertical_layout = new QVBoxLayout(this);
  vertical_layout->setContentsMargins(0, 0, 0, 0);
  vertical_layout->setSpacing(0);
  vertical_layout->addSpacerItem(
      new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));

  vertical_layout->addWidget(d->horizontal_scrollbar);
  
  auto horizontal_layout = new QHBoxLayout();
  horizontal_layout->setContentsMargins(0, 0, 0, 0);
  horizontal_layout->setSpacing(0);
  horizontal_layout->addLayout(vertical_layout);
  horizontal_layout->addWidget(d->vertical_scrollbar);

  setLayout(horizontal_layout);
  setContentsMargins(0, 0, 0, 0);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

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

void CodeWidget::OnIndexChanged(const ConfigManager &config_manager) {
  // d->model->Reset();

  (void) config_manager;
}

void CodeWidget::OnThemeChanged(const ThemeManager &theme_manager) {
  d->theme = theme_manager.Theme();

  // d->view->setFont(d->theme->Font());

  auto theme = d->theme.get();
  QPalette p = palette();
  p.setColor(QPalette::Window, theme->DefaultBackgroundColor());
  p.setColor(QPalette::WindowText, theme->DefaultForegroundColor());
  p.setColor(QPalette::Base, theme->DefaultBackgroundColor());
  p.setColor(QPalette::Text, theme->DefaultForegroundColor());
  p.setColor(QPalette::AlternateBase, theme->DefaultBackgroundColor());
  setPalette(p);

  // d->model->ChangeTheme(theme);
}

void CodeWidget::OnIconsChanged(const MediaManager &media_manager) {
  (void) media_manager;
}

}  // namespace mx::gui
