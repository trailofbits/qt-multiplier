// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentSessionListWidget.h"

#include <QAction>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>

namespace mx::gui {

struct AgentSessionListWidget::PrivateData {
  ConfigManager &config_manager;
  QListWidget *list{nullptr};

  // Map list row -> session_id.
  QVector<int64_t> session_ids;

  explicit PrivateData(ConfigManager &cm)
      : config_manager(cm) {}
};

AgentSessionListWidget::~AgentSessionListWidget(void) {}

AgentSessionListWidget::AgentSessionListWidget(ConfigManager &config_manager,
                                                QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(config_manager)) {

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(4);

  d->list = new QListWidget(this);
  d->list->setContextMenuPolicy(Qt::CustomContextMenu);
  layout->addWidget(d->list, 1);

  connect(d->list, &QListWidget::itemDoubleClicked,
          this, [this](QListWidgetItem *) {
            onItemDoubleClicked(d->list->currentRow());
          });
  connect(d->list, &QListWidget::customContextMenuRequested,
          this, &AgentSessionListWidget::onContextMenu);

  refresh();
}

void AgentSessionListWidget::refresh(void) {
  d->list->clear();
  d->session_ids.clear();

  auto sessions = d->config_manager.LoadAgentSessions();
  for (const auto &info : sessions) {
    auto total_tokens = info.total_prompt_tokens + info.total_completion_tokens;
    auto text = QStringLiteral("%1\n%2 | %3 | %L4 tok")
                    .arg(info.name)
                    .arg(info.backend)
                    .arg(info.status)
                    .arg(total_tokens);
    d->list->addItem(text);
    d->session_ids.append(info.session_id);
  }
}

void AgentSessionListWidget::onItemDoubleClicked(int row) {
  if (row >= 0 && row < d->session_ids.size()) {
    emit sessionSelected(d->session_ids[row]);
  }
}

void AgentSessionListWidget::onContextMenu(const QPoint &pos) {
  auto row = d->list->currentRow();
  if (row < 0 || row >= d->session_ids.size()) {
    return;
  }

  auto session_id = d->session_ids[row];
  auto *menu = new QMenu(this);

  auto *resume_action = menu->addAction(tr("Resume"));
  connect(resume_action, &QAction::triggered, this,
          [this, session_id] { emit sessionResumeRequested(session_id); });

  auto *delete_action = menu->addAction(tr("Delete"));
  connect(delete_action, &QAction::triggered, this,
          [this, session_id] { emit sessionDeleteRequested(session_id); });

  menu->popup(d->list->mapToGlobal(pos));
}

}  // namespace mx::gui
