/*
  Copyright (c) 2021-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/
#include <Python.h>
#include <py-multiplier/api.h>

#include "Multiplier.h"

#ifdef __APPLE__
#include "MacOSUtils.h"
#endif

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QScreen>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <multiplier/Index.h>
#include <tuple>
#include <vector>

#include "Configuration.h"
#include "CodeBrowserView.h"
#include "CodeTheme.h"
#include "FileBrowserView.h"
#include "FileView.h"
#include "HistoryBrowserView.h"
#include "IndexMonitorThread.h"
#include "OmniBoxView.h"
#include "PythonPromptView.h"
#include "ReferenceBrowserView.h"

#include <iostream>

namespace mx::gui {

enum class ConnectionState : int {
  kNotConnected,
  kConnecting,
  kConnectedNoIndex,
  kConnectedIndexing,
  kConnectedInitializing,
  kConnected
};

namespace {

struct MainMindowMenus final {

  QMenu *file_menu{nullptr};
  QAction *file_new_instance_action{nullptr};
  QAction *file_open_db_action{nullptr};
  QAction *import_database_action{nullptr};
  QAction *file_exit_action{nullptr};

  QMenu *view_menu{nullptr};
  QAction *view_reference_browser_action{nullptr};
  QAction *view_history_browser_action{nullptr};
  QAction *view_file_browser_action{nullptr};

  QMenu *help_menu{nullptr};
};

static FileLocationConfiguration LineNumConfig(const CodeTheme &theme) {
  FileLocationConfiguration config;
  config.tab_width = static_cast<unsigned>(theme.NumSpacesInTab());
  return config;
}

}  // namespace

struct Multiplier::PrivateData final {
  struct Configuration &config;

  MainMindowMenus menus;

  class CodeBrowserView *code_browser_view{nullptr};

  class FileBrowserView *file_browser_view{nullptr};
  QDockWidget *file_browser_dock{nullptr};

  class ReferenceBrowserView *reference_browser_view{nullptr};
  QDockWidget *reference_browser_dock{nullptr};

  class HistoryBrowserView *history_browser_view{nullptr};
  QDockWidget *history_browser_dock{nullptr};

  PythonPromptView *python_prompt_view{nullptr};
  QDockWidget *python_prompt_dock{nullptr};

  Qt::KeyboardModifiers modifiers;
  Qt::Key key{Qt::Key_unknown};
  Qt::MouseButtons buttons;
  Qt::MouseButtons double_click_buttons;
  MouseClickKind click_kind{MouseClickKind::kNotClicked};

  // The last-pressed locations.
  std::unordered_map<EventSource, EventLocations> last_locations;

  // The last user-caused event, excluding physical events.
  uint64_t last_event{0u};

  mx::EntityProvider::Ptr ep;
  mx::Index index;
  mx::FileLocationCache line_cache;

  IndexMonitorThread *monitor{nullptr};

  ConnectionState connection_state{ConnectionState::kNotConnected};

  inline PrivateData(::mx::gui::Configuration &config_)
      : config(config_),
        line_cache(LineNumConfig(config.theme ?
                                 *config.theme :
                                 CodeTheme::DefaultTheme())) {}
};

Multiplier::~Multiplier(void) {}

// Return the current configuration.
::mx::gui::Configuration &Multiplier::Configuration(void) const {
  return d->config;
}

// Return the current connected index.
const ::mx::Index &Multiplier::Index(void) const {
  return d->index;
}

// Return the current connected entity provider.
const ::mx::EntityProvider::Ptr &Multiplier::EntityProvider(void) const {
  return d->ep;
}

// Return the current code theme.
const ::mx::gui::CodeTheme &Multiplier::CodeTheme(void) const {
  if (d->config.theme) {
    return *(d->config.theme);
  } else {
    return CodeTheme::DefaultTheme();
  }
}

// Return a cache of pre-computed file locations.
const ::mx::FileLocationCache &Multiplier::FileLocationCache(void) const {
  return d->line_cache;
}

void Multiplier::paintEvent(QPaintEvent *event) {
  QString message;
  switch (d->connection_state) {
    case ConnectionState::kNotConnected:
      message = tr("Not connected.");
      break;
    case ConnectionState::kConnecting:
      message = tr("Connecting...");
      break;
    case ConnectionState::kConnectedNoIndex:
      message = tr("Ready for build importing.");
      break;
    case ConnectionState::kConnectedIndexing:
      message = tr("Indexing...");
      break;
    case ConnectionState::kConnectedInitializing:
      message = tr("Synchronizing with index...");
      break;
    case ConnectionState::kConnected:
      this->QWidget::paintEvent(event);
      return;
  }

  static const auto kTextFlags = Qt::AlignCenter | Qt::TextSingleLine;

  auto message_font = font();
  message_font.setPointSizeF(message_font.pointSizeF() * 2.0);
  message_font.setBold(true);

  QFontMetrics font_metrics(message_font);
  auto message_rect = font_metrics.boundingRect(QRect(0, 0, 0xFFFF, 0xFFFF),
                                                kTextFlags, message);

  const auto &event_rec = event->rect();
  auto message_x_pos = (event_rec.width() / 2) - (message_rect.width() / 2);
  auto message_y_pos = (event_rec.height() / 2) - (message_rect.height() / 2);

  message_rect.moveTo(message_x_pos, message_y_pos);

  QPainter painter(this);
  painter.fillRect(event_rec, palette().color(QPalette::Window));

  painter.setFont(message_font);
  painter.setPen(QPen(palette().color(QPalette::WindowText)));
  painter.drawText(message_rect, kTextFlags, message);

  event->accept();
}

void Multiplier::closeEvent(QCloseEvent *event) {
  if (d->connection_state != ConnectionState::kNotConnected) {
    auto answer = QMessageBox::question(
        this, tr("Question"), tr("Are you sure you want to exit the program?"),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      event->ignore();
      return;
    }
  }

  event->accept();
}

#ifdef Q_OS_MAC
#  define IF_APPLE_ELSE(a, b) a
#else
#  define IF_APPLE_ELSE(a, b) b
#endif

bool Multiplier::eventFilter(QObject *watched, QEvent *event) {

  switch (const QEvent::Type et = event->type(); et) {
    case QEvent::KeyPress:
      d->last_event += 1;
      d->key = Qt::Key_unknown;
      d->click_kind = MouseClickKind::kNotClicked;
      switch (auto key = dynamic_cast<QKeyEvent *>(event)->key(); key) {
        case Qt::Key_Meta:
          d->modifiers.setFlag(Qt::KeyboardModifier::ControlModifier);
          break;
        case Qt::Key_Shift:
          d->modifiers.setFlag(Qt::KeyboardModifier::ShiftModifier);
          break;
        case Qt::Key_Alt:
        case Qt::Key_Option:
          d->modifiers.setFlag(Qt::KeyboardModifier::AltModifier);
          break;
        case Qt::Key_Control:
          d->modifiers.setFlag(Qt::KeyboardModifier::MetaModifier);
          break;
        default:
          d->key = static_cast<Qt::Key>(key);
          break;
      }
      break;

    case QEvent::KeyRelease:
      d->last_event += 1;
      switch (auto key = dynamic_cast<QKeyEvent *>(event)->key(); key) {
        case Qt::Key_Meta:
          d->modifiers.setFlag(Qt::KeyboardModifier::ControlModifier, false);
          break;
        case Qt::Key_Shift:
          d->modifiers.setFlag(Qt::KeyboardModifier::ShiftModifier, false);
          break;
        case Qt::Key_Alt:
        case Qt::Key_Option:
          d->modifiers.setFlag(Qt::KeyboardModifier::AltModifier, false);
          break;
        case Qt::Key_Control:
          d->modifiers.setFlag(Qt::KeyboardModifier::MetaModifier, false);
          break;
        default:
          if (d->key == key) {
            d->key = Qt::Key_unknown;
          }
          break;
      }
      break;

    case QEvent::MouseButtonPress:
    case QEvent::NonClientAreaMouseButtonPress:
    case QEvent::GraphicsSceneMousePress: {
      d->last_event += 1;
      QMouseEvent *me = dynamic_cast<QMouseEvent *>(event);
      const Qt::MouseButton button = me->button();
      d->buttons.setFlag(button, true);
      d->double_click_buttons = {};
      d->click_kind = MouseClickKind::kNotClicked;
      ClearLastLocations();
      return false;
    }

    case QEvent::MouseButtonRelease:
    case QEvent::NonClientAreaMouseButtonRelease:
    case QEvent::GraphicsSceneMouseRelease: {
      d->last_event += 1;
      QMouseEvent *me = dynamic_cast<QMouseEvent *>(event);
      const Qt::MouseButton button = me->button();
      d->click_kind = MouseClickKind::kNotClicked;
      if (d->buttons.testFlag(button)) {
        d->buttons.setFlag(button, false);
        if (button == Qt::MouseButton::LeftButton) {
          d->click_kind = MouseClickKind::kLeftClick;
        } else if (button == Qt::MouseButton::RightButton) {
          d->click_kind = MouseClickKind::kRightClick;
        }
      } else if (d->double_click_buttons.testFlag(button)) {
        d->double_click_buttons.setFlag(button, false);
        if (button == Qt::MouseButton::LeftButton) {
          d->click_kind = MouseClickKind::kLeftDoubleClick;
        } else if (button == Qt::MouseButton::RightButton) {
          d->click_kind = MouseClickKind::kRightDoubleClick;
        }
      }
      break;
    }
    case QEvent::MouseButtonDblClick:
    case QEvent::NonClientAreaMouseButtonDblClick:
    case QEvent::GraphicsSceneMouseDoubleClick: {
      d->last_event += 1;
      QMouseEvent *me = dynamic_cast<QMouseEvent *>(event);
      d->double_click_buttons.setFlag(me->button(), true);
      break;
    }

    default:
      return false;
  }

  auto ret = EmitEvent();
  d->key = Qt::Key_unknown;
  d->click_kind = MouseClickKind::kNotClicked;
  return ret;
}

Multiplier::Multiplier(struct Configuration &config_)
    : QMainWindow(nullptr),
      d(new PrivateData(config_)) {
  InitializeUI();
  UpdateUI();
}

void Multiplier::InitializeWidgets(void) {
  installEventFilter(this);

  d->code_browser_view = new CodeBrowserView(*this);

  d->file_browser_view = new FileBrowserView(d->config.file_browser);
  d->file_browser_dock = new QDockWidget(d->file_browser_view->windowTitle());
  d->file_browser_dock->setWidget(d->file_browser_view);

  d->reference_browser_view = new ReferenceBrowserView(*this);
  d->reference_browser_dock = new QDockWidget(d->reference_browser_view->windowTitle());
  d->reference_browser_dock->setWidget(d->reference_browser_view);

  d->history_browser_view = new HistoryBrowserView(*this);
  d->history_browser_dock = new QDockWidget(d->history_browser_view->windowTitle());
  d->history_browser_dock->setWidget(d->history_browser_view);

  d->python_prompt_view = new PythonPromptView(*this);
  d->python_prompt_dock = new QDockWidget(d->python_prompt_view->windowTitle());
  d->python_prompt_dock->setWidget(d->python_prompt_view);

  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::East);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::North);

  addDockWidget(Qt::LeftDockWidgetArea, d->file_browser_dock);
  addDockWidget(Qt::LeftDockWidgetArea, d->history_browser_dock);
  addDockWidget(Qt::LeftDockWidgetArea, d->reference_browser_dock);
  addDockWidget(Qt::BottomDockWidgetArea, d->python_prompt_dock);
  tabifyDockWidget(d->file_browser_dock, d->history_browser_dock);
  tabifyDockWidget(d->history_browser_dock, d->reference_browser_dock);

  setCentralWidget(d->code_browser_view);

#ifdef __APPLE__
  if (getenv("MX_NO_CUSTOM_THEME") == nullptr) {
    setTitleBarColor(winId(), palette().color(QPalette::Window), false);
  }
#endif

  connect(d->history_browser_dock, &QDockWidget::visibilityChanged,
          this, &Multiplier::FocusOnHistory);

  connect(d->file_browser_view, &FileBrowserView::Connected,
          this, &Multiplier::OnConnected);

  connect(d->file_browser_view, &FileBrowserView::SourceFileDoubleClicked,
          this, &Multiplier::OnSourceFileDoubleClicked);

  connect(d->reference_browser_dock, &QDockWidget::dockLocationChanged,
          this, &Multiplier::OnMoveReferenceBrowser);

  connect(d->history_browser_view, &HistoryBrowserView::TokenPressEvent,
          this, &Multiplier::ActOnTokenPressEvent);

  connect(d->code_browser_view, &CodeBrowserView::CurrentFile,
          d->python_prompt_view, &PythonPromptView::CurrentFile);

  connect(d->python_prompt_view, &PythonPromptView::SourceFileOpened,
          this, &Multiplier::OnSourceFileDoubleClicked);

  connect(d->python_prompt_view, &PythonPromptView::TokenOpened,
          d->code_browser_view, &CodeBrowserView::OnScrollToToken);
}

void Multiplier::InitializeMenus(void) {
  //
  // File menu
  //

  d->menus.file_open_db_action = new QAction(tr("Open database"));
  connect(d->menus.file_open_db_action, &QAction::triggered,
          this, &Multiplier::OnFileOpenDatabaseAction);

  d->menus.file_exit_action = new QAction(tr("Exit"));
  connect(d->menus.file_exit_action, &QAction::triggered,
          this, &Multiplier::OnFileExitAction);

  d->menus.view_file_browser_action = new QAction(tr("File Browser"));
  connect(d->menus.view_file_browser_action, &QAction::triggered,
          this, &Multiplier::OnViewFileBrowserAction);

  d->menus.view_reference_browser_action = new QAction(tr("Reference Browser"));
  connect(d->menus.view_reference_browser_action, &QAction::triggered,
          this, &Multiplier::OnViewReferenceBrowserAction);

  d->menus.view_history_browser_action = new QAction(tr("History Browser"));
  connect(d->menus.view_history_browser_action, &QAction::triggered,
          this, &Multiplier::OnViewHistoryBrowserAction);

  d->menus.file_menu = menuBar()->addMenu(tr("File"));

  d->menus.file_menu->addAction(d->menus.file_open_db_action);

  if (d->config.indexer_exe_path.size()) {
    d->menus.import_database_action = new QAction(tr("Import project into database"));
    connect(d->menus.import_database_action, &QAction::triggered,
            this, &Multiplier::OnFileImportIntoDatabaseAction);
    d->menus.file_menu->addAction(d->menus.import_database_action);
  }

  d->menus.file_menu->addSeparator();
  d->menus.file_menu->addAction(d->menus.file_exit_action);

  d->menus.view_menu = menuBar()->addMenu(tr("View"));
  d->menus.view_menu->addAction(d->menus.view_reference_browser_action);
  d->menus.view_menu->addAction(d->menus.view_history_browser_action);
  d->menus.view_menu->addAction(d->menus.view_file_browser_action);
}

void Multiplier::UpdateMenus(void) {
  bool is_disconnected = d->connection_state == ConnectionState::kNotConnected;
  bool is_connected = d->connection_state == ConnectionState::kConnected;
  d->menus.import_database_action->setEnabled(is_disconnected);
  d->menus.view_reference_browser_action->setEnabled(is_connected);
  d->menus.view_history_browser_action->setEnabled(is_connected);
  d->menus.view_file_browser_action->setEnabled(is_connected);
}

void Multiplier::UpdateWidgets(void) {
  switch (d->connection_state) {
    case ConnectionState::kNotConnected:
      d->file_browser_view->Clear();
      d->history_browser_view->Clear();
      d->reference_browser_view->Clear();
      d->code_browser_view->Clear();
      d->file_browser_dock->hide();
      d->history_browser_dock->hide();
      d->reference_browser_dock->hide();
      d->python_prompt_dock->hide();
      d->code_browser_view->Disconnected();
      break;

    case ConnectionState::kConnected:
      d->file_browser_dock->show();
      d->history_browser_dock->show();
      d->reference_browser_dock->show();
      d->python_prompt_dock->show();
      d->code_browser_view->Connected();
      break;

    default:
      d->file_browser_dock->hide();
      d->history_browser_dock->hide();
      d->reference_browser_dock->hide();
      d->python_prompt_dock->hide();
      d->code_browser_view->Disconnected();
      break;
  }
}

void Multiplier::UpdateUI(void) {
  UpdateMenus();
  UpdateWidgets();
  update();
}

void Multiplier::InitializeUI(void) {
  setWindowTitle("Multiplier");
  QRect rect = QGuiApplication::primaryScreen()->geometry();

  resize(rect.width(), rect.height());

  ClearLastLocations();
  InitializeWidgets();
  InitializeMenus();
}

void Multiplier::ClearLastLocations(void) {
  d->last_locations.clear();

  // Fill in with empty stuff. The way `EmitActions` works is that it relies
  // on the event sources being present.
  std::ignore = d->last_locations[EventSource::kReferenceBrowserPreviewClickSource];
  std::ignore = d->last_locations[EventSource::kReferenceBrowserPreviewClickDest];
  std::ignore = d->last_locations[EventSource::kReferenceBrowser];
  std::ignore = d->last_locations[EventSource::kCodeBrowserClickSource];
  std::ignore = d->last_locations[EventSource::kCodeBrowserClickDest];
  std::ignore = d->last_locations[EventSource::kHistoryBrowserVisualItemSelected];
  std::ignore = d->last_locations[EventSource::kHistoryBrowserLinearItemChanged];
  std::ignore = d->last_locations[EventSource::kCodeSearchResult];
  std::ignore = d->last_locations[EventSource::kCodeSearchResultPreviewClickSource];
  std::ignore = d->last_locations[EventSource::kCodeSearchResultPreviewClickDest];
  std::ignore = d->last_locations[EventSource::kEntitySearchResult];
  std::ignore = d->last_locations[EventSource::kEntityIDSearchResultSource];
  std::ignore = d->last_locations[EventSource::kEntityIDSearchResultDest];
}

void Multiplier::OnMoveReferenceBrowser(Qt::DockWidgetArea area) {
  UpdateUI();
  switch (area) {
    case Qt::DockWidgetArea::LeftDockWidgetArea:
    case Qt::DockWidgetArea::RightDockWidgetArea:
      d->reference_browser_view->SetCodePreviewVertical();
      break;
    default:
      d->reference_browser_view->SetCodePreviewHorizontal();
      break;
  }
}

void Multiplier::FocusOnHistory(bool visible) {
  if (visible) {
    d->history_browser_view->Focus();
  }
}

void Multiplier::OnConnected(void) {
  d->connection_state = ConnectionState::kConnected;
  UpdateUI();
}

void Multiplier::OnOpenTab(QString title, QWidget *widget) {
  widget->setWindowTitle(title);
  d->code_browser_view->OpenCustom(title, widget);
}

void Multiplier::OnOpenDock(QString title, QWidget *widget) {
  QDockWidget *custom_dock = new QDockWidget(title);
  widget->setWindowTitle(title);
  custom_dock->setWidget(widget);
  addDockWidget(Qt::LeftDockWidgetArea, custom_dock);
  custom_dock->setAttribute(Qt::WA_DeleteOnClose);
}

void Multiplier::OnSourceFileDoubleClicked(
    std::filesystem::path path, RawEntityId file_id) {
  d->code_browser_view->OpenFile(std::move(path), file_id, true);
}

void Multiplier::OnVersionNumberChanged(::mx::Index index_) {
  d->index = std::move(index_);

  auto version_number = d->index.version_number();
  if (!version_number) {
    return;

  } else if (version_number == 1) {
    d->connection_state = ConnectionState::kConnectedNoIndex;
    UpdateUI();

  } else if (!(version_number % 2)) {
    d->connection_state = ConnectionState::kConnectedIndexing;
    UpdateUI();

  } else {
    d->connection_state = ConnectionState::kConnectedInitializing;
    UpdateUI();

    auto downloader = new DownloadFileListThread(Index());
    downloader->setAutoDelete(true);

    connect(downloader, &DownloadFileListThread::DownloadedFileList,
            d->file_browser_view, &FileBrowserView::OnDownloadedFileList);

    connect(downloader, &DownloadFileListThread::DownloadedFileList,
            d->reference_browser_view, &ReferenceBrowserView::OnDownloadedFileList);

    connect(downloader, &DownloadFileListThread::DownloadedFileList,
            d->history_browser_view, &HistoryBrowserView::OnDownloadedFileList);

    connect(downloader, &DownloadFileListThread::DownloadedFileList,
            d->code_browser_view, &CodeBrowserView::OnDownloadedFileList);

    connect(downloader, &DownloadFileListThread::DownloadedFileList,
            d->code_browser_view->OmniBox(), &OmniBoxView::OnDownloadedFileList);

    QThreadPool::globalInstance()->start(downloader);
  }
}

void Multiplier::OnFileImportIntoDatabaseAction(void) {
  using namespace std::filesystem;

  auto db_str = QFileDialog::getSaveFileName(this, tr("Choose database"));
  if(!db_str.size()) {
    return;
  }

  auto bin_str = QFileDialog::getOpenFileName(this, tr("Choose binary"));
  if(!bin_str.size()) {
    return;
  }
  
  auto db_path = absolute(path(db_str.toStdString()));
  auto bin_path = absolute(path(bin_str.toStdString()));

  QStringList arguments;
  
  // SQLite database file.
  arguments.push_back("--db");
  arguments.push_back(db_path.c_str());
  
  // Target file (JSON compilation database, binary) to import.
  arguments.push_back("--target");
  arguments.push_back(bin_path.c_str());

  auto process = new QProcess(this);
  connect(
      process,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      [=](int code, QProcess::ExitStatus status) {
        Open(db_path);

        process->disconnect();
        process->deleteLater();
      });

  connect(process, &QProcess::errorOccurred, [=](QProcess::ProcessError error) {
    d->connection_state = ConnectionState::kNotConnected;

    process->disconnect();
    process->deleteLater();
  });

  process->start(d->config.indexer_exe_path, arguments);
  d->connection_state = ConnectionState::kConnectedIndexing;
  UpdateUI();
}

void Multiplier::Open(std::filesystem::path db) {
  d->connection_state = ConnectionState::kConnected;
  UpdateUI();

  d->ep = EntityProvider::in_memory_cache(EntityProvider::from_database(db));
  d->monitor = new IndexMonitorThread(d->ep);

  connect(d->monitor, &IndexMonitorThread::VersionNumberChanged,
          this, &Multiplier::OnVersionNumberChanged);

  d->monitor->Start();
  emit IndexReady();
}

void Multiplier::OnFileOpenDatabaseAction(void) {
  QString file_str = QFileDialog::getOpenFileName(this, tr("Open database"));
  if (!file_str.size()) {
    return;
  }

  std::filesystem::path file_path = file_str.toStdString();

  // Get the full path to the workspace.
  std::error_code ec;
  auto full_file_path = std::filesystem::absolute(file_path, ec);
  if (ec) {
    (void) QMessageBox::warning(
        this, tr("Read error"),
        tr("Could not locate file %1: %2")
            .arg(file_str)
            .arg(QString::fromStdString(ec.message())),
        QMessageBox::Ok);
    return;
  }

  Open(full_file_path);
}

void Multiplier::OnFileExitAction(void) { close(); }

void Multiplier::OnViewFileBrowserAction(void) {
  d->file_browser_dock->setEnabled(true);
  d->file_browser_dock->toggleViewAction()->setChecked(true);
  d->file_browser_dock->setVisible(true);
  d->file_browser_view->Focus();
}

void Multiplier::OnViewReferenceBrowserAction(void) {
  d->reference_browser_dock->setEnabled(true);
  d->reference_browser_dock->toggleViewAction()->setChecked(true);
  d->reference_browser_dock->setVisible(true);
  d->reference_browser_view->Focus();
}

void Multiplier::OnViewHistoryBrowserAction(void) {
  d->history_browser_dock->setEnabled(true);
  d->history_browser_dock->toggleViewAction()->setChecked(true);
  d->history_browser_dock->setVisible(true);
  d->history_browser_view->Focus();
}

void Multiplier::OnHelpAboutAction(void) {}

bool Multiplier::DoActions(EventSource source, const EventAction &ea) {
  const EventLocations &locs = d->last_locations[source];

  if ((ea.match_sources & source) != EventSources(source)) {
    return false;
  }

  if (ea.last_triggered >= d->last_event) {
    return false;
  }

  auto has_locs = !locs.IsEmpty();

  switch (ea.do_action) {
    case Action::kDoNothing:
      break;
    case Action::kOpenCodeBrowser:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->code_browser_view->OpenEntities(locs);
        return true;
      } else {
        return false;
      }
    case Action::kOpenReferenceBrowser:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->reference_browser_view->SetRoots(locs);
        if (d->reference_browser_dock->visibleRegion().isEmpty()) {
          d->reference_browser_dock->raise();
        }
        return true;
      } else {
        return false;
      }
    case Action::kAddToVisualHistoryAsChild:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->history_browser_view->AddChildDeclarations(locs);
        return true;
      } else {
        return false;
      }
    case Action::kAddToVisualHistoryAsSibling:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->history_browser_view->AddSiblingDeclarations(locs);
        return true;
      } else {
        return false;
      }
    case Action::kAddToVisualHistoryUnderRoot:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->history_browser_view->AddDeclarationsUnderRoot(locs);
        return true;
      } else {
        return false;
      }
    case Action::kAddToVisualHistoryAsRoots:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->history_browser_view->AddRootDeclarations(locs);
        return true;
      } else {
        return false;
      }
    case Action::kAddToLinearHistory:
      if (has_locs) {
        ea.last_triggered = d->last_event;
        d->history_browser_view->AddToLinearHistory(locs);
        return true;
      } else {
        return false;
      }
    case Action::kGoBackLinearHistory:
      ea.last_triggered = d->last_event;
      return d->history_browser_view->GoBackInLinearHistory();

    case Action::kOpenRegexSearch:
      ea.last_triggered = d->last_event;
      d->code_browser_view->OpenRegexSearch();
      return true;

    case Action::kOpenEntitySearch:
      ea.last_triggered = d->last_event;
      d->code_browser_view->OpenEntitySearch();
      return true;

    case Action::kOpenSymbolQuerySearch:
      ea.last_triggered = d->last_event;
      d->code_browser_view->OpenSymbolQuerySearch();
      return true;

    case Action::kOpenWeggliSearch:
      ea.last_triggered = d->last_event;
      d->code_browser_view->OpenWeggliSearch();
      return true;

    case Action::kOpenSyntexSearch:
      ea.last_triggered = d->last_event;
      d->code_browser_view->OpenSyntexSearch();
      return true;
  }

  return false;
}

bool Multiplier::EmitEvent(void) {
  auto acted = false;
  for (const EventAction &ea : d->config.actions) {
    if (ea.match_modifiers != d->modifiers) {
      continue;
    }

    if (ea.match_key != d->key) {
      continue;
    }

    if (ea.match_click != d->click_kind) {
      continue;
    }

    for (const auto &[source, locs] : d->last_locations) {
      if (DoActions(source, ea)) {
        acted = true;
      }
    }
  }

  return acted;
}

void Multiplier::ActOnTokenPressEvent(EventSource source, EventLocations locs) {
  d->last_locations[source] = std::move(locs);
//  d->key = Qt::Key_unknown;
  d->click_kind = MouseClickKind::kNotClicked;
  for (const EventAction &ea : d->config.immediate_actions) {
    DoActions(source, ea);
  }
}

void Multiplier::SetSingleEntityGlobal(const QString& name, mx::RawEntityId id) {
  auto entity = d->index.entity(id);
  d->python_prompt_view->SetGlobal(name, py::CreateObject(std::move(entity)));
  d->python_prompt_view->OnLineEntered(name);
}

void Multiplier::SetMultipleEntitiesGlobal(const QString& name, const std::vector<mx::RawEntityId>& ids) {
  auto list = PyList_New(ids.size());
  for(size_t i = 0; i < ids.size(); ++i) {
    auto entity = d->index.entity(ids[i]);
    PyList_SetItem(list, i, py::CreateObject(std::move(entity)));
  }
  d->python_prompt_view->SetGlobal(name, list);
  d->python_prompt_view->OnLineEntered(name);
}

}  // namespace mx::gui
