/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CodePreviewWidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QThreadPool>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

#include <multiplier/Frontend/TokenTree.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/HistoryWidget.h>
#include <multiplier/GUI/Widgets/CodeWidget.h>

namespace mx::gui {
namespace {

static constexpr unsigned kMaxHistorySize = 32;

}  // namespace

struct CodePreviewWidget::PrivateData {

  // Preview of the code.
  CodeWidget * const code;

  // Toolbar of buttons.
  QToolBar * const toolbar;

  // Widget keeping track of the history of the entity information browser. May
  // be `nullptr`.
  HistoryWidget * const history;

  // Used to pop out a copy of the current entity info into a pinned info
  // browser. May be `nullptr`.
  QPushButton * const pop_out_button;

  // Current entity being shown by this widget.
  VariantEntity current_entity;
  VariantEntity containing_entity;

  // Should we show a checkbox and synchronize this info explorer with
  bool sync{true};

  // Trigger to open some info in a pinned preview.
  TriggerHandle pinned_entity_info_trigger;

  inline PrivateData(const ConfigManager &config_manager, bool enable_history,
                     QWidget *parent)
      : code(new CodeWidget(config_manager, kModelPrefix, parent)),
        toolbar(enable_history ? new QToolBar(parent) : nullptr),
        history(
            enable_history ?
            new HistoryWidget(config_manager, kMaxHistorySize, false, toolbar) :
            nullptr),
        pop_out_button(enable_history ? new QPushButton(toolbar) : nullptr),
        pinned_entity_info_trigger(config_manager.ActionManager().Find(
            "com.trailofbits.action.OpenPinnedEntityPreview")) {}
};

const QString CodePreviewWidget::kModelPrefix(
    "com.trailofbits.CodePreviewModel");

CodePreviewWidget::~CodePreviewWidget(void) {}

CodePreviewWidget::CodePreviewWidget(
    const ConfigManager &config_manager, bool enable_history,
    QWidget *parent)
    : IWindowWidget(parent),
      d(new PrivateData(config_manager, enable_history, this)) {

  setWindowTitle(tr("Code Preview"));

  connect(d->code, &CodeWidget::RequestPrimaryClick,
          this, &IWindowWidget::RequestPrimaryClick);

  connect(d->code, &CodeWidget::RequestSecondaryClick,
          this, &IWindowWidget::RequestSecondaryClick);

  connect(d->code, &CodeWidget::RequestKeyPress,
          this, &IWindowWidget::RequestKeyPress);

  connect(d->code, &CodeWidget::Closed,
          this, &IWindowWidget::close);

  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  if (d->toolbar) {
    d->toolbar->addWidget(d->history);
    d->toolbar->setIconSize(QSize(16, 16));
    d->history->SetIconSize(d->toolbar->iconSize());

    // Add a popout icon, to pop the current info into a pinned browser.
    auto &media_manager = config_manager.MediaManager();
    OnIconsChanged(media_manager);
    d->toolbar->addWidget(new QLabel(" "));
    d->toolbar->addWidget(d->pop_out_button);
    d->pop_out_button->setEnabled(false);
    d->pop_out_button->setToolTip(
        tr("Duplicate this preview into a pinned code preview"));

    // Create a sync checkbox that tells us whether or not to keep this entity
    // information explorer up-to-date with user clicks.
    auto sync = new QCheckBox(tr("Sync"), this);
    sync->setTristate(false);
    sync->setCheckState(Qt::Checked);
    d->toolbar->addWidget(new QLabel(" "));
    d->toolbar->addWidget(sync);

#ifndef QT_NO_TOOLTIP
    sync->setToolTip(tr("Keep in sync with preview requests in other views"));
#endif

    connect(d->history, &HistoryWidget::GoToEntity,
            this, [this] (VariantEntity original_entity, VariantEntity) {
                    emit HistoricalEntitySelected(std::move(original_entity));
                  });

    connect(sync, &QCheckBox::stateChanged,
            this, &CodePreviewWidget::OnChangeSync);

    connect(&media_manager, &MediaManager::IconsChanged,
            this, &CodePreviewWidget::OnIconsChanged);

    connect(d->pop_out_button, &QPushButton::pressed,
            this, &CodePreviewWidget::OnPopOutPressed);

    layout->addWidget(d->toolbar);
  }

  layout->addWidget(d->code, 1);
  layout->addStretch();

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
}

void CodePreviewWidget::OnPopOutPressed(void) {
  d->pinned_entity_info_trigger.Trigger(QVariant::fromValue(d->current_entity));
}

void CodePreviewWidget::OnIconsChanged(
    const MediaManager &media_manager) {
  if (!d->pop_out_button) {
    return;
  }

  QIcon pop_out_icon;
  pop_out_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.PopOut"),
                                              QIcon::Normal, QIcon::On);
  
  pop_out_icon.addPixmap(media_manager.Pixmap("com.trailofbits.icon.PopOut",
                                              ITheme::IconStyle::DISABLED),
                                              QIcon::Disabled, QIcon::On);

  d->pop_out_button->setIcon(pop_out_icon);
  d->pop_out_button->setIconSize(d->toolbar->iconSize());
}

void CodePreviewWidget::DisplayEntity(
    VariantEntity entity, bool is_explicit_request, bool add_to_history) {

  // If we don't have this info browser synchronized with implicit events, then
  // ignore this request to display the entity.
  if (!is_explicit_request && !d->sync) {
    return;
  }

  if (std::holds_alternative<NotAnEntity>(entity)) {
    return;
  }

  // Dedup check; don't want to reload the model unnecessarily.
  if (EntityId(d->current_entity) == EntityId(entity)) {
    return;
  }

  TokenTree tt;
  VariantEntity containing_entity;
  if (auto type = Type::from(entity)) {
    containing_entity = type.value();
    tt = TokenTree::from(type->tokens());
  
  } else if (auto frag = Fragment::containing(entity)) {
    containing_entity = frag.value();
    tt = TokenTree::from(frag.value());
  
  } else if (auto file = File::containing(entity)) {
    containing_entity = file.value();
    tt = TokenTree::from(file.value());
  }

  if (std::holds_alternative<NotAnEntity>(containing_entity)) {
    return;
  }

  // Dedup check; don't want to reload the model unnecessarily.
  if (EntityId(d->containing_entity) == EntityId(containing_entity)) {

    // TODO(pag): Change scroll position.

    return;
  }

  d->current_entity = std::move(entity);
  d->containing_entity = std::move(containing_entity);

  if (d->pop_out_button) {
    d->pop_out_button->setEnabled(true);
  }

  // If we're showing the history widget then keep track of the history.
  if (add_to_history && d->history) {
    d->history->CommitCurrentLocationToHistory();
    d->history->SetCurrentLocation(d->current_entity);
  }

  d->code->SetTokenTree(tt);
}

void CodePreviewWidget::OnChangeSync(int state) {
  d->sync = Qt::Checked == state;
}

// Invoked when the set of macros to be expanded changes.
void CodePreviewWidget::OnExpandMacros(
    const QSet<RawEntityId> &macros_to_expand) {
  d->code->OnExpandMacros(macros_to_expand);
}

// Invoked when the set of entities to be renamed changes.
void CodePreviewWidget::OnRenameEntities(
    const QMap<RawEntityId, QString> &new_entity_names) {
  d->code->OnRenameEntities(new_entity_names);
}

// Invoked when we want to scroll to a specific entity.
void CodePreviewWidget::OnGoToEntity(const VariantEntity &entity) {
  d->code->OnGoToEntity(entity);
}

}  // namespace mx::gui
