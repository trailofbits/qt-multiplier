// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <Python.h>

#include <multiplier/GUI/Widgets/PythonConsoleWidget.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QThreadPool>

#include <multiplier/Bindings/Python.h>
#include <multiplier/Index.h>
#include <multiplier/GUI/ThemeManager.h>

#include "PythonCodeRunner.h"
#include "PythonCompletionModel.h"
#include "PythonOutputAdapter.h"

extern "C" PyObject *PyInit_multiplier(void);

namespace mx::gui {

struct PythonConsoleWidget::PrivateData {
  Index index;
  CodeViewTheme theme;
  QPalette palette;

  QLineEdit *input_box{nullptr};
  QTextEdit *output_box{nullptr};
  QLabel *prompt_label{nullptr};

  PyObject *main_module{nullptr};
  PyObject *sys_module{nullptr};

  QStringList buffer;
  PyObject *compile{nullptr};

  PythonCompletionModel *completion_model{nullptr};
  PyObject *prev_stdout{nullptr};
  PyObject *prev_stderr{nullptr};
  PythonOutputAdapter *stdout{nullptr};
  PythonOutputAdapter *stderr{nullptr};
  QCompleter *completer{nullptr};

  QThreadPool thread_pool;

  QStringList history;
  typename QStringList::const_iterator current_history_position;

  // The input that was in the LineEdit before the user started navigating
  // the history.
  QString original_input;

  void InputBoxArrowUp(void);
  void InputBoxArrowDown(void);
  bool InputBoxFilter(QEvent *event);
  bool CompleterFilter(QEvent *event);

  PrivateData(const Index &index_)
    : index(index_) {

    Py_Initialize();
    main_module = PyImport_AddModule("__main__");

    // Emulate a `from multiplier import *`. The code completion module pulls
    // its initial completions out of `__main__`, so we want to ensure it has
    // been populated.
    auto mx_module = PyInit_multiplier();
    auto mx_module_dict = PyModule_GetDict(mx_module);
    Py_ssize_t i = 0;
    PyObject *key = nullptr;
    PyObject *val = nullptr;
    while (PyDict_Next(mx_module_dict, &i, &key, &val)) {
      Py_INCREF(val);
      if (PyObject_SetAttr(main_module, key, val)) {
        PyErr_Clear();
        Py_DECREF(val);
      }
    }
    Py_DECREF(mx_module);

    // Set the `index` variable to be the index that we've connected to.
    auto index_obj = ::mx::to_python<::mx::Index>(index);
    assert(index_obj != nullptr);
    PyObject_SetAttrString(main_module, "index", index_obj);
    PyErr_Clear();

    // Bring in the `codeop` module and get the `compile_command` function so
    // that we can emulate the interactive python shell, and try to compile
    // complete or partial code into executable code objects. If we give it
    // partial code objects, then we change our prompt from `>>>` to `...`, just
    // like the Python REPL.
    auto codeop = PyImport_ImportModule("codeop");
    compile = PyObject_GetAttrString(codeop, "compile_command");
    Py_DECREF(codeop);

    // Interceptor objects for `sys.stdout` and `sys.stderr`.
    stdout = new PythonOutputAdapter;
    stderr = new PythonOutputAdapter;
    sys_module = PyImport_ImportModule("sys");
    prev_stdout = PyObject_GetAttrString(sys_module, "stdout");
    prev_stderr = PyObject_GetAttrString(sys_module, "stderr");
    PyObject_SetAttrString(sys_module, "stdout", stdout->GetInstance());
    PyObject_SetAttrString(sys_module, "stderr", stderr->GetInstance());
  }

  ~PrivateData(void) {
    thread_pool.waitForDone();
    delete completion_model;
    delete stdout;
    delete stderr;
    PyObject_SetAttrString(sys_module, "stdout", prev_stdout);
    PyObject_SetAttrString(sys_module, "stderr", prev_stderr);
    Py_DECREF(sys_module);
    Py_DECREF(main_module);
    Py_Finalize();
  }
};

void PythonConsoleWidget::PrivateData::InputBoxArrowUp(void) {
  if(current_history_position == history.begin()) {
    return; // Already at the top
  }

  if(current_history_position == history.end()) {
    // Starting history scroll, need to save current input
    original_input = input_box->text();
  }

  --current_history_position;
  input_box->setText(*current_history_position);
}

void PythonConsoleWidget::PrivateData::InputBoxArrowDown(void) {
  
  // Already at the bottom, don't let us go past it.
  if(current_history_position == history.end()) {
    return;
  }

  ++current_history_position;

  // Reached the bottom, restore original input.
  if(current_history_position == history.end()) {
    input_box->setText(original_input);
    return;
  }

  input_box->setText(*current_history_position);
}

bool PythonConsoleWidget::PrivateData::InputBoxFilter(QEvent *event) {
  if(event->type() != QEvent::KeyPress) {
    return false;
  }

  auto key_event = static_cast<QKeyEvent*>(event);
  if(key_event->key() == Qt::Key_Up) {
    InputBoxArrowUp();
    return true;
  }

  if(key_event->key() == Qt::Key_Down) {
    InputBoxArrowDown();
    return true;
  }

  if(key_event->key() != Qt::Key_Tab) {
    return false;
  }

  completion_model->enableSuggestions();
  completer->complete();
  return true;
}

bool PythonConsoleWidget::PrivateData::CompleterFilter(QEvent *event) {
  if(event->type() != QEvent::KeyPress) {
    return false;
  }

  auto key_event = static_cast<QKeyEvent*>(event);
  if(key_event->key() != Qt::Key_Tab) {
    return false;
  }

  return true;
}

PythonConsoleWidget::PythonConsoleWidget(const Index &index_, QWidget *parent)
    : QWidget(parent),
      d(std::make_unique<PrivateData>(index_)) {
  
  InitializeModel();
  InitializeWidgets();
  SetHere(NotAnEntity{});
}

PythonConsoleWidget::~PythonConsoleWidget(void) {}

void PythonConsoleWidget::InitializeModel(void) {
  d->completion_model = new PythonCompletionModel(this);

  connect(d->stdout, &PythonOutputAdapter::OnWrite,
          this, &PythonConsoleWidget::OnStdOut);

  connect(d->stderr, &PythonOutputAdapter::OnWrite,
          this, &PythonConsoleWidget::OnStdErr);
}

void PythonConsoleWidget::SetHere(VariantEntity entity) {
  PyObject_SetAttrString(
    d->main_module, "here", ::mx::to_python<VariantEntity>(std::move(entity)));
}

void PythonConsoleWidget::SetTheme(const QPalette &palette,
                                   const CodeViewTheme &theme) {
  d->theme = theme;
  d->palette = palette;

  QFont font(d->theme.font_name);
  font.setStyleHint(QFont::TypeWriter);

  d->input_box->setFont(font);
  d->output_box->setFont(font);
  d->prompt_label->setFont(font);

  ResetFontColor();
}

void PythonConsoleWidget::InitializeWidgets(void) {
  auto vlayout = new QVBoxLayout;
  vlayout->setContentsMargins(0, 0, 0, 0);
  setLayout(vlayout);

  d->output_box = new QTextEdit;
  d->input_box = new QLineEdit;
  d->prompt_label = new QLabel(">>>");

  d->output_box->setReadOnly(true);

  auto &theme_manager = ThemeManager::Get();
  SetTheme(QApplication::palette(), theme_manager.GetCodeViewTheme());

  connect(&theme_manager, &ThemeManager::ThemeChanged,
          this, &PythonConsoleWidget::SetTheme);

  vlayout->addWidget(d->output_box, /*stretch=*/1);

  auto input_area = new QWidget();
  auto input_layout = new QHBoxLayout();
  input_area->setLayout(input_layout);
  input_layout->addWidget(d->prompt_label);
  input_layout->addWidget(d->input_box, /*stretch=*/1);

  d->completer = new QCompleter(d->completion_model, this);
  d->completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
  d->completer->setCompletionRole(Qt::DisplayRole);
  d->completer->setModelSorting(QCompleter::UnsortedModel);
  d->input_box->setCompleter(d->completer);

  vlayout->addWidget(input_area);

  setWindowTitle("Python Console");

  connect(d->input_box, &QLineEdit::returnPressed,
          this, &PythonConsoleWidget::OnPromptEnter);

  connect(d->input_box, &QLineEdit::textEdited,
          d->completion_model, &PythonCompletionModel::setPrefix);

  d->input_box->installEventFilter(this);
  d->input_box->completer()->popup()->installEventFilter(this);

  QString welcome_string =
      QString("Python %1 on %2\n").arg(Py_GetVersion(), Py_GetPlatform());
  d->output_box->insertPlainText(welcome_string);

  d->current_history_position = d->history.end();
}

void PythonConsoleWidget::OnLineEntered(const QString &s) {
  auto completer_view{d->input_box->completer()->popup()};
  if(completer_view->isVisible()) {
    return;
  }

  auto group = QPalette::Disabled;
  auto role = QPalette::Text;

  d->output_box->setTextColor(d->palette.color(group, role));
  d->output_box->setFontItalic(true);
  d->output_box->moveCursor(QTextCursor::End);

  auto prompt = d->prompt_label->text();
  d->output_box->insertPlainText(prompt + " " + s + "\n");

  d->history.push_back(s);
  d->current_history_position = d->history.end();

  d->buffer << s;

  auto utf8 = d->buffer.join('\n').toUtf8();

  PyErr_Clear();
  auto code = PyObject_CallFunction(
      d->compile, "sss", utf8.data(), "<input>", "single");
  
  if (code == Py_None) {
    Py_DECREF(code);
    d->prompt_label->setText("...");
    return;
  
  } else if (code) {
    d->input_box->setEnabled(false);
    d->prompt_label->setText("~~~");
    d->buffer.clear();

    // Run the compiled code on another thread so that it doesn't block us.
    auto env = PyModule_GetDict(d->main_module);
    Py_INCREF(env);
    auto runner = new PythonCodeRunner(code, env);
    
    connect(runner, &PythonCodeRunner::EvaluationDone,
            this, &PythonConsoleWidget::OnEvaluationDone);

    d->thread_pool.start(runner);
  
  } else if (PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
    d->prompt_label->setText(">>>");
    d->buffer.clear();
  }
}

void PythonConsoleWidget::OnEvaluationDone(void) {
  d->prompt_label->setText(">>>");
  d->input_box->setEnabled(true);
  d->input_box->setFocus();
}

void PythonConsoleWidget::OnPromptEnter(void) {
  auto input = d->input_box->text();
  d->input_box->clear();
  OnLineEntered(input);
}

void PythonConsoleWidget::ResetFontColor(void) {
  auto group = QPalette::Active;
  auto role = QPalette::Text;
  d->output_box->setTextColor(d->palette.color(group, role));
}

void PythonConsoleWidget::OnStdOut(const QString &str) {
  d->output_box->moveCursor(QTextCursor::End);
  d->output_box->setFontItalic(false);
  d->output_box->insertPlainText(str);
}

void PythonConsoleWidget::OnStdErr(const QString &str) {
  d->output_box->moveCursor(QTextCursor::End);
  d->output_box->setTextColor(Qt::red);
  d->output_box->setFontItalic(false);
  d->output_box->insertPlainText(str);
  ResetFontColor();
}

bool PythonConsoleWidget::eventFilter(QObject *sender, QEvent *event) {
  if(sender == d->input_box) {
    return d->InputBoxFilter(event);
  }

  if(sender == d->input_box->completer()->popup()) {
    return d->CompleterFilter(event);
  }

  return false;
}

}  // namespace mx::gui
