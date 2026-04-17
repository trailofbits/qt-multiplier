// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <multiplier/GUI/Explorers/DocumentExplorer.h>

#include <QAction>
#include <cmath>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QListView>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QIODevice>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <multiplier/GUI/Interfaces/IWindowManager.h>
#include <multiplier/GUI/Interfaces/IWindowWidget.h>
#include <multiplier/GUI/Managers/ActionManager.h>
#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/MediaManager.h>
#include <multiplier/GUI/Widgets/SearchWidget.h>
#include <multiplier/GUI/Widgets/SimpleTextInputDialog.h>
#include <multiplier/GUI/Widgets/SpreadsheetModel.h>

namespace mx::gui {

static const QString kDocMimeType =
    QStringLiteral("application/x-qtmultiplier-document");

// Delegate that renders HTML rich text in list items.
class HtmlDelegate : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    auto *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter,
                         opt.widget);

    QTextDocument doc;
    doc.setHtml(opt.text);
    doc.setTextWidth(opt.rect.width() - 4);

    painter->save();
    painter->translate(opt.rect.left() + 2, opt.rect.top() + 2);
    doc.drawContents(painter);
    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    QTextDocument doc;
    doc.setHtml(opt.text);

    // Use the widget's width if available, otherwise a reasonable default.
    int width = opt.rect.width();
    if (width <= 0 && opt.widget) {
      width = opt.widget->width();
    }
    if (width <= 0) {
      width = 300;
    }
    doc.setTextWidth(width - 4);

    return QSize(width,
                 static_cast<int>(std::ceil(doc.size().height())) + 6);
  }
};

struct DocumentExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  QListView *list{nullptr};
  QStandardItemModel *model{nullptr};
  QSortFilterProxyModel *filter_proxy{nullptr};
  SearchWidget *search_widget{nullptr};

  inline PrivateData(ConfigManager &config_manager_, IWindowManager *manager_)
      : config_manager(config_manager_),
        manager(manager_) {}
};

DocumentExplorer::~DocumentExplorer(void) {}

DocumentExplorer::DocumentExplorer(ConfigManager &config_manager,
                                   IWindowManager *parent)
    : IMainWindowPlugin(config_manager, parent),
      d(new PrivateData(config_manager, parent)) {

  auto &action_manager = config_manager.ActionManager();
  auto &media_manager = config_manager.MediaManager();

  auto new_doc_trigger = action_manager.Register(
      this, "com.trailofbits.action.NewDocument",
      &DocumentExplorer::OnNewDocument);

  // Other explorers can trigger this to refresh the list.
  action_manager.Register(
      this, QStringLiteral("com.trailofbits.action.RefreshDocuments"),
      [this] (const QVariant &) { Refresh(); });

  // Toolbar button.
  NamedAction new_doc_named_action;
  new_doc_named_action.name = tr("New Document");
  new_doc_named_action.action = new_doc_trigger;
  parent->AddToolBarButton(
      media_manager.Icon("com.trailofbits.icon.NewDocument"),
      new_doc_named_action);

  CreateDockWidget(parent);

  // Load documents when a project is opened.
  connect(&config_manager, &ConfigManager::IndexChanged,
          this, &DocumentExplorer::OnIndexChanged);

  // Refresh when the agent modifies documents externally.
  connect(&config_manager, &ConfigManager::ExternalDocumentsChanged,
          this, &DocumentExplorer::Refresh);

  OnIndexChanged(config_manager);
}

void DocumentExplorer::CreateDockWidget(IWindowManager *manager) {
  d->dock = new IWindowWidget;
  d->dock->setWindowTitle(tr("Document Explorer"));
  d->dock->setContentsMargins(0, 0, 0, 0);

  d->model = new QStandardItemModel(d->dock);
  d->filter_proxy = new QSortFilterProxyModel(d->dock);
  d->filter_proxy->setSourceModel(d->model);
  d->filter_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

  d->list = new QListView(d->dock);
  d->list->setModel(d->filter_proxy);
  d->list->setEditTriggers(QAbstractItemView::NoEditTriggers);
  d->list->setSelectionMode(QAbstractItemView::SingleSelection);
  d->list->setAlternatingRowColors(true);
  d->list->setWordWrap(true);
  d->list->setItemDelegate(new HtmlDelegate(d->list));

  // Click opens the document in the Documents viewer.
  connect(d->list, &QListView::clicked,
          this, [this] (const QModelIndex &proxy_idx) {
    auto idx = d->filter_proxy->mapToSource(proxy_idx);
    int doc_id = d->model->data(idx, Qt::UserRole + 1).toInt();
    if (doc_id >= 0) {
      // Trigger open in document viewer via the spreadsheet explorer's action.
      auto trigger = d->config_manager.ActionManager().Find(
          "com.trailofbits.action.OpenDocument");
      trigger.Trigger(QVariant(doc_id));
    }
  });

  // Right-click context menu.
  d->list->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->list, &QWidget::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    auto proxy_idx = d->list->indexAt(pos);
    QMenu menu(d->list);

    menu.addAction(tr("New Document"), this, [this] () {
      OnNewDocument({});
    });

    if (proxy_idx.isValid()) {
      auto idx = d->filter_proxy->mapToSource(proxy_idx);
      int doc_id = d->model->data(idx, Qt::UserRole + 1).toInt();

      menu.addSeparator();

      menu.addAction(tr("Set Description..."), this,
                     [this, doc_id] () {
        // Load existing description.
        auto docs = d->config_manager.LoadAllDocuments();
        QString desc;
        for (const auto &doc : docs) {
          if (doc.doc_id == doc_id) { desc = doc.description; break; }
        }

        QDialog dialog(d->list);
        dialog.setWindowTitle(tr("Document Description"));
        dialog.resize(400, 200);
        auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(tr("Enter a description:"), &dialog));
        auto *edit = new QPlainTextEdit(&dialog);
        edit->setPlainText(desc);
        edit->setTabChangesFocus(true);
        layout->addWidget(edit, 1);
        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() == QDialog::Accepted) {
          d->config_manager.SaveDocumentDescription(
              doc_id, edit->toPlainText());
          Refresh();
          d->config_manager.ActionManager().Find(
              "com.trailofbits.action.RefreshDocuments").Trigger({});
        }
      });

      menu.addAction(tr("Copy"), this, [doc_id] () {
        auto *mime = new QMimeData;
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << static_cast<qint32>(doc_id);
        mime->setData(kDocMimeType, data);
        mime->setText(QStringLiteral("[doc:%1]").arg(doc_id));
        QApplication::clipboard()->setMimeData(mime);
      });

      menu.addAction(tr("Save as..."), this,
                     [this, doc_id] () {
        auto title = d->config_manager.LoadDocumentTitle(doc_id);
        auto format = d->config_manager.LoadDocumentFormat(doc_id);

        // Choose a default extension based on the document format.
        QString extension;
        if (format == QStringLiteral("markdown") ||
            format == QStringLiteral("md")) {
          extension = QStringLiteral(".md");
        } else if (format == QStringLiteral("html")) {
          extension = QStringLiteral(".html");
        } else {
          extension = QStringLiteral(".txt");
        }

        QString suggested = title.isEmpty()
            ? QStringLiteral("document_%1").arg(doc_id) + extension
            : title + extension;

        auto path = QFileDialog::getSaveFileName(
            d->list, tr("Save Document"), suggested);
        if (path.isEmpty()) return;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        auto content = d->config_manager.LoadDocumentContent(doc_id);
        file.write(content.toUtf8());
      });

      menu.addSeparator();

      int ref_count = d->config_manager.DocumentReferenceCount(doc_id);
      auto *delete_action = menu.addAction(
          ref_count > 0
              ? tr("Delete (referenced by %1 cell(s))").arg(ref_count)
              : tr("Delete"));
      connect(delete_action, &QAction::triggered, this,
              [this, doc_id] () {
        d->config_manager.SoftDeleteDocument(doc_id);
        Refresh();
      });
    }

    menu.exec(d->list->mapToGlobal(pos));
  });

  // Search filter.
  auto &media_manager = d->config_manager.MediaManager();
  d->search_widget = new SearchWidget(
      media_manager, SearchWidget::Mode::Filter, d->dock);

  connect(d->search_widget, &SearchWidget::SearchParametersChanged,
          this, [this] () {
    auto &params = d->search_widget->Parameters();
    auto pattern = QString::fromStdString(params.pattern);
    if (params.type == SearchWidget::SearchParameters::Type::Text) {
      pattern = QRegularExpression::escape(pattern);
    }
    QRegularExpression::PatternOptions opts{
        QRegularExpression::NoPatternOption};
    if (!params.case_sensitive) {
      opts |= QRegularExpression::CaseInsensitiveOption;
    }
    d->filter_proxy->setFilterRegularExpression(
        QRegularExpression(pattern, opts));
  });

  auto *layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->list, 1);
  layout->addWidget(d->search_widget);
  d->dock->setLayout(layout);

  IWindowManager::DockConfig config;
  config.id = "com.trailofbits.dock.DocumentExplorer";
  config.location = IWindowManager::DockLocation::Left;
  config.tabify = true;
  config.start_hidden = true;
  config.app_menu_location = {tr("View"), tr("Explorers")};
  manager->AddDockWidget(d->dock, config);
}

void DocumentExplorer::OnNewDocument(const QVariant &) {
  int doc_id = d->config_manager.CreateDocument();
  if (doc_id < 0) return;

  d->config_manager.SaveDocumentTitle(
      doc_id, tr("Document %1").arg(doc_id));
  Refresh();

  // Show the document explorer.
  d->dock->show();
  d->dock->EmitRequestAttention();

  // Open the new document in the viewer.
  d->config_manager.ActionManager().Find(
      "com.trailofbits.action.OpenDocument")
      .Trigger(QVariant(doc_id));
}

void DocumentExplorer::Refresh(void) {
  d->model->clear();

  auto docs = d->config_manager.LoadAllDocuments();
  for (const auto &doc : docs) {
    QString title = doc.title.isEmpty()
        ? tr("Document %1").arg(doc.doc_id) : doc.title;

    // Build HTML display: bold title, description, small date.
    QString html = QStringLiteral("<b>%1</b>")
        .arg(title.toHtmlEscaped());
    if (!doc.description.isEmpty()) {
      html += QStringLiteral("<br>") + doc.description.toHtmlEscaped();
    }
    if (!doc.updated_at.isEmpty()) {
      auto dt = QDateTime::fromString(doc.updated_at, Qt::ISODate);
      if (dt.isValid()) {
        html += QStringLiteral("<br><small style=\"color:gray\">Updated: ")
            + dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm"))
            + QStringLiteral("</small>");
      }
    }

    auto *item = new QStandardItem(html);
    item->setData(doc.doc_id, Qt::UserRole + 1);
    // Store title separately for filtering.
    item->setData(title + QStringLiteral(" ") + doc.description,
                  Qt::UserRole + 2);
    item->setToolTip(title);
    d->model->appendRow(item);
  }

  // Filter on title + description.
  d->filter_proxy->setFilterRole(Qt::UserRole + 2);
}

void DocumentExplorer::OnIndexChanged(const ConfigManager &) {
  Refresh();
}

void DocumentExplorer::ActOnPrimaryClick(
    IWindowManager *, const QModelIndex &) {
}

void DocumentExplorer::ActOnContextMenu(
    IWindowManager *, QMenu *, const QModelIndex &) {
}

}  // namespace mx::gui
