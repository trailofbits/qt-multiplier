// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.
#include <Python.h>
#include <py-multiplier/api.h>
#include "PythonOutputAdapter.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QTextEdit>

#include "Multiplier.h"

#include "PythonPromptView.h"

namespace mx::gui {

struct PythonPromptView::PrivateData {
  Multiplier& multiplier;

  QLineEdit* input_box;
  QTextEdit* output_box;
  QLabel* prompt_label;

  QStringList buffer;
  PyObject* compile;

  PrivateData(Multiplier& multiplier) : multiplier(multiplier) {}

  ~PrivateData() {
    Py_DECREF(compile);
  }
};

PythonPromptView::PythonPromptView(Multiplier& multiplier)
    : QWidget(&multiplier), d{std::make_unique<PrivateData>(multiplier)} {
  InitializeWidgets();
}

PythonPromptView::~PythonPromptView(void) {}

void PythonPromptView::InitializeWidgets(void) {
  auto vlayout = new QVBoxLayout;
  vlayout->setContentsMargins(0, 0, 0, 0);
  setLayout(vlayout);

  d->output_box = new QTextEdit;
  d->input_box = new QLineEdit;
  d->prompt_label = new QLabel(tr(">>>"));

  d->output_box->setReadOnly(true);

  QFont monospace{"Source Code Pro"};
  monospace.setStyleHint(QFont::TypeWriter);
  d->input_box->setFont(monospace);
  d->output_box->setFont(monospace);
  d->prompt_label->setFont(monospace);

  vlayout->addWidget(d->output_box, /*stretch=*/1);

  auto input_area = new QWidget();
  auto input_layout = new QHBoxLayout();
  input_area->setLayout(input_layout);
  input_layout->addWidget(d->prompt_label);
  input_layout->addWidget(d->input_box, /*stretch=*/1);

  vlayout->addWidget(input_area);

  setWindowTitle("Python Console");

  connect(d->input_box, &QLineEdit::returnPressed, this, &PythonPromptView::OnPromptEnter);
  connect(PythonOutputAdapter::StdOut, &PythonOutputAdapter::OnWrite, this, &PythonPromptView::OnStdOut);
  connect(PythonOutputAdapter::StdErr, &PythonOutputAdapter::OnWrite, this, &PythonPromptView::OnStdErr);

  auto codeop = PyImport_ImportModule("codeop");
  d->compile = PyObject_GetAttrString(codeop, "compile_command");

  QString welcome_string = QString("Python ") + Py_GetVersion() + " on " + Py_GetPlatform() + "\n";
  d->output_box->insertPlainText(welcome_string);
}

void PythonPromptView::OnPromptEnter() {
  auto group = QPalette::Disabled;
  auto role = QPalette::Text;

  auto palette = QApplication::palette();
  d->output_box->moveCursor(QTextCursor::End);

  auto prompt = d->prompt_label->text();
  auto input = d->input_box->text();

  d->output_box->setTextColor(palette.color(group, role));
  d->output_box->setFontItalic(true);
  d->output_box->insertPlainText(prompt + " " + input + "\n");
  d->input_box->clear();

  d->buffer << input;

  auto data = d->buffer.join('\n').toUtf8();

  auto args = Py_BuildValue("sss", data.data(), "<input>", "single");
  auto compiled = PyObject_Call(d->compile, args, nullptr);
  Py_DECREF(args);

  if(compiled == Py_None) {
    d->prompt_label->setText("...");
    return;
  }

  if(compiled) {
    auto env = PyModule_GetDict(PyImport_AddModule("__main__"));
    PyEval_EvalCode(compiled, env, env);
  }
  
  if(PyErr_Occurred()) {
    PyErr_Print();
    PyErr_Clear();
  }

  d->buffer.clear();
  d->prompt_label->setText(">>>");
}

void PythonPromptView::OnStdOut(const QString& str) {
  auto group = QPalette::Active;
  auto role = QPalette::Text;

  auto palette = QApplication::palette();
  d->output_box->moveCursor(QTextCursor::End);
  d->output_box->setTextColor(palette.color(group, role));
  d->output_box->setFontItalic(false);
  d->output_box->insertPlainText(str);
}

void PythonPromptView::OnStdErr(const QString& str) {
  d->output_box->moveCursor(QTextCursor::End);
  d->output_box->setTextColor(Qt::red);
  d->output_box->setFontItalic(false);
  d->output_box->insertPlainText(str);
}

void PythonPromptView::Connected() {
  auto dict = PyModule_GetDict(PyImport_AddModule("__main__"));
  auto index_obj = ::mx::py::CreateObject(::mx::Index{d->multiplier.EntityProvider()});
  PyDict_SetItemString(dict, "index", index_obj);
}

void PythonPromptView::Disconnected() {
  auto dict = PyModule_GetDict(PyImport_AddModule("__main__"));
  PyDict_DelItemString(dict, "index");
}

void PythonPromptView::CurrentFile(mx::RawEntityId id) {
  auto dict = PyModule_GetDict(PyImport_AddModule("__main__"));
  auto index = reinterpret_cast<mx::py::IndexWrapper*>(PyDict_GetItemString(dict, "index"));
  auto file = mx::py::CreateObject(index->index.entity(id));
  PyDict_SetItemString(dict, "current_file", file);
}

}  // namespace mx::gui
