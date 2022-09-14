// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

// This needs to be included before any Qt stuff because of AUTOMOC/UIC macros
#include <Python.h>
#include <py-multiplier/api.h>
#include <structmember.h>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QFontDatabase>
#include <QMetaType>
#include <QPixmap>
#include <QSplashScreen>

#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <multiplier/Index.h>
#include <system_error>
#include <tuple>
#include <variant>


#include "CodeView.h"
#include "Configuration.h"
#include "Multiplier.h"
#include "ReferenceBrowserView.h"
#include "PythonOutputAdapter.h"

namespace {
  static PyObject* fake_read(PyObject* self, PyObject* args) {
    return Py_BuildValue("s", "");
  }

  static PyObject* fake_readline(PyObject* self, PyObject* args) {
    return Py_BuildValue("s", "\n");
  }

  static PyMethodDef fake_methods[] = {
    {"read", fake_read, METH_VARARGS, ""},
    {"readline", fake_readline, METH_VARARGS, ""},
    {}};

  static PyMemberDef fake_members[] = {{}};

  static PyTypeObject fake_stdin = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "_multiplier.FakeStdin",
    .tp_doc = PyDoc_STR(""),
    .tp_basicsize = sizeof(PyObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_members = fake_members,
    .tp_methods = fake_methods,
  };
}

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);
  application.setApplicationName("Multiplier");

  QCommandLineParser parser;
  QCommandLineOption host_option("host");
  host_option.setValueName("host");
  QCommandLineOption port_option("port");
  port_option.setValueName("port");
  parser.addOption(host_option);
  parser.addOption(port_option);
  parser.process(application);

  Py_Initialize();

  auto mod = PyImport_AddModule("__main__");
  PyObject_SetAttrString(mod, "multiplier", mx::py::PyInit__multiplier());

  std::unique_ptr<int, void(*)(int*)> python_release(new int, [](int* x) {
    Py_FinalizeEx();
  });

  mx::gui::PythonOutputAdapter::InitPythonType();
  mx::gui::PythonOutputAdapter::StdOut = new mx::gui::PythonOutputAdapter();
  mx::gui::PythonOutputAdapter::StdErr = new mx::gui::PythonOutputAdapter();

  auto stdout_obj = mx::gui::PythonOutputAdapter::StdOut->GetInstance();
  auto stderr_obj = mx::gui::PythonOutputAdapter::StdErr->GetInstance();

  PyObject *sys = PyImport_ImportModule("sys");
  PyObject_SetAttrString(sys, "stdout", stdout_obj);
  PyObject_SetAttrString(sys, "stderr", stderr_obj);

  PyType_Ready(&fake_stdin);
  Py_INCREF(&fake_stdin);
  
  auto stdin_obj = fake_stdin.tp_alloc(&fake_stdin, 0);
  PyObject_SetAttrString(sys, "stdin", stdin_obj);
  Py_DECREF(sys);

  QSplashScreen splash_screen(QPixmap(":/Icons/appicon"));
  splash_screen.show();
  application.processEvents();

#define ADD_FONT(path) \
    do { \
      auto ret = QFontDatabase::addApplicationFont(path); \
      assert(ret != -1); \
      std::ignore = ret; \
    } while (0)

  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Black.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-BlackItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Bold.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-BoldItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraBold.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraBoldItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraLight.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-ExtraLightItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Italic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Light.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-LightItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Medium.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-MediumItalic.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-Regular.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-SemiBold.ttf");
  ADD_FONT(":/Fonts/Fonts/Source_Code_Pro/static/SourceCodePro-SemiBoldItalic.ttf");

  mx::gui::Configuration config;

  if (getenv("MX_NO_CUSTOM_THEME") == nullptr) {
    qApp->setStyle(config.style);
    QPalette dark_palette;
    dark_palette.setColor(QPalette::Window, QColor(53,53,53));
    dark_palette.setColor(QPalette::WindowText, QColor(240, 240, 240));
    dark_palette.setColor(QPalette::Base, QColor(35, 35, 35));
    dark_palette.setColor(QPalette::AlternateBase, QColor(53,53,53));
    dark_palette.setColor(QPalette::ToolTipBase, QColor(240, 240, 240));
    dark_palette.setColor(QPalette::ToolTipText, QColor(240, 240, 240));
    dark_palette.setColor(QPalette::Text, QColor(240, 240, 240));
    dark_palette.setColor(QPalette::Button, QColor(53,53,53));
    dark_palette.setColor(QPalette::ButtonText, QColor(240, 240, 240));
    dark_palette.setColor(QPalette::BrightText, Qt::red);
    dark_palette.setColor(QPalette::Link, QColor(42, 130, 218));

    dark_palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    dark_palette.setColor(QPalette::HighlightedText, Qt::white);

    qApp->setPalette(dark_palette);
  }

  // Per Josh Hofing:
  //
  //    You spend a lot of time clicking stuff to go deeper and deeper while
  //    auditing, so that should be as easy as possible.
  //
  //    There's a finite number of times that I'll be able to click in my life
  //    before I get arthritis, so I don't want to halve it.

  // Click to open something in the code browser view, or in the code preview
  // view of the reference browser, should open the thing in the code browser.
  config.actions.emplace_back(mx::gui::EventAction{
    .description = "A clicked token should be added to the linear history",
    .match_click = mx::gui::MouseClickKind::kLeftClick,
    .match_sources = {mx::gui::EventSource::kCodeBrowserClickSource,
                      mx::gui::EventSource::kReferenceBrowserPreviewClickSource,
                      mx::gui::EventSource::kCodeSearchResultPreviewClickSource},
    .do_action = mx::gui::Action::kAddToLinearHistory,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "The code browser should show a clicked entity",
    .match_click = mx::gui::MouseClickKind::kLeftClick,
    .match_sources = {mx::gui::EventSource::kCodeBrowserClickDest,
                      mx::gui::EventSource::kReferenceBrowserPreviewClickSource,
                      mx::gui::EventSource::kCodeSearchResultPreviewClickSource},
    .do_action = mx::gui::Action::kOpenCodeBrowser,
  });

//  config.actions.emplace_back(mx::gui::EventAction{
//    .description = "A double clicked declaration search result should be added "
//                   "to the linear history",
//    .match_click = mx::gui::MouseClickKind::kLeftDoubleClick,
//    .match_sources = {mx::gui::EventSource::kEntitySearchResult},
//    .do_action = mx::gui::Action::kAddToLinearHistory,
//  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "A double clicked declaration search result should be added "
                   "to the visual history",
    .match_click = mx::gui::MouseClickKind::kLeftDoubleClick,
    .match_sources = {mx::gui::EventSource::kEntitySearchResult},
    .do_action = mx::gui::Action::kAddToVisualHistoryUnderRoot,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "The code browser should show a double clicked entity result",
    .match_click = mx::gui::MouseClickKind::kLeftDoubleClick,
    .match_sources = {mx::gui::EventSource::kEntitySearchResult,
                      mx::gui::EventSource::kEntityIDSearchResultSource},
    .do_action = mx::gui::Action::kOpenCodeBrowser,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "A clicked declaration should be added to the visual history",
    .match_click = mx::gui::MouseClickKind::kLeftClick,
    .match_sources = {mx::gui::EventSource::kCodeBrowserClickDest},
    .do_action = mx::gui::Action::kAddToVisualHistoryUnderRoot,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "A meta-click on an entity should open up the references view",
    .match_click = mx::gui::MouseClickKind::kLeftClick,
    .match_modifiers = {Qt::KeyboardModifier::MetaModifier},
    .match_sources = ~mx::gui::EventSources(),
    .do_action = mx::gui::Action::kOpenReferenceBrowser,
  });

//  config.actions.emplace_back(mx::gui::EventAction{
//    .description = "A double click in the reference browser adds to the linear history",
//    .match_click = mx::gui::MouseClickKind::kLeftDoubleClick,
//    .match_sources = {mx::gui::EventSource::kReferenceBrowser},
//    .do_action = mx::gui::Action::kAddToLinearHistory,
//  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "A double click in the reference browser adds to the visual history",
    .match_click = mx::gui::MouseClickKind::kLeftDoubleClick,
    .match_sources = {mx::gui::EventSource::kReferenceBrowser},
    .do_action = mx::gui::Action::kAddToVisualHistoryUnderRoot,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "A double click in the reference browser shows the used code in the code browser",
    .match_click = mx::gui::MouseClickKind::kLeftDoubleClick,
    .match_sources = {mx::gui::EventSource::kReferenceBrowser},
    .do_action = mx::gui::Action::kOpenCodeBrowser,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "Pressing ESC should go back in linear history",
    .match_key = Qt::Key_Escape,
    .match_sources = ~mx::gui::EventSources(),
    .do_action = mx::gui::Action::kGoBackLinearHistory,
  });

  // This is to handle `ESC` to go back.
  config.immediate_actions.emplace_back(mx::gui::EventAction{
    .description = "A change in the current active history item should be shown in the code browser",
    .match_sources = {mx::gui::EventSource::kHistoryBrowserLinearItemChanged,
                      mx::gui::EventSource::kHistoryBrowserVisualItemSelected},
    .do_action = mx::gui::Action::kOpenCodeBrowser,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "F should open the RE2 search.",
    .match_key = Qt::Key_F,
    .match_sources = ~mx::gui::EventSources(),
    .do_action = mx::gui::Action::kOpenRegexSearch,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "W should open the Weggli search.",
    .match_key = Qt::Key_W,
    .match_sources = ~mx::gui::EventSources(),
    .do_action = mx::gui::Action::kOpenWeggliSearch,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "G should open the entity search by name.",
    .match_key = Qt::Key_G,
    .match_sources = ~mx::gui::EventSources(),
    .do_action = mx::gui::Action::kOpenSymbolQuerySearch,
  });

  config.actions.emplace_back(mx::gui::EventAction{
    .description = "I should open the entity search by ID.",
    .match_key = Qt::Key_I,
    .match_sources = ~mx::gui::EventSources(),
    .do_action = mx::gui::Action::kOpenEntitySearch,
  });

  qRegisterMetaType<mx::NamedDeclList>("NamedDeclList");
  qRegisterMetaType<mx::DeclCategory>("DeclCategory");
  qRegisterMetaType<mx::gui::EventLocation>("EventLocation");
  qRegisterMetaType<mx::gui::EventLocations>("EventLocations");
  qRegisterMetaType<mx::gui::EventSource>("EventSource");
  qRegisterMetaType<mx::gui::EventSources>("EventSources");
  qRegisterMetaType<uint8_t>("uint8_t");
  qRegisterMetaType<uint16_t>("uint16_t");
  qRegisterMetaType<uint32_t>("uint32_t");
  qRegisterMetaType<uint64_t>("uint64_t");
  qRegisterMetaType<std::optional<mx::Decl>>("std::optional<Decl>");
  qRegisterMetaType<std::optional<mx::Stmt>>("std::optional<Stmt>");
  qRegisterMetaType<std::optional<mx::Type>>("std::optional<Type>");
  qRegisterMetaType<std::optional<mx::Token>>("std::optional<Token>");
  qRegisterMetaType<mx::gui::UserLocationsPtr>("UserLocationsPtr");
  qRegisterMetaType<mx::gui::UserLocations>("UserLocations");
  qRegisterMetaType<mx::RawEntityId>("RawEntityId");
  qRegisterMetaType<mx::EntityId>("EntityId");
  qRegisterMetaType<std::optional<mx::VariantEntity>>("std::optional<VariantEntity>");
  qRegisterMetaType<mx::FilePathList>("FilePathList");
  qRegisterMetaType<mx::Token>("Token");
  qRegisterMetaType<mx::TokenRange>("TokenRange");
  qRegisterMetaType<mx::TokenList>("TokenList");
  qRegisterMetaType<mx::Index>("Index");
  qRegisterMetaType<mx::Index>("::mx::Index");
  qRegisterMetaType<mx::Index>("mx::Index");
  qRegisterMetaType<std::optional<mx::File>>("std::optional<File>");
  qRegisterMetaType<std::optional<mx::Fragment>>("std::optional<Fragment>");
  qRegisterMetaType<std::optional<mx::Decl>>("std::optional<Decl>");
  qRegisterMetaType<std::optional<mx::Stmt>>("std::optional<Stmt>");
  qRegisterMetaType<std::optional<mx::Type>>("std::optional<Type>");
  qRegisterMetaType<std::optional<mx::Token>>("std::optional<Token>");
  qRegisterMetaType<std::vector<mx::Fragment>>("std::vector<Fragment>");
  qRegisterMetaType<std::vector<mx::RawEntityId>>("std::vector<RawEntityId>");

  mx::gui::Multiplier main_window(config);
  main_window.show();

  if (parser.isSet(host_option) && parser.isSet(port_option)) {
    main_window.Connect(parser.value(host_option),
                        parser.value(port_option));
  }

  splash_screen.finish(&main_window);

  return application.exec();
}
