// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "AgentConfigPanel.h"

#include <QComboBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/LLMManager.h>

namespace mx::gui {
namespace {

static const QString kDefaultPromptTitle =
    QStringLiteral("Default Agent System Prompt");

static const QString kDefaultPromptContent = QStringLiteral(
R"(You are an expert analyst working inside the Multiplier binary analysis IDE. You have access to tools for managing spreadsheets, documents, and navigating the codebase.

## How to work

1. **Use spreadsheets as task boards.** Create a sheet to track your work. Each row is a task. Use columns for: Description, Status, Priority, Notes. Use checkboxes for completion. Use row colors for priority (red=critical, orange=high, yellow=medium, green=done).

2. **Use documents for findings.** Create documents to record detailed analysis, reasoning chains, and conclusions. Link documents to spreadsheet rows so findings are traceable to tasks.

3. **Use the Python REPL** to run scripts that leverage the Multiplier API for programmatic analysis when tools alone are insufficient.

4. **Navigate the codebase** using search_entities, get_definition, get_references, and list_files to understand code structure.

5. **Save checkpoints** periodically so your progress is recoverable and observable.

6. **Stay focused.** Update your task board as you work. Mark tasks done. Reprioritize as you learn more. If you discover new work, add it to the board.

## Important guidelines

- Be methodical. Enumerate candidates broadly, then investigate each one deeply.
- Record your reasoning. Future analysis (by you or an observer) depends on understanding why decisions were made.
- When you hit a dead end, record it and move on. Don't loop.
- Use get_audit_context to orient yourself if you lose track of progress.)");

// Find or create the default system prompt document.
// Returns the document content.
static QString ensureDefaultPromptDocument(ConfigManager &config) {
  // Look for existing default prompt document.
  auto prompts = config.LoadDocumentsByCategory(QStringLiteral("prompt"));
  for (const auto &doc : prompts) {
    if (doc.title == kDefaultPromptTitle) {
      return config.LoadDocumentContent(doc.doc_id);
    }
  }

  // Doesn't exist yet — create it.
  auto doc_id = config.CreateDocument(kDefaultPromptContent, kDefaultPromptTitle);
  if (doc_id >= 0) {
    config.SetDocumentCategory(doc_id, QStringLiteral("prompt"));
  }
  return kDefaultPromptContent;
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
  int prompt_doc_id{-1};  // Document ID backing the system prompt.

  explicit PrivateData(LLMManager &lm, ConfigManager &cm)
      : llm_manager(lm), config_manager(cm) {}
};

AgentConfigPanel::~AgentConfigPanel(void) {}

AgentConfigPanel::AgentConfigPanel(LLMManager &llm_manager,
                                    ConfigManager &config_manager,
                                    QWidget *parent)
    : QWidget(parent),
      d(new PrivateData(llm_manager, config_manager)) {

  auto *outer_layout = new QVBoxLayout(this);
  outer_layout->setContentsMargins(0, 0, 0, 0);

  auto *scroll = new QScrollArea(this);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto *content = new QWidget;
  auto *form = new QFormLayout(content);
  form->setContentsMargins(8, 8, 8, 8);

  // Backend type.
  d->backend_combo = new QComboBox(content);
  d->backend_combo->addItems(
      {QStringLiteral("claude"), QStringLiteral("openai"),
       QStringLiteral("bedrock"), QStringLiteral("vllm")});
  form->addRow(tr("Backend:"), d->backend_combo);

  // API key.
  d->api_key_edit = new QLineEdit(content);
  d->api_key_edit->setEchoMode(QLineEdit::Password);
  d->api_key_edit->setPlaceholderText(tr("Enter API key..."));
  form->addRow(tr("API Key:"), d->api_key_edit);

  // Base URL (only for openai/vllm).
  d->base_url_edit = new QLineEdit(content);
  d->base_url_edit->setPlaceholderText(tr("https://api.openai.com/v1"));
  d->base_url_label = new QLabel(tr("Base URL:"), content);
  form->addRow(d->base_url_label, d->base_url_edit);
  d->base_url_edit->setVisible(false);
  d->base_url_label->setVisible(false);

  // Model.
  d->model_combo = new QComboBox(content);
  d->model_combo->setEditable(true);
  form->addRow(tr("Model:"), d->model_combo);
  populateModels(d->backend_combo->currentText());

  // Separator.
  auto *sep1 = new QFrame(content);
  sep1->setFrameShape(QFrame::HLine);
  sep1->setFrameShadow(QFrame::Sunken);
  form->addRow(sep1);

  // System prompt — backed by a document.
  d->system_prompt_edit = new QPlainTextEdit(content);
  d->system_prompt_edit->setMinimumHeight(100);

  // Load the default prompt from the document store (creates it if needed).
  auto default_prompt = ensureDefaultPromptDocument(d->config_manager);
  d->system_prompt_edit->setPlainText(default_prompt);

  // Track which document is backing the prompt.
  auto prompts = d->config_manager.LoadDocumentsByCategory(
      QStringLiteral("prompt"));
  for (const auto &doc : prompts) {
    if (doc.title == kDefaultPromptTitle) {
      d->prompt_doc_id = doc.doc_id;
      break;
    }
  }

  form->addRow(tr("System Prompt:"), d->system_prompt_edit);

  // Load from documents.
  d->load_prompt_button = new QPushButton(tr("Load from documents"), content);
  form->addRow(QString(), d->load_prompt_button);

  // Separator.
  auto *sep2 = new QFrame(content);
  sep2->setFrameShape(QFrame::HLine);
  sep2->setFrameShadow(QFrame::Sunken);
  form->addRow(sep2);

  // Max iterations.
  d->max_iterations_spin = new QSpinBox(content);
  d->max_iterations_spin->setRange(1, 200);
  d->max_iterations_spin->setValue(50);
  form->addRow(tr("Max iterations:"), d->max_iterations_spin);

  // Temperature.
  d->temperature_spin = new QDoubleSpinBox(content);
  d->temperature_spin->setRange(0.0, 2.0);
  d->temperature_spin->setSingleStep(0.1);
  d->temperature_spin->setValue(0.0);
  d->temperature_spin->setDecimals(1);
  form->addRow(tr("Temperature:"), d->temperature_spin);

  scroll->setWidget(content);
  outer_layout->addWidget(scroll);

  // Connections.
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

  // Save prompt edits back to the backing document.
  connect(d->system_prompt_edit, &QPlainTextEdit::textChanged, this, [this] {
    if (d->prompt_doc_id >= 0) {
      d->config_manager.SaveDocumentContent(
          d->prompt_doc_id, d->system_prompt_edit->toPlainText());
    }
  });

  connect(d->max_iterations_spin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this] { emit configChanged(); });
  connect(d->temperature_spin,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this] { emit configChanged(); });

  // Initialize backend if one is already active.
  auto active = d->llm_manager.activeBackendName();
  if (!active.isEmpty()) {
    auto type = d->llm_manager.backendType(active);
    auto idx = d->backend_combo->findText(type);
    if (idx >= 0) {
      d->backend_combo->setCurrentIndex(idx);
    }
  }
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

void AgentConfigPanel::onBackendTypeChanged(int index) {
  auto type = d->backend_combo->itemText(index);
  bool show_url = (type == QStringLiteral("openai") ||
                   type == QStringLiteral("vllm"));
  d->base_url_edit->setVisible(show_url);
  d->base_url_label->setVisible(show_url);
  populateModels(type);
  ensureBackendExists(type);
  emit configChanged();
}

void AgentConfigPanel::onApiKeyChanged(void) {
  auto type = d->backend_combo->currentText();
  auto name = d->llm_manager.activeBackendName();
  if (!name.isEmpty()) {
    d->llm_manager.setBackendConfig(
        name, QStringLiteral("api_key"), d->api_key_edit->text());
    d->llm_manager.saveConfig();
  }
  emit configChanged();
}

void AgentConfigPanel::onBaseUrlChanged(void) {
  auto name = d->llm_manager.activeBackendName();
  if (!name.isEmpty()) {
    d->llm_manager.setBackendConfig(
        name, QStringLiteral("base_url"), d->base_url_edit->text());
    d->llm_manager.saveConfig();
  }
  emit configChanged();
}

void AgentConfigPanel::onModelChanged(void) {
  auto name = d->llm_manager.activeBackendName();
  if (!name.isEmpty()) {
    d->llm_manager.setBackendConfig(
        name, QStringLiteral("model"), d->model_combo->currentText());
    d->llm_manager.saveConfig();
  }
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
  // vllm: leave empty, user types.
}

void AgentConfigPanel::ensureBackendExists(const QString &type) {
  // Use the type name as the backend name for simplicity.
  auto names = d->llm_manager.backendNames();
  if (!names.contains(type)) {
    d->llm_manager.addBackend(type, type);
  }
  d->llm_manager.setActiveBackend(type);
  d->llm_manager.saveConfig();
}

}  // namespace mx::gui
