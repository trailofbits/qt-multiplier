// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentToolLogWidget.h"

#include <QDateTime>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardItemModel>
#include <QTreeView>
#include <QVBoxLayout>

namespace mx::gui {

struct AgentToolLogWidget::PrivateData {
  QTreeView *tree_view{nullptr};
  QStandardItemModel *model{nullptr};

  // Map from tool name to pending top-level item (for matching start/complete).
  QHash<QString, QStandardItem *> pending_calls;
};

AgentToolLogWidget::~AgentToolLogWidget(void) {}

AgentToolLogWidget::AgentToolLogWidget(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  d->model = new QStandardItemModel(0, 5, this);
  d->model->setHorizontalHeaderLabels(
      {tr("Time"), tr("Tool"), tr("Duration"), tr("Cost"), tr("Status")});

  d->tree_view = new QTreeView(this);
  d->tree_view->setModel(d->model);
  d->tree_view->setRootIsDecorated(true);
  d->tree_view->setAlternatingRowColors(true);
  d->tree_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  d->tree_view->header()->setStretchLastSection(false);
  d->tree_view->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  d->tree_view->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  d->tree_view->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  d->tree_view->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  d->tree_view->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

  layout->addWidget(d->tree_view);
}

void AgentToolLogWidget::onToolCallStarted(int64_t /*session_id*/,
                                            const QString &name,
                                            const QJsonObject &args) {
  auto time_str = QDateTime::currentDateTime().toString(
      QStringLiteral("HH:mm:ss"));

  auto *time_item = new QStandardItem(time_str);
  auto *name_item = new QStandardItem(name);
  auto *dur_item = new QStandardItem(QStringLiteral("..."));
  auto *cost_item = new QStandardItem(QStringLiteral("\xe2\x80\x94"));
  auto *status_item = new QStandardItem(QStringLiteral("running"));
  status_item->setForeground(QColor(0xF5, 0x9E, 0x0B));  // Amber.

  d->model->insertRow(0, {time_item, name_item, dur_item, cost_item,
                          status_item});
  auto row = time_item->row();

  // Add args as child row (truncated).
  if (!args.isEmpty()) {
    auto args_str = QString::fromUtf8(
        QJsonDocument(args).toJson(QJsonDocument::Compact));
    if (args_str.size() > 300) {
      args_str = args_str.left(300) + QStringLiteral("...(truncated)");
    }
    auto *args_item = new QStandardItem(
        QStringLiteral("Args: ") + args_str);
    time_item->appendRow({args_item, new QStandardItem,
                          new QStandardItem, new QStandardItem,
                          new QStandardItem});
  }

  d->pending_calls[name] = time_item;
  (void) row;
}

void AgentToolLogWidget::onToolCallCompleted(int64_t /*session_id*/,
                                              const QString &name,
                                              const QJsonObject &result,
                                              int duration_ms) {
  auto it = d->pending_calls.find(name);
  QStandardItem *time_item = nullptr;

  if (it != d->pending_calls.end()) {
    time_item = it.value();
    d->pending_calls.erase(it);
  }

  if (!time_item) {
    // No matching start -- create a standalone row at the top.
    auto time_str = QDateTime::currentDateTime().toString(
        QStringLiteral("HH:mm:ss"));
    time_item = new QStandardItem(time_str);
    d->model->insertRow(0,
        {time_item, new QStandardItem(name),
         new QStandardItem, new QStandardItem, new QStandardItem});
  }

  // Update duration with human-friendly formatting.
  auto row = time_item->row();
  auto *dur_item = d->model->item(row, 2);
  if (dur_item) {
    if (duration_ms < 1000) {
      dur_item->setText(QStringLiteral("%1ms").arg(duration_ms));
    } else if (duration_ms < 60000) {
      dur_item->setText(QStringLiteral("%1s")
          .arg(duration_ms / 1000.0, 0, 'f', 1));
    } else {
      auto mins = duration_ms / 60000;
      auto secs = (duration_ms % 60000) / 1000;
      dur_item->setText(QStringLiteral("%1m%2s").arg(mins).arg(secs));
    }
  }

  // Update status.
  bool has_error = result.contains(QStringLiteral("error"));
  auto *status_item = d->model->item(row, 4);
  if (status_item) {
    if (has_error) {
      status_item->setText(QStringLiteral("error"));
      status_item->setForeground(QColor(0xEF, 0x44, 0x44));  // Red.
    } else {
      status_item->setText(QStringLiteral("ok"));
      status_item->setForeground(QColor(0x22, 0xC5, 0x5E));  // Green.
    }
  }

  // Add result as child row (truncated).
  if (!result.isEmpty()) {
    auto result_str = QString::fromUtf8(
        QJsonDocument(result).toJson(QJsonDocument::Compact));
    if (result_str.size() > 500) {
      result_str = result_str.left(500) + QStringLiteral("...(truncated)");
    }
    auto label = has_error ? QStringLiteral("Error: ") + result_str
                           : QStringLiteral("Result: ") + result_str;
    auto *result_item = new QStandardItem(label);
    time_item->appendRow({result_item, new QStandardItem,
                          new QStandardItem, new QStandardItem,
                          new QStandardItem});
  }
}

void AgentToolLogWidget::clear(void) {
  d->model->removeRows(0, d->model->rowCount());
  d->pending_calls.clear();
}

}  // namespace mx::gui
