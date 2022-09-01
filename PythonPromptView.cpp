// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.
#include <Python.h>
#include <py-multiplier/api.h>
#include "PythonOutputAdapter.h"
#include <structmember.h>

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

  mx::RawEntityId current_file{};

  PrivateData(Multiplier& multiplier) : multiplier(multiplier) {}

  ~PrivateData() {
    Py_DECREF(compile);
  }
};

struct PythonPromptView::Wrapper final {
  PyObject_HEAD
  PythonPromptView* view;

  Multiplier& Multiplier() {
    return view->d->multiplier;
  }

  static PyObject* get_index(PyObject* self, void*) {
    auto me = reinterpret_cast<Wrapper*>(self);
    auto &ep = me->Multiplier().EntityProvider();
    return mx::py::CreateObject(mx::Index{ep});
  }

  static PyObject* get_current_file(PyObject* self, void*) {
    auto me = reinterpret_cast<Wrapper*>(self);
    auto &index = me->Multiplier().Index();
    return mx::py::CreateObject(index.entity(me->view->d->current_file));
  }

  static PyGetSetDef *GetGetSetDefs() {
    static PyGetSetDef props[] = {
      {"index", get_index, nullptr,
       "The index to which Multiplier is connected", nullptr},
      {"current_file", get_current_file, nullptr,
       "The file that's currently selected", nullptr},
      {}};

    return props;
  }

  static PyMemberDef *GetMemberDefs() {
    static PyMemberDef members[] = {{}};

    return members;
  }

  static PyObject* open_entity(PyObject* self, PyObject* arg) {
    auto me = reinterpret_cast<Wrapper*>(self);
    mx::VariantEntity entity{mx::NotAnEntity{}};

    if(PyLong_Check(arg)) {
      auto id = PyLong_AsSize_t(arg);
      entity = me->Multiplier().Index().entity(id);
    }

    if(PyObject_IsInstance(arg,
         reinterpret_cast<PyObject*>(py::GetFileType()))) {
      auto wrapper = reinterpret_cast<py::EntityWrapper<mx::File>*>(arg);
      entity = wrapper->entity;
    } else if(PyObject_IsInstance(arg,
                reinterpret_cast<PyObject*>(py::GetTokenType()))) {
      auto wrapper = reinterpret_cast<py::EntityWrapper<mx::Token>*>(arg);
      entity = wrapper->entity;
    } else if(PyObject_IsInstance(arg,
                reinterpret_cast<PyObject*>(py::GetDeclType()))) {
      auto wrapper = reinterpret_cast<py::EntityWrapper<mx::Decl>*>(arg);
      entity = wrapper->entity;
    } else if(PyObject_IsInstance(arg,
                reinterpret_cast<PyObject*>(py::GetAttrType()))) {
      auto wrapper = reinterpret_cast<py::EntityWrapper<mx::Attr>*>(arg);
      entity = wrapper->entity;
    } else {
        PyErr_SetString(PyExc_TypeError,
          "Can only open entities of type File, Token, Decl or Attr");
        return nullptr;
    }

    return Py_NewRef(me->view->Open(entity) ? Py_True : Py_False);
  }

  static PyObject* print_html(PyObject* self, PyObject* args) {
    auto me = reinterpret_cast<Wrapper*>(self);
    const char* str;
    if(!PyArg_ParseTuple(args, "s", &str)) {
      return nullptr;
    }
    me->view->d->output_box->moveCursor(QTextCursor::End);
    me->view->d->output_box->insertHtml(str);
    Py_RETURN_NONE;
  }

  static PyMethodDef *GetMethodDefs() {
    static PyMethodDef methods[] = {
      {"open_entity", open_entity, METH_O, "Opens an entity in the GUI"},
      {"print_html", print_html, METH_VARARGS, "Prints HTML to the console"},
      {}};

    return methods;
  }

  static PyTypeObject* GetTypeObject() {
    static PyTypeObject type = {
      PyVarObject_HEAD_INIT(NULL, 0)
      .tp_name = "_multiplier.GUI",
      .tp_doc = "Allows programmatic interaction with the Multiplier GUI",
      .tp_basicsize = sizeof(Wrapper),
      .tp_itemsize = 0,
      .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
      .tp_members = GetMemberDefs(),
      .tp_methods = GetMethodDefs(),
      .tp_getset = GetGetSetDefs()
    };

    return &type;
  }
};

PythonPromptView::PythonPromptView(Multiplier& multiplier)
    : QWidget(&multiplier), d{std::make_unique<PrivateData>(multiplier)} {
  PyType_Ready(Wrapper::GetTypeObject());
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

  connect(d->input_box, &QLineEdit::returnPressed,
    this, &PythonPromptView::OnPromptEnter);

  connect(PythonOutputAdapter::StdOut, &PythonOutputAdapter::OnWrite,
    this, &PythonPromptView::OnStdOut);

  connect(PythonOutputAdapter::StdErr, &PythonOutputAdapter::OnWrite,
    this, &PythonPromptView::OnStdErr);

  auto mod = PyImport_AddModule("__main__");
  auto gui_obj = PyObject_NEW(Wrapper, Wrapper::GetTypeObject());
  gui_obj->view = this;
  PyObject_SetAttrString(mod, "gui", reinterpret_cast<PyObject*>(gui_obj));

  auto codeop = PyImport_ImportModule("codeop");
  d->compile = PyObject_GetAttrString(codeop, "compile_command");

  QString welcome_string =
    QString("Python %1 on %2\n").arg(Py_GetVersion(), Py_GetPlatform());
  d->output_box->insertPlainText(welcome_string);
}

void PythonPromptView::OnLineEntered(const QString& s) {
  auto group = QPalette::Disabled;
  auto role = QPalette::Text;

  auto palette = QApplication::palette();
  d->output_box->moveCursor(QTextCursor::End);

  auto prompt = d->prompt_label->text();

  d->output_box->setTextColor(palette.color(group, role));
  d->output_box->setFontItalic(true);
  d->output_box->insertPlainText(prompt + " " + s + "\n");

  d->buffer << s;

  auto utf8 = d->buffer.join('\n').toUtf8();

  auto args = Py_BuildValue("sss", utf8.data(), "<input>", "single");
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

void PythonPromptView::OnPromptEnter() {
  auto input = d->input_box->text();
  d->input_box->clear();
  OnLineEntered(input);
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

void PythonPromptView::CurrentFile(mx::RawEntityId id) {
  d->current_file = id;
}

bool PythonPromptView::Open(const mx::VariantEntity& entity) {
  auto GetFilePath = [&](const std::optional<mx::File>& file) ->
                         std::optional<std::filesystem::path> {
    if(!file) {
      return {};
    }

    auto paths = d->multiplier.Index().file_paths();
    for(auto [path, id] : paths) {
      if(id == file->id()) {
        return path;
      }
    }
    return {};
  };

  mx::Token token;
  if(std::holds_alternative<mx::File>(entity)) {
    std::filesystem::path path;
    auto& file{std::get<mx::File>(entity)};
    if(auto path = GetFilePath(file)) {
      emit SourceFileOpened(*path, file.id());
      return true;
    }
  } else if(std::holds_alternative<mx::Token>(entity)) {
    token = std::get<mx::Token>(entity);
  } else if(std::holds_alternative<mx::Decl>(entity)) {
    auto& decl = std::get<mx::Decl>(entity);
    token = decl.token();
  } else if(std::holds_alternative<mx::Attr>(entity)) {
    auto& attr = std::get<mx::Attr>(entity);
    token = attr.token();
  } else {
    return false;
  }

  auto file = mx::File::containing(token);
  if(auto path = GetFilePath(file)) {
    emit TokenOpened(*path, file->id(), token.id());
    return true;
  }

  return false;
}

void PythonPromptView::SetGlobal(const QString& name, PyObject* obj) {
  auto mod = PyImport_AddModule("__main__");
  PyObject_SetAttrString(mod, name.toUtf8().data(), obj);
}

}  // namespace mx::gui
