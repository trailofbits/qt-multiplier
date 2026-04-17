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
#include <QFont>
#include <QLabel>
#include <QMap>
#include <QPlainTextEdit>
#include <QTreeView>
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

// Role constants for item data.
static constexpr int kDocIdRole = Qt::UserRole + 1;
static constexpr int kFilterRole = Qt::UserRole + 2;
static constexpr int kCategoryRole = Qt::UserRole + 3;

// Known category keys in display order.
static const QStringList kCategoryOrder = {
    QStringLiteral("prompt"),
    QStringLiteral("reference"),
    QStringLiteral("skill"),
    QStringLiteral("note"),
    QStringLiteral("observer_notes"),
};

static QString category_display_name(const QString &key) {
  if (key == QStringLiteral("prompt")) return QStringLiteral("Prompts");
  if (key == QStringLiteral("reference")) return QStringLiteral("References");
  if (key == QStringLiteral("skill")) return QStringLiteral("Skills");
  if (key == QStringLiteral("note")) return QStringLiteral("Notes");
  if (key == QStringLiteral("observer_notes"))
    return QStringLiteral("Observer Notes");
  // Unknown category: capitalize first letter.
  if (key.isEmpty()) return QStringLiteral("Other");
  return key.at(0).toUpper() + key.mid(1);
}

static QString sanitize_filename(const QString &name) {
  QString result = name;
  static const QRegularExpression kBadChars(
      QStringLiteral("[/\\\\:*?\"<>|]"));
  result.replace(kBadChars, QStringLiteral("_"));
  result = result.trimmed();
  if (result.isEmpty()) result = QStringLiteral("document");
  return result;
}

static QString extension_for_format(const QString &format) {
  if (format == QStringLiteral("markdown") ||
      format == QStringLiteral("md")) {
    return QStringLiteral(".md");
  } else if (format == QStringLiteral("html")) {
    return QStringLiteral(".html");
  }
  return QStringLiteral(".txt");
}

// Delegate that renders HTML rich text in tree items.
class HtmlDelegate : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    // Folder items: use default painting (bold plain text).
    int doc_id = index.data(kDocIdRole).toInt();
    if (doc_id < 0) {
      QStyledItemDelegate::paint(painter, option, index);
      return;
    }

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
    int doc_id = index.data(kDocIdRole).toInt();
    if (doc_id < 0) {
      return QStyledItemDelegate::sizeHint(option, index);
    }

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

// Filter proxy that keeps folder items visible when any child matches.
class DocumentFilterProxy : public QSortFilterProxyModel {
 public:
  using QSortFilterProxyModel::QSortFilterProxyModel;

 protected:
  bool filterAcceptsRow(int source_row,
                        const QModelIndex &source_parent) const override {
    auto idx = sourceModel()->index(source_row, 0, source_parent);

    // Document item (has a valid doc_id): filter normally.
    int doc_id = idx.data(kDocIdRole).toInt();
    if (doc_id >= 0) {
      auto filter_text = idx.data(kFilterRole).toString();
      return filter_text.contains(filterRegularExpression());
    }

    // Folder item: accept if any child matches.
    int rows = sourceModel()->rowCount(idx);
    for (int i = 0; i < rows; ++i) {
      if (filterAcceptsRow(i, idx)) return true;
    }
    return false;
  }
};

struct DocumentExplorer::PrivateData {
  ConfigManager &config_manager;
  IWindowManager * const manager;

  IWindowWidget *dock{nullptr};
  QTreeView *tree{nullptr};
  QStandardItemModel *model{nullptr};
  DocumentFilterProxy *filter_proxy{nullptr};
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
  d->filter_proxy = new DocumentFilterProxy(d->dock);
  d->filter_proxy->setSourceModel(d->model);
  d->filter_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
  d->filter_proxy->setRecursiveFilteringEnabled(true);

  d->tree = new QTreeView(d->dock);
  d->tree->setModel(d->filter_proxy);
  d->tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
  d->tree->setSelectionMode(QAbstractItemView::SingleSelection);
  d->tree->setAlternatingRowColors(true);
  d->tree->setWordWrap(true);
  d->tree->setHeaderHidden(true);
  d->tree->setRootIsDecorated(true);
  d->tree->setItemDelegate(new HtmlDelegate(d->tree));

  // Click opens the document in the Documents viewer.
  connect(d->tree, &QTreeView::clicked,
          this, [this] (const QModelIndex &proxy_idx) {
    auto idx = d->filter_proxy->mapToSource(proxy_idx);
    int doc_id = d->model->data(idx, kDocIdRole).toInt();
    if (doc_id >= 0) {
      auto trigger = d->config_manager.ActionManager().Find(
          "com.trailofbits.action.OpenDocument");
      trigger.Trigger(QVariant(doc_id));
    }
  });

  // Right-click context menu.
  d->tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(d->tree, &QWidget::customContextMenuRequested,
          this, [this] (const QPoint &pos) {
    auto proxy_idx = d->tree->indexAt(pos);
    QMenu menu(d->tree);

    menu.addAction(tr("New Document"), this, [this] () {
      OnNewDocument({});
    });

    if (proxy_idx.isValid()) {
      auto idx = d->filter_proxy->mapToSource(proxy_idx);
      int doc_id = d->model->data(idx, kDocIdRole).toInt();
      auto category = d->model->data(idx, kCategoryRole).toString();

      if (doc_id < 0) {
        // Folder item context menu.
        menu.addSeparator();

        menu.addAction(
            tr("New Document in %1").arg(category_display_name(category)),
            this, [this, category] () {
          int new_id = d->config_manager.CreateDocument();
          if (new_id < 0) return;
          d->config_manager.SaveDocumentTitle(
              new_id, tr("Document %1").arg(new_id));
          d->config_manager.SetDocumentCategory(new_id, category);
          Refresh();
          d->dock->show();
          d->dock->EmitRequestAttention();
          d->config_manager.ActionManager().Find(
              "com.trailofbits.action.OpenDocument")
              .Trigger(QVariant(new_id));
        });

        menu.addAction(tr("Save All to Folder..."), this,
                       [this, category] () {
          auto dir = QFileDialog::getExistingDirectory(
              d->tree, tr("Save All Documents in %1")
                           .arg(category_display_name(category)));
          if (dir.isEmpty()) return;

          auto docs = d->config_manager.LoadDocumentsByCategory(category);
          for (const auto &doc : docs) {
            auto content = d->config_manager.LoadDocumentContent(doc.doc_id);
            auto format = d->config_manager.LoadDocumentFormat(doc.doc_id);
            auto ext = extension_for_format(format);
            auto filename = sanitize_filename(doc.title) + ext;
            QFile f(dir + QStringLiteral("/") + filename);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
              f.write(content.toUtf8());
            }
          }
        });
      } else {
        // Document item context menu.
        menu.addSeparator();

        menu.addAction(tr("Set Description..."), this,
                       [this, doc_id] () {
          auto docs = d->config_manager.LoadAllDocuments();
          QString desc;
          for (const auto &doc : docs) {
            if (doc.doc_id == doc_id) { desc = doc.description; break; }
          }

          QDialog dialog(d->tree);
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
          connect(buttons, &QDialogButtonBox::accepted,
                  &dialog, &QDialog::accept);
          connect(buttons, &QDialogButtonBox::rejected,
                  &dialog, &QDialog::reject);

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
          auto ext = extension_for_format(format);

          QString suggested = title.isEmpty()
              ? QStringLiteral("document_%1").arg(doc_id) + ext
              : title + ext;

          auto path = QFileDialog::getSaveFileName(
              d->tree, tr("Save Document"), suggested);
          if (path.isEmpty()) return;

          QFile file(path);
          if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
          auto content = d->config_manager.LoadDocumentContent(doc_id);
          file.write(content.toUtf8());
        });

        // "Move to..." submenu.
        auto *move_menu = menu.addMenu(tr("Move to..."));
        for (const auto &cat : kCategoryOrder) {
          if (cat == category) continue;
          move_menu->addAction(category_display_name(cat), this,
                               [this, doc_id, cat] () {
            d->config_manager.SetDocumentCategory(doc_id, cat);
            Refresh();
          });
        }

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
    }

    menu.exec(d->tree->mapToGlobal(pos));
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
    d->tree->expandAll();
  });

  auto *layout = new QVBoxLayout(d->dock);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(d->tree, 1);
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

  // Group documents by category.
  QMap<QString, QVector<ConfigManager::DocumentInfo>> by_category;
  for (const auto &doc : docs) {
    by_category[doc.category].push_back(doc);
  }

  // Build tree: one folder per category in display order, then any extras.
  QStringList ordered_keys;
  for (const auto &cat : kCategoryOrder) {
    ordered_keys.push_back(cat);
  }
  // Add any categories not in the known list.
  for (auto it = by_category.constBegin(); it != by_category.constEnd();
       ++it) {
    if (!ordered_keys.contains(it.key())) {
      ordered_keys.push_back(it.key());
    }
  }

  for (const auto &cat : ordered_keys) {
    auto it = by_category.constFind(cat);
    int count = (it != by_category.constEnd())
        ? static_cast<int>(it->size()) : 0;

    QString display = QStringLiteral("%1 (%2 %3)")
        .arg(category_display_name(cat))
        .arg(count)
        .arg(count == 1 ? tr("document") : tr("documents"));

    auto *folder = new QStandardItem(display);
    QFont font = folder->font();
    font.setBold(true);
    folder->setFont(font);
    folder->setData(-1, kDocIdRole);
    folder->setData(cat, kCategoryRole);
    folder->setData(display, kFilterRole);
    folder->setFlags(Qt::ItemIsEnabled);  // Not selectable.

    if (it != by_category.constEnd()) {
      for (const auto &doc : *it) {
        QString title = doc.title.isEmpty()
            ? tr("Document %1").arg(doc.doc_id) : doc.title;

        QString html = QStringLiteral("<b>%1</b>")
            .arg(title.toHtmlEscaped());
        if (!doc.description.isEmpty()) {
          html += QStringLiteral("<br>") + doc.description.toHtmlEscaped();
        }
        if (!doc.updated_at.isEmpty()) {
          auto dt = QDateTime::fromString(doc.updated_at, Qt::ISODate);
          if (dt.isValid()) {
            html += QStringLiteral(
                        "<br><small style=\"color:gray\">Updated: ")
                + dt.toLocalTime().toString(
                      QStringLiteral("yyyy-MM-dd hh:mm"))
                + QStringLiteral("</small>");
          }
        }

        auto *item = new QStandardItem(html);
        item->setData(doc.doc_id, kDocIdRole);
        item->setData(title + QStringLiteral(" ") + doc.description,
                      kFilterRole);
        item->setData(cat, kCategoryRole);
        item->setToolTip(title);
        folder->appendRow(item);
      }
    }

    d->model->appendRow(folder);
  }

  // Filter on title + description.
  d->filter_proxy->setFilterRole(kFilterRole);

  d->tree->expandAll();
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
