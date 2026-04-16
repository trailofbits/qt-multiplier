// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentConfigPanel.h"

#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/LLMManager.h>

namespace mx::gui {
namespace {

static const QString kDefaultPromptTitle =
    QStringLiteral("Default Agent System Prompt");

static const QString kDefaultPromptContent = QString::fromUtf8(
R"MX(You are an expert analyst working inside the Multiplier binary analysis IDE. You have access to tools for managing tasks, spreadsheets, documents, running Python scripts, and navigating the codebase.

## Entity References

This codebase is fully indexed. Every file, function, type, variable, and macro has a unique entity ID. Always use precise entity references:
- Files: "file:<entity_id>" (e.g. "file:4295032833")
- Functions: "func:<entity_id>"
- Types: "type:<entity_id>"
- Variables: "var:<entity_id>"
- Macros: "macro:<entity_id>"

Use search_entities to find entity IDs by name. Use get_definition to read their source code. Use get_references to find all uses. Always record entity IDs in tasks and findings so they are machine-traceable.

## Task Management

Use the task management tools to track your work:
- create_task: Add a new task with description, priority, and entity reference
- update_task: Change status (planned -> in_progress -> completed/blocked)
- complete_task: Mark a task done with completion notes
- list_tasks: See your current task board
- get_task_board_summary: Quick overview of progress

Keep your task board current. Create tasks before starting work. Update status as you go. Complete tasks with findings.

## Workflow

1. **Orient**: Use list_tasks and get_task_board_summary to see where you left off.
2. **Plan**: Create tasks for what needs to be done. Prioritize.
3. **Execute**: Work through tasks in priority order. For each:
   - Set status to "in_progress"
   - Use navigation tools (search_entities, get_definition, get_references) to investigate
   - Use run_python to execute analysis scripts leveraging the Multiplier Python bindings
   - Record findings in documents, linked to the task
   - Complete the task with a summary
4. **Report**: Use get_task_board_summary to report progress.

## Tools Available

- **Task tools**: create_task, update_task, complete_task, list_tasks, get_task_board_summary
- **Spreadsheet tools**: create_sheet, read_cell, write_cell, add_row, set_row_color, set_checkbox, sort_sheet, etc.
- **Document tools**: create_document, read_document, edit_document, list_documents, link_document_to_cell
- **Navigation tools**: search_entities, get_definition, get_references, list_files
- **Python tools**: run_python (execute scripts using Multiplier Python bindings), create_script_file
- **Session tools**: get_audit_context, save_checkpoint, log_observation
- **Completion**: finish (call when done with current work -- provide summary and next actions)

## Completing Work

When you finish your current work, always call the finish tool with:
- A summary of what you accomplished
- Suggested next actions for follow-up
- Status: "completed" if done, "blocked" if stuck, "needs_input" if you need human guidance

This ensures your work is properly recorded and the human knows what to do next.

## Important Guidelines

- Be methodical. Use entity IDs, not just names. "Audit func:4295032833 (parse_header)" not just "Audit parse_header".
- Record reasoning in documents. Future analysis depends on understanding decisions.
- Save checkpoints periodically for recoverability.
- When blocked, record it and move to the next task.
- Use run_python for bulk analysis -- the Multiplier Python bindings give you full programmatic access to the index.)MX");

static QString ensureDefaultPromptDocument(ConfigManager &config) {
  auto prompts = config.LoadDocumentsByCategory(QStringLiteral("prompt"));
  for (const auto &doc : prompts) {
    if (doc.title == kDefaultPromptTitle) {
      return config.LoadDocumentContent(doc.doc_id);
    }
  }

  auto doc_id = config.CreateDocument(kDefaultPromptContent, kDefaultPromptTitle);
  if (doc_id >= 0) {
    config.SetDocumentCategory(doc_id, QStringLiteral("prompt"));
  }
  return kDefaultPromptContent;
}

static QLabel *makeHint(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setWordWrap(true);
  auto f = label->font();
  f.setPointSize(f.pointSize() - 1);
  label->setFont(f);
  label->setStyleSheet(QStringLiteral("color: palette(mid);"));
  return label;
}

}  // namespace

struct AgentConfigPanel::PrivateData {
  LLMManager &llm_manager;
  ConfigManager &config_manager;

  QComboBox *backend_combo{nullptr};
  QLineEdit *api_key_edit{nullptr};
  QLineEdit *base_url_edit{nullptr};
  QLabel *base_url_label{nullptr};
  QComboBox *model_combo{nullptr};
  QPlainTextEdit *system_prompt_edit{nullptr};
  QPushButton *load_prompt_button{nullptr};
  QSpinBox *max_iterations_spin{nullptr};
  QDoubleSpinBox *temperature_spin{nullptr};
  QLineEdit *python_path_edit{nullptr};
  QPushButton *python_browse_btn{nullptr};
  QPushButton *python_verify_btn{nullptr};
  QLabel *python_status_label{nullptr};
  QLabel *save_indicator{nullptr};
  int prompt_doc_id{-1};
  bool restoring{false};  // Suppress saves during initialization.

  explicit PrivateData(LLMManager &lm, ConfigManager &cm)
      : llm_manager(lm), config_manager(cm) {}
};

AgentConfigPanel::~AgentConfigPanel(void) {}

AgentConfigPanel::AgentConfigPanel(LLMManager &llm_manager,
                                    ConfigManager &config_manager,
                                    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(llm_manager, config_manager)) {

  d->restoring = true;

  auto *outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *content = new QWidget;
  auto *form = new QFormLayout(content);
  form->setContentsMargins(8, 8, 8, 8);
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  // ---- LLM Backend ----
  d->backend_combo = new QComboBox(content);
  d->backend_combo->addItems(
      {QStringLiteral("claude"), QStringLiteral("openai"),
       QStringLiteral("bedrock"), QStringLiteral("vllm")});
  form->addRow(tr("Backend:"), d->backend_combo);

  d->api_key_edit = new QLineEdit(content);
  d->api_key_edit->setEchoMode(QLineEdit::Password);
  d->api_key_edit->setPlaceholderText(tr("Enter API key..."));
  form->addRow(tr("API Key:"), d->api_key_edit);

  d->base_url_edit = new QLineEdit(content);
  d->base_url_edit->setPlaceholderText(tr("https://api.openai.com/v1"));
  d->base_url_label = new QLabel(tr("Base URL:"), content);
  form->addRow(d->base_url_label, d->base_url_edit);
  d->base_url_edit->setVisible(false);
  d->base_url_label->setVisible(false);

  d->model_combo = new QComboBox(content);
  d->model_combo->setEditable(true);
  form->addRow(tr("Model:"), d->model_combo);

  // Separator.
  auto *sep1 = new QFrame(content);
  sep1->setFrameShape(QFrame::HLine);
  sep1->setFrameShadow(QFrame::Sunken);
  form->addRow(sep1);

  // ---- System Prompt ----
  d->system_prompt_edit = new QPlainTextEdit(content);
  d->system_prompt_edit->setMinimumHeight(100);

  auto default_prompt = ensureDefaultPromptDocument(d->config_manager);
  d->system_prompt_edit->setPlainText(default_prompt);

  auto prompts = d->config_manager.LoadDocumentsByCategory(
      QStringLiteral("prompt"));
  for (const auto &doc : prompts) {
    if (doc.title == kDefaultPromptTitle) {
      d->prompt_doc_id = doc.doc_id;
      break;
    }
  }

  form->addRow(tr("System Prompt:"), d->system_prompt_edit);

  d->load_prompt_button = new QPushButton(tr("Load from documents"), content);
  form->addRow(QString(), d->load_prompt_button);

  // Separator.
  auto *sep2 = new QFrame(content);
  sep2->setFrameShape(QFrame::HLine);
  sep2->setFrameShadow(QFrame::Sunken);
  form->addRow(sep2);

  // ---- Parameters ----
  d->max_iterations_spin = new QSpinBox(content);
  d->max_iterations_spin->setRange(1, 200);
  d->max_iterations_spin->setValue(50);
  form->addRow(tr("Max iterations:"), d->max_iterations_spin);
  form->addRow(QString(), makeHint(
      tr("Maximum tool-call rounds per message. The agent stops after "
         "this many LLM calls even if not finished."), content));

  d->temperature_spin = new QDoubleSpinBox(content);
  d->temperature_spin->setRange(0.0, 2.0);
  d->temperature_spin->setSingleStep(0.1);
  d->temperature_spin->setValue(0.0);
  d->temperature_spin->setDecimals(1);
  form->addRow(tr("Temperature:"), d->temperature_spin);
  form->addRow(QString(), makeHint(
      tr("0.0 = deterministic (best for analysis). Higher values "
         "increase randomness. Use 0.5-1.0 for brainstorming."),
      content));

  // Separator.
  auto *sep3 = new QFrame(content);
  sep3->setFrameShape(QFrame::HLine);
  sep3->setFrameShadow(QFrame::Sunken);
  form->addRow(sep3);

  // ---- Python ----
  auto *python_row = new QWidget(content);
  auto *python_layout = new QHBoxLayout(python_row);
  python_layout->setContentsMargins(0, 0, 0, 0);
  python_layout->setSpacing(4);

  d->python_path_edit = new QLineEdit(python_row);
  d->python_path_edit->setPlaceholderText(tr("System default"));
  d->python_path_edit->setText(d->config_manager.PythonInterpreterPath());
  python_layout->addWidget(d->python_path_edit, 1);

  d->python_browse_btn = new QPushButton(tr("Browse..."), python_row);
  python_layout->addWidget(d->python_browse_btn);

  d->python_verify_btn = new QPushButton(tr("Verify"), python_row);
  python_layout->addWidget(d->python_verify_btn);

  form->addRow(tr("Python:"), python_row);

  d->python_status_label = new QLabel(content);
  d->python_status_label->setWordWrap(true);
  form->addRow(QString(), d->python_status_label);

  form->addRow(QString(), makeHint(
      tr("Path to a Python interpreter that has the multiplier "
         "bindings installed. Click Verify to check."), content));

  // ---- Save indicator ----
  d->save_indicator = new QLabel(content);
  d->save_indicator->setAlignment(Qt::AlignCenter);
  form->addRow(d->save_indicator);

  scroll->setWidget(content);
  outer_layout->addWidget(scroll);

  // ---- Connections ----
  connect(d->backend_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AgentConfigPanel::onBackendTypeChanged);
  connect(d->api_key_edit, &QLineEdit::editingFinished,
          this, &AgentConfigPanel::onApiKeyChanged);
  connect(d->base_url_edit, &QLineEdit::editingFinished,
          this, &AgentConfigPanel::onBaseUrlChanged);
  connect(d->model_combo, &QComboBox::currentTextChanged,
          this, &AgentConfigPanel::onModelChanged);
  connect(d->load_prompt_button, &QPushButton::clicked,
          this, &AgentConfigPanel::onLoadPromptClicked);
  connect(d->python_browse_btn, &QPushButton::clicked,
          this, &AgentConfigPanel::onBrowsePythonClicked);
  connect(d->python_verify_btn, &QPushButton::clicked,
          this, &AgentConfigPanel::onVerifyPythonClicked);
  connect(d->python_path_edit, &QLineEdit::editingFinished, this, [this] {
    d->config_manager.SetPythonInterpreterPath(d->python_path_edit->text());
    showSaved();
  });

  connect(d->system_prompt_edit, &QPlainTextEdit::textChanged, this, [this] {
    if (d->restoring) return;
    if (d->prompt_doc_id >= 0) {
      d->config_manager.SaveDocumentContent(
          d->prompt_doc_id, d->system_prompt_edit->toPlainText());
    }
  });

  connect(d->max_iterations_spin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this] {
            if (!d->restoring) showSaved();
            emit configChanged();
          });
  connect(d->temperature_spin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this] {
            if (!d->restoring) showSaved();
            emit configChanged();
          });

  // ---- Restore saved state ----
  auto active = d->llm_manager.activeBackendName();
  if (!active.isEmpty()) {
    auto type = d->llm_manager.backendType(active);
    auto idx = d->backend_combo->findText(type);
    if (idx >= 0) {
      d->backend_combo->setCurrentIndex(idx);
    }

    // Restore saved API key and model.
    auto saved_key = d->llm_manager.backendConfig(active,
                         QStringLiteral("api_key"));
    if (!saved_key.isEmpty()) {
      d->api_key_edit->setText(saved_key);
    }

    auto saved_model = d->llm_manager.backendConfig(active,
                           QStringLiteral("model"));
    if (!saved_model.isEmpty()) {
      populateModels(type);
      auto model_idx = d->model_combo->findText(saved_model);
      if (model_idx >= 0) {
        d->model_combo->setCurrentIndex(model_idx);
      } else {
        d->model_combo->setEditText(saved_model);
      }
    } else {
      populateModels(type);
    }

    auto saved_url = d->llm_manager.backendConfig(active,
                         QStringLiteral("base_url"));
    if (!saved_url.isEmpty()) {
      d->base_url_edit->setText(saved_url);
    }
  } else {
    populateModels(d->backend_combo->currentText());
  }

  d->restoring = false;
}

QString AgentConfigPanel::systemPrompt(void) const {
  return d->system_prompt_edit->toPlainText();
}

int AgentConfigPanel::maxIterations(void) const {
  return d->max_iterations_spin->value();
}

double AgentConfigPanel::temperature(void) const {
  return d->temperature_spin->value();
}

void AgentConfigPanel::showSaved(void) {
  d->save_indicator->setText(tr("Settings saved."));
  d->save_indicator->setStyleSheet(
      QStringLiteral("color: palette(mid); font-style: italic;"));
  QTimer::singleShot(2000, d->save_indicator, [label = d->save_indicator] {
    label->clear();
  });
}

void AgentConfigPanel::onBackendTypeChanged(int index) {
  auto type = d->backend_combo->itemText(index);
  bool show_url = (type == QStringLiteral("openai") ||
                   type == QStringLiteral("vllm"));
  d->base_url_edit->setVisible(show_url);
  d->base_url_label->setVisible(show_url);

  ensureBackendExists(type);

  // Restore saved config for this backend.
  auto saved_key = d->llm_manager.backendConfig(type,
                       QStringLiteral("api_key"));
  d->api_key_edit->setText(saved_key);

  auto saved_url = d->llm_manager.backendConfig(type,
                       QStringLiteral("base_url"));
  d->base_url_edit->setText(saved_url);

  auto saved_model = d->llm_manager.backendConfig(type,
                         QStringLiteral("model"));
  populateModels(type);
  if (!saved_model.isEmpty()) {
    auto model_idx = d->model_combo->findText(saved_model);
    if (model_idx >= 0) {
      d->model_combo->setCurrentIndex(model_idx);
    } else {
      d->model_combo->setEditText(saved_model);
    }
  }

  if (!d->restoring) showSaved();
  emit configChanged();
}

void AgentConfigPanel::onApiKeyChanged(void) {
  auto name = d->llm_manager.activeBackendName();
  if (!name.isEmpty()) {
    d->llm_manager.setBackendConfig(
        name, QStringLiteral("api_key"), d->api_key_edit->text());
    d->llm_manager.saveConfig();
  }
  showSaved();
  emit configChanged();
}

void AgentConfigPanel::onBaseUrlChanged(void) {
  auto name = d->llm_manager.activeBackendName();
  if (!name.isEmpty()) {
    d->llm_manager.setBackendConfig(
        name, QStringLiteral("base_url"), d->base_url_edit->text());
    d->llm_manager.saveConfig();
  }
  showSaved();
  emit configChanged();
}

void AgentConfigPanel::onModelChanged(void) {
  if (d->restoring) return;
  auto name = d->llm_manager.activeBackendName();
  if (!name.isEmpty()) {
    d->llm_manager.setBackendConfig(
        name, QStringLiteral("model"), d->model_combo->currentText());
    d->llm_manager.saveConfig();
  }
  showSaved();
  emit configChanged();
}

void AgentConfigPanel::onLoadPromptClicked(void) {
  auto docs = d->config_manager.LoadDocumentsByCategory(
      QStringLiteral("prompt"));
  if (docs.isEmpty()) {
    return;
  }

  auto *menu = new QMenu(this);
  for (const auto &doc : docs) {
    auto *action = menu->addAction(
        doc.title.isEmpty() ? tr("Document %1").arg(doc.doc_id) : doc.title);
    connect(action, &QAction::triggered, this, [this, doc_id = doc.doc_id] {
      auto content = d->config_manager.LoadDocumentContent(doc_id);
      if (!content.isEmpty()) {
        d->prompt_doc_id = doc_id;
        d->system_prompt_edit->setPlainText(content);
      }
    });
  }
  menu->popup(d->load_prompt_button->mapToGlobal(
      d->load_prompt_button->rect().bottomLeft()));
}

void AgentConfigPanel::onBrowsePythonClicked(void) {
  QFileDialog dialog(this, tr("Select Python Interpreter"),
                     QStringLiteral("/usr"));
  dialog.setNameFilter(tr("All files (*)"));
  dialog.setFileMode(QFileDialog::ExistingFile);
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  dialog.setWindowModality(Qt::ApplicationModal);

  if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
    auto path = dialog.selectedFiles().first();
    d->python_path_edit->setText(path);
    d->config_manager.SetPythonInterpreterPath(path);
    showSaved();
  }
}

void AgentConfigPanel::onVerifyPythonClicked(void) {
  auto path = d->python_path_edit->text();
  if (path.isEmpty()) {
    path = QStringLiteral("python3");
  }

  d->python_status_label->setText(tr("Checking..."));
  d->python_status_label->setStyleSheet(
      QStringLiteral("color: palette(mid);"));

  QProcess proc;
  proc.setProgram(path);
  proc.setArguments({QStringLiteral("-c"),
      QStringLiteral("import multiplier; print('OK:', multiplier.__file__)")});
  proc.start();

  if (!proc.waitForFinished(5000)) {
    d->python_status_label->setText(
        tr("Failed: interpreter not found or timed out"));
    d->python_status_label->setStyleSheet(
        QStringLiteral("color: red;"));
    return;
  }

  if (proc.exitCode() != 0) {
    auto err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    d->python_status_label->setText(
        tr("Missing bindings: %1").arg(
            err.isEmpty() ? tr("import multiplier failed") : err));
    d->python_status_label->setStyleSheet(
        QStringLiteral("color: red;"));
    return;
  }

  auto output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
  d->python_status_label->setText(output);
  d->python_status_label->setStyleSheet(
      QStringLiteral("color: green;"));
}

void AgentConfigPanel::populateModels(const QString &backend_type) {
  d->model_combo->clear();
  if (backend_type == QStringLiteral("claude")) {
    d->model_combo->addItems(
        {QStringLiteral("claude-sonnet-4-20250514"),
         QStringLiteral("claude-opus-4-20250514")});
  } else if (backend_type == QStringLiteral("openai")) {
    d->model_combo->addItems(
        {QStringLiteral("gpt-4o"),
         QStringLiteral("gpt-4o-mini"),
         QStringLiteral("o3")});
  } else if (backend_type == QStringLiteral("bedrock")) {
    d->model_combo->addItems(
        {QStringLiteral("anthropic.claude-sonnet-4-20250514-v1:0")});
  }
}

void AgentConfigPanel::ensureBackendExists(const QString &type) {
  auto names = d->llm_manager.backendNames();
  if (!names.contains(type)) {
    d->llm_manager.addBackend(type, type);
  }
  d->llm_manager.setActiveBackend(type);
  d->llm_manager.saveConfig();
}

}  // namespace mx::gui
