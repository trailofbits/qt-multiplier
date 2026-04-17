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
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QGroupBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <multiplier/GUI/Managers/ConfigManager.h>
#include <multiplier/GUI/Managers/LLMManager.h>

#include "AgentResourceLoader.h"

namespace mx::gui {
namespace {

static const QString kDefaultPromptTitle =
    QStringLiteral("Default Agent System Prompt");

static constexpr int kPromptVersion = 10;

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
  QComboBox *suggestion_combo{nullptr};
  QComboBox *enter_key_combo{nullptr};
  QComboBox *recommender_model_combo{nullptr};
  QComboBox *summarizer_model_combo{nullptr};
  QComboBox *observer_model_combo{nullptr};
  QGroupBox *model_roles_group{nullptr};
  QLineEdit *bedrock_access_key{nullptr};
  QLineEdit *bedrock_secret_key{nullptr};
  QLineEdit *bedrock_region{nullptr};
  QLabel *bedrock_access_label{nullptr};
  QLabel *bedrock_secret_label{nullptr};
  QLabel *bedrock_region_label{nullptr};
  QLabel *api_key_label{nullptr};
  QWidget *api_key_row{nullptr};
  QLineEdit *workspace_path_edit{nullptr};
  QPushButton *workspace_browse_btn{nullptr};
  QLineEdit *python_path_edit{nullptr};
  QPushButton *python_browse_btn{nullptr};
  QLabel *python_status_label{nullptr};
  QLineEdit *cc_path_edit{nullptr};
  QPushButton *cc_browse_btn{nullptr};
  QLineEdit *cxx_path_edit{nullptr};
  QPushButton *cxx_browse_btn{nullptr};
  QLineEdit *sdk_root_edit{nullptr};
  QPushButton *sdk_browse_btn{nullptr};
  QLabel *save_indicator{nullptr};
  QProcess *python_verify_proc{nullptr};
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
  d->backend_combo->addItem(tr("Anthropic (Claude)"), QStringLiteral("claude"));
  d->backend_combo->addItem(tr("AWS Bedrock"), QStringLiteral("bedrock"));
  d->backend_combo->addItem(tr("OpenAI"), QStringLiteral("openai"));
  d->backend_combo->addItem(tr("vLLM / Custom"), QStringLiteral("vllm"));
  form->addRow(tr("Backend:"), d->backend_combo);

  d->api_key_edit = new QLineEdit(content);
  d->api_key_edit->setEchoMode(QLineEdit::Password);
  d->api_key_edit->setPlaceholderText(tr("Enter API key..."));
  d->api_key_row = new QWidget(content);
  auto *api_key_layout = new QHBoxLayout(d->api_key_row);
  api_key_layout->setContentsMargins(0, 0, 0, 0);
  api_key_layout->setSpacing(4);
  api_key_layout->addWidget(d->api_key_edit, 1);

  auto *clear_keys_btn = new QPushButton(tr("Clear All"), d->api_key_row);
  clear_keys_btn->setToolTip(tr("Remove all saved API keys from settings"));
  api_key_layout->addWidget(clear_keys_btn);
  connect(clear_keys_btn, &QPushButton::clicked, this, [this] {
    d->llm_manager.clearAllApiKeys();
    d->api_key_edit->clear();
    showSaved();
  });

  d->api_key_label = new QLabel(tr("API Key:"), content);
  form->addRow(d->api_key_label, d->api_key_row);

  // Bedrock-specific credential fields (hidden by default).
  d->bedrock_access_key = new QLineEdit(content);
  d->bedrock_access_key->setEchoMode(QLineEdit::Password);
  d->bedrock_access_key->setPlaceholderText(tr("AWS Access Key ID"));
  d->bedrock_access_label = new QLabel(tr("Access Key:"), content);
  form->addRow(d->bedrock_access_label, d->bedrock_access_key);

  d->bedrock_secret_key = new QLineEdit(content);
  d->bedrock_secret_key->setEchoMode(QLineEdit::Password);
  d->bedrock_secret_key->setPlaceholderText(tr("AWS Secret Access Key"));
  d->bedrock_secret_label = new QLabel(tr("Secret Key:"), content);
  form->addRow(d->bedrock_secret_label, d->bedrock_secret_key);

  d->bedrock_region = new QLineEdit(content);
  d->bedrock_region->setPlaceholderText(tr("us-east-1"));
  d->bedrock_region_label = new QLabel(tr("Region:"), content);
  form->addRow(d->bedrock_region_label, d->bedrock_region);

  d->bedrock_access_key->setVisible(false);
  d->bedrock_secret_key->setVisible(false);
  d->bedrock_region->setVisible(false);
  d->bedrock_access_label->setVisible(false);
  d->bedrock_secret_label->setVisible(false);
  d->bedrock_region_label->setVisible(false);

  d->base_url_edit = new QLineEdit(content);
  d->base_url_edit->setPlaceholderText(tr("https://api.openai.com/v1"));
  d->base_url_label = new QLabel(tr("Base URL:"), content);
  form->addRow(d->base_url_label, d->base_url_edit);
  d->base_url_edit->setVisible(false);
  d->base_url_label->setVisible(false);

  d->model_combo = new QComboBox(content);
  d->model_combo->setEditable(true);
  form->addRow(tr("Model:"), d->model_combo);

  // Model Roles (collapsible).
  d->model_roles_group = new QGroupBox(tr("Model Roles"), content);
  d->model_roles_group->setCheckable(true);
  d->model_roles_group->setChecked(false);

  auto *roles_form = new QFormLayout(d->model_roles_group);
  roles_form->setContentsMargins(8, 8, 8, 8);
  roles_form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  auto setup_role_combo = [&](QComboBox *&combo, const QString &label) {
    combo = new QComboBox(d->model_roles_group);
    combo->setEditable(true);
    combo->addItem(tr("Same as primary"));
    roles_form->addRow(label, combo);
  };

  setup_role_combo(d->recommender_model_combo, tr("Recommender:"));
  setup_role_combo(d->summarizer_model_combo, tr("Code Summarizer:"));
  setup_role_combo(d->observer_model_combo, tr("Observer:"));

  // Hide content when collapsed.
  connect(d->model_roles_group, &QGroupBox::toggled, this, [this](bool checked) {
    auto *layout = d->model_roles_group->layout();
    if (!layout) return;
    for (int i = 0; i < layout->count(); ++i) {
      auto *item = layout->itemAt(i);
      if (item->widget()) {
        item->widget()->setVisible(checked);
      }
    }
    // Also hide labels in the QFormLayout.
    auto *fl = qobject_cast<QFormLayout *>(layout);
    if (fl) {
      for (int i = 0; i < fl->rowCount(); ++i) {
        auto *label_item = fl->itemAt(i, QFormLayout::LabelRole);
        auto *field_item = fl->itemAt(i, QFormLayout::FieldRole);
        if (label_item && label_item->widget()) {
          label_item->widget()->setVisible(checked);
        }
        if (field_item && field_item->widget()) {
          field_item->widget()->setVisible(checked);
        }
      }
    }
  });
  // Start collapsed.
  emit d->model_roles_group->toggled(false);

  form->addRow(d->model_roles_group);

  // Separator.
  auto *sep1 = new QFrame(content);
  sep1->setFrameShape(QFrame::HLine);
  sep1->setFrameShadow(QFrame::Sunken);
  form->addRow(sep1);

  // ---- System Prompt ----
  d->system_prompt_edit = new QPlainTextEdit(content);
  d->system_prompt_edit->setMinimumHeight(100);

  auto default_prompt = ensureResourceDocument(
      d->config_manager, kDefaultPromptTitle, QStringLiteral("prompt"),
      QStringLiteral(":/agent/prompts/system_prompt.md"), kPromptVersion);
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
  d->max_iterations_spin->setRange(1, 10000);
  d->max_iterations_spin->setValue(50);
  form->addRow(tr("Max tool-call rounds:"), d->max_iterations_spin);
  form->addRow(makeHint(
      tr("Maximum LLM round-trips per message. Each round: the agent "
         "calls tools, gets results, and decides what to do next."), content));

  d->temperature_spin = new QDoubleSpinBox(content);
  d->temperature_spin->setRange(0.0, 2.0);
  d->temperature_spin->setSingleStep(0.1);
  d->temperature_spin->setValue(0.0);
  d->temperature_spin->setDecimals(1);
  form->addRow(tr("Temperature:"), d->temperature_spin);
  form->addRow(makeHint(
      tr("0.0 = deterministic (best for analysis). Higher values "
         "increase randomness. Use 0.5-1.0 for brainstorming."),
      content));

  d->suggestion_combo = new QComboBox(content);
  d->suggestion_combo->addItems(
      {tr("Off"), tr("After each response")});
  d->suggestion_combo->setCurrentIndex(1);
  form->addRow(tr("Suggestions:"), d->suggestion_combo);

  d->enter_key_combo = new QComboBox(content);
  d->enter_key_combo->addItems(
      {tr("Send message"), tr("New line")});
  d->enter_key_combo->setCurrentIndex(0);
  form->addRow(tr("Enter key:"), d->enter_key_combo);
  form->addRow(makeHint(
      tr("When \"New line\" is selected, use Shift+Enter to send."),
      content));

  // Separator.
  auto *sep3 = new QFrame(content);
  sep3->setFrameShape(QFrame::HLine);
  sep3->setFrameShadow(QFrame::Sunken);
  form->addRow(sep3);

  // ---- Workspace ----
  auto *workspace_row = new QWidget(content);
  auto *workspace_layout = new QHBoxLayout(workspace_row);
  workspace_layout->setContentsMargins(0, 0, 0, 0);
  workspace_layout->setSpacing(4);

  d->workspace_path_edit = new QLineEdit(workspace_row);
  d->workspace_path_edit->setPlaceholderText(tr("System temp directory"));
  d->workspace_path_edit->setText(d->config_manager.WorkspacePath());
  workspace_layout->addWidget(d->workspace_path_edit, 1);

  d->workspace_browse_btn = new QPushButton(tr("Browse..."), workspace_row);
  workspace_layout->addWidget(d->workspace_browse_btn);

  form->addRow(tr("Workspace:"), workspace_row);

  form->addRow(makeHint(
      tr("Scripts, reports, and other artifacts are saved here."),
      content));

  // Separator.
  auto *sep4 = new QFrame(content);
  sep4->setFrameShape(QFrame::HLine);
  sep4->setFrameShadow(QFrame::Sunken);
  form->addRow(sep4);

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

  form->addRow(tr("Python:"), python_row);

  d->python_status_label = new QLabel(content);
  d->python_status_label->setWordWrap(true);
  form->addRow(QString(), d->python_status_label);

  form->addRow(makeHint(
      tr("Path to a Python interpreter with multiplier bindings. "
         "Verified automatically."), content));

  // Separator.
  auto *sep5 = new QFrame(content);
  sep5->setFrameShape(QFrame::HLine);
  sep5->setFrameShadow(QFrame::Sunken);
  form->addRow(sep5);

  // ---- C/C++ Compilers ----
  auto *cc_row = new QWidget(content);
  auto *cc_layout = new QHBoxLayout(cc_row);
  cc_layout->setContentsMargins(0, 0, 0, 0);
  cc_layout->setSpacing(4);

  d->cc_path_edit = new QLineEdit(cc_row);
  d->cc_path_edit->setPlaceholderText(tr("/usr/bin/cc"));
  d->cc_path_edit->setText(d->config_manager.CCompilerPath());
  cc_layout->addWidget(d->cc_path_edit, 1);

  d->cc_browse_btn = new QPushButton(tr("Browse..."), cc_row);
  cc_layout->addWidget(d->cc_browse_btn);

  form->addRow(tr("C Compiler:"), cc_row);

  auto *cxx_row = new QWidget(content);
  auto *cxx_layout = new QHBoxLayout(cxx_row);
  cxx_layout->setContentsMargins(0, 0, 0, 0);
  cxx_layout->setSpacing(4);

  d->cxx_path_edit = new QLineEdit(cxx_row);
  d->cxx_path_edit->setPlaceholderText(tr("/usr/bin/c++"));
  d->cxx_path_edit->setText(d->config_manager.CXXCompilerPath());
  cxx_layout->addWidget(d->cxx_path_edit, 1);

  d->cxx_browse_btn = new QPushButton(tr("Browse..."), cxx_row);
  cxx_layout->addWidget(d->cxx_browse_btn);

  form->addRow(tr("C++ Compiler:"), cxx_row);

  auto *sdk_row = new QWidget(content);
  auto *sdk_layout = new QHBoxLayout(sdk_row);
  sdk_layout->setContentsMargins(0, 0, 0, 0);
  sdk_layout->setSpacing(4);

  d->sdk_root_edit = new QLineEdit(sdk_row);
  d->sdk_root_edit->setPlaceholderText(tr("(auto-detect)"));
  d->sdk_root_edit->setText(d->config_manager.SDKRoot());
  sdk_layout->addWidget(d->sdk_root_edit, 1);

  d->sdk_browse_btn = new QPushButton(tr("Browse..."), sdk_row);
  sdk_layout->addWidget(d->sdk_browse_btn);

  form->addRow(tr("SDK Root:"), sdk_row);

  form->addRow(makeHint(
      tr("Used when compiling fuzzer harnesses and test programs."),
      content));

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

  connect(d->bedrock_access_key, &QLineEdit::editingFinished, this, [this] {
    auto name = d->llm_manager.activeBackendName();
    if (!name.isEmpty()) {
      d->llm_manager.setBackendConfig(
          name, QStringLiteral("access_key_id"),
          d->bedrock_access_key->text());
      d->llm_manager.saveConfig();
    }
    showSaved();
    emit configChanged();
  });
  connect(d->bedrock_secret_key, &QLineEdit::editingFinished, this, [this] {
    auto name = d->llm_manager.activeBackendName();
    if (!name.isEmpty()) {
      d->llm_manager.setBackendConfig(
          name, QStringLiteral("secret_access_key"),
          d->bedrock_secret_key->text());
      d->llm_manager.saveConfig();
    }
    showSaved();
    emit configChanged();
  });
  connect(d->bedrock_region, &QLineEdit::editingFinished, this, [this] {
    auto name = d->llm_manager.activeBackendName();
    if (!name.isEmpty()) {
      d->llm_manager.setBackendConfig(
          name, QStringLiteral("region"),
          d->bedrock_region->text());
      d->llm_manager.saveConfig();
    }
    showSaved();
    emit configChanged();
  });

  connect(d->model_combo, &QComboBox::currentTextChanged,
          this, &AgentConfigPanel::onModelChanged);
  connect(d->load_prompt_button, &QPushButton::clicked,
          this, &AgentConfigPanel::onLoadPromptClicked);
  connect(d->workspace_browse_btn, &QPushButton::clicked,
          this, &AgentConfigPanel::onBrowseWorkspaceClicked);
  connect(d->workspace_path_edit, &QLineEdit::editingFinished, this, [this] {
    d->config_manager.SetWorkspacePath(d->workspace_path_edit->text());
    showSaved();
  });
  connect(d->python_browse_btn, &QPushButton::clicked,
          this, &AgentConfigPanel::onBrowsePythonClicked);
  connect(d->python_path_edit, &QLineEdit::textChanged, this, [this] {
    auto text = d->python_path_edit->text();
    // Auto-verify when the path looks like a python interpreter.
    static QRegularExpression python_re(
        QStringLiteral("python[0-9.]*$"));
    if (python_re.match(text).hasMatch() || text.isEmpty()) {
      maybeVerifyPython();
    }
  });
  connect(d->python_path_edit, &QLineEdit::editingFinished, this, [this] {
    d->config_manager.SetPythonInterpreterPath(d->python_path_edit->text());
    showSaved();
    maybeVerifyPython();
  });

  // Compiler connections.
  connect(d->cc_browse_btn, &QPushButton::clicked, this, [this] {
    QFileDialog dialog(this, tr("Select C Compiler"), QStringLiteral("/usr"));
    dialog.setNameFilter(tr("All files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setWindowModality(Qt::ApplicationModal);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
      auto path = dialog.selectedFiles().first();
      d->cc_path_edit->setText(path);
      d->config_manager.SetCCompilerPath(path);
      showSaved();
    }
  });
  connect(d->cc_path_edit, &QLineEdit::editingFinished, this, [this] {
    d->config_manager.SetCCompilerPath(d->cc_path_edit->text());
    showSaved();
  });

  connect(d->cxx_browse_btn, &QPushButton::clicked, this, [this] {
    QFileDialog dialog(this, tr("Select C++ Compiler"), QStringLiteral("/usr"));
    dialog.setNameFilter(tr("All files (*)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setWindowModality(Qt::ApplicationModal);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
      auto path = dialog.selectedFiles().first();
      d->cxx_path_edit->setText(path);
      d->config_manager.SetCXXCompilerPath(path);
      showSaved();
    }
  });
  connect(d->cxx_path_edit, &QLineEdit::editingFinished, this, [this] {
    d->config_manager.SetCXXCompilerPath(d->cxx_path_edit->text());
    showSaved();
  });

  connect(d->sdk_browse_btn, &QPushButton::clicked, this, [this] {
    QFileDialog dialog(this, tr("Select SDK Root"), QDir::homePath());
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setWindowModality(Qt::ApplicationModal);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
      auto path = dialog.selectedFiles().first();
      d->sdk_root_edit->setText(path);
      d->config_manager.SetSDKRoot(path);
      showSaved();
    }
  });
  connect(d->sdk_root_edit, &QLineEdit::editingFinished, this, [this] {
    d->config_manager.SetSDKRoot(d->sdk_root_edit->text());
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
  connect(d->enter_key_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this] {
            if (!d->restoring) showSaved();
            emit configChanged();
          });

  auto role_combo_changed = [this] {
    if (d->restoring) return;
    auto name = d->llm_manager.activeBackendName();
    if (!name.isEmpty()) {
      d->llm_manager.setBackendConfig(
          name, QStringLiteral("recommender_model"),
          d->recommender_model_combo->currentIndex() == 0
              ? QString() : d->recommender_model_combo->currentText());
      d->llm_manager.setBackendConfig(
          name, QStringLiteral("summarizer_model"),
          d->summarizer_model_combo->currentIndex() == 0
              ? QString() : d->summarizer_model_combo->currentText());
      d->llm_manager.setBackendConfig(
          name, QStringLiteral("observer_model"),
          d->observer_model_combo->currentIndex() == 0
              ? QString() : d->observer_model_combo->currentText());
      d->llm_manager.saveConfig();
    }
    showSaved();
    emit configChanged();
  };
  connect(d->recommender_model_combo, &QComboBox::currentTextChanged,
          this, role_combo_changed);
  connect(d->summarizer_model_combo, &QComboBox::currentTextChanged,
          this, role_combo_changed);
  connect(d->observer_model_combo, &QComboBox::currentTextChanged,
          this, role_combo_changed);

  // ---- Restore saved state ----
  auto active = d->llm_manager.activeBackendName();
  if (!active.isEmpty()) {
    auto type = d->llm_manager.backendType(active);
    auto idx = d->backend_combo->findData(type);
    if (idx >= 0) {
      d->backend_combo->setCurrentIndex(idx);
    }

    // Restore saved API key and model.
    auto saved_key = d->llm_manager.apiKeyForType(type);
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

    // Restore Bedrock fields if applicable.
    if (type == QStringLiteral("bedrock")) {
      d->api_key_label->setVisible(false);
      d->api_key_row->setVisible(false);
      d->bedrock_access_key->setVisible(true);
      d->bedrock_secret_key->setVisible(true);
      d->bedrock_region->setVisible(true);
      d->bedrock_access_label->setVisible(true);
      d->bedrock_secret_label->setVisible(true);
      d->bedrock_region_label->setVisible(true);
      d->bedrock_access_key->setText(
          d->llm_manager.backendConfig(type, QStringLiteral("access_key_id")));
      d->bedrock_secret_key->setText(
          d->llm_manager.backendConfig(type, QStringLiteral("secret_access_key")));
      d->bedrock_region->setText(
          d->llm_manager.backendConfig(type, QStringLiteral("region")));
    }
  } else {
    populateModels(d->backend_combo->currentData().toString());
  }

  // Restore per-role model settings.
  auto active_name = d->llm_manager.activeBackendName();
  if (!active_name.isEmpty()) {
    auto restore_role = [&](QComboBox *combo, const QString &key) {
      auto saved = d->llm_manager.backendConfig(active_name, key);
      if (!saved.isEmpty()) {
        auto idx = combo->findText(saved);
        if (idx >= 0) {
          combo->setCurrentIndex(idx);
        } else {
          combo->setEditText(saved);
        }
      }
    };
    restore_role(d->recommender_model_combo,
                 QStringLiteral("recommender_model"));
    restore_role(d->summarizer_model_combo,
                 QStringLiteral("summarizer_model"));
    restore_role(d->observer_model_combo,
                 QStringLiteral("observer_model"));
  }

  d->restoring = false;

  // Auto-verify Python on startup if a path is configured.
  if (!d->python_path_edit->text().isEmpty()) {
    maybeVerifyPython();
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

int AgentConfigPanel::suggestionMode(void) const {
  return d->suggestion_combo->currentIndex();
}

bool AgentConfigPanel::enterToSend(void) const {
  return d->enter_key_combo->currentIndex() == 0;
}

QString AgentConfigPanel::recommenderModel(void) const {
  if (d->recommender_model_combo->currentIndex() == 0) {
    return {};
  }
  return d->recommender_model_combo->currentText();
}

QString AgentConfigPanel::summarizerModel(void) const {
  if (d->summarizer_model_combo->currentIndex() == 0) {
    return {};
  }
  return d->summarizer_model_combo->currentText();
}

QString AgentConfigPanel::observerModel(void) const {
  if (d->observer_model_combo->currentIndex() == 0) {
    return {};
  }
  return d->observer_model_combo->currentText();
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
  auto type = d->backend_combo->itemData(index).toString();
  bool show_url = (type == QStringLiteral("openai") ||
                   type == QStringLiteral("vllm"));
  d->base_url_edit->setVisible(show_url);
  d->base_url_label->setVisible(show_url);

  bool is_bedrock = (type == QStringLiteral("bedrock"));
  d->api_key_label->setVisible(!is_bedrock);
  d->api_key_row->setVisible(!is_bedrock);
  d->bedrock_access_key->setVisible(is_bedrock);
  d->bedrock_secret_key->setVisible(is_bedrock);
  d->bedrock_region->setVisible(is_bedrock);
  d->bedrock_access_label->setVisible(is_bedrock);
  d->bedrock_secret_label->setVisible(is_bedrock);
  d->bedrock_region_label->setVisible(is_bedrock);

  ensureBackendExists(type);

  // Restore saved config for this backend.
  auto saved_key = d->llm_manager.apiKeyForType(type);
  d->api_key_edit->setText(saved_key);

  if (is_bedrock) {
    d->bedrock_access_key->setText(
        d->llm_manager.backendConfig(type, QStringLiteral("access_key_id")));
    d->bedrock_secret_key->setText(
        d->llm_manager.backendConfig(type, QStringLiteral("secret_access_key")));
    d->bedrock_region->setText(
        d->llm_manager.backendConfig(type, QStringLiteral("region")));
  }

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
  auto type = d->backend_combo->currentData().toString();
  d->llm_manager.setApiKeyForType(type, d->api_key_edit->text());
  d->llm_manager.saveConfig();
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

void AgentConfigPanel::onBrowseWorkspaceClicked(void) {
  QFileDialog dialog(this, tr("Select Workspace Directory"),
                     QDir::homePath());
  dialog.setFileMode(QFileDialog::Directory);
  dialog.setOption(QFileDialog::ShowDirsOnly, true);
  dialog.setOption(QFileDialog::DontUseNativeDialog, true);
  dialog.setWindowModality(Qt::ApplicationModal);

  if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
    auto path = dialog.selectedFiles().first();
    d->workspace_path_edit->setText(path);
    d->config_manager.SetWorkspacePath(path);
    showSaved();
  }
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
    maybeVerifyPython();
  }
}

void AgentConfigPanel::setPythonStatus(int state) {
  // 0 = neutral, 1 = checking, 2 = ok, 3 = error
  switch (state) {
    case 1:
      d->python_path_edit->setStyleSheet(QStringLiteral(
          "QLineEdit { background-color: palette(base); }"));
      d->python_status_label->setText(tr("Checking..."));
      d->python_status_label->setStyleSheet(
          QStringLiteral("color: palette(mid);"));
      break;
    case 2:
      d->python_path_edit->setStyleSheet(QStringLiteral(
          "QLineEdit { background-color: rgba(0, 180, 0, 40); }"));
      break;
    case 3:
      d->python_path_edit->setStyleSheet(QStringLiteral(
          "QLineEdit { background-color: rgba(220, 0, 0, 40); }"));
      break;
    default:
      d->python_path_edit->setStyleSheet({});
      d->python_status_label->clear();
      break;
  }
}

void AgentConfigPanel::maybeVerifyPython(void) {
  if (d->python_verify_proc) {
    d->python_verify_proc->kill();
    d->python_verify_proc->deleteLater();
    d->python_verify_proc = nullptr;
  }

  auto path = d->python_path_edit->text();
  if (path.isEmpty()) {
    path = QStringLiteral("python3");
  }

  setPythonStatus(1);

  auto *proc = new QProcess(this);
  d->python_verify_proc = proc;
  proc->setProgram(path);
  proc->setArguments({QStringLiteral("-c"),
      QStringLiteral(
          "import importlib.util, sys; "
          "spec = importlib.util.find_spec('multiplier'); "
          "print(spec.origin if spec else 'NOT_FOUND'); "
          "sys.exit(0 if spec else 1)")});

  // Set up venv environment. Try multiple strategies:
  // 1. Check for pyvenv.cfg in parent dir (standard venv layout: venv/bin/python)
  // 2. Check for pyvenv.cfg in grandparent dir
  // 3. Check if VIRTUAL_ENV is already set in the system environment
  QFileInfo fi(path);
  auto bin_dir = fi.absolutePath();
  auto env = QProcessEnvironment::systemEnvironment();

  // Strategy 1: venv/bin/python → pyvenv.cfg at venv/
  auto venv_dir = QFileInfo(bin_dir).absolutePath();
  auto pyvenv_cfg = venv_dir + QStringLiteral("/pyvenv.cfg");

  if (QFileInfo::exists(pyvenv_cfg)) {
    env.insert(QStringLiteral("VIRTUAL_ENV"), venv_dir);
    env.insert(QStringLiteral("PATH"),
               bin_dir + QStringLiteral(":") + env.value(QStringLiteral("PATH")));
  } else {
    // Strategy 2: maybe the path IS the venv dir and they pointed at the
    // python inside it without the bin/ prefix. Or the binary is a symlink.
    auto canonical = fi.canonicalFilePath();
    if (!canonical.isEmpty()) {
      QFileInfo cfi(canonical);
      auto canon_bin = cfi.absolutePath();
      auto canon_venv = QFileInfo(canon_bin).absolutePath();
      auto canon_cfg = canon_venv + QStringLiteral("/pyvenv.cfg");
      if (QFileInfo::exists(canon_cfg)) {
        env.insert(QStringLiteral("VIRTUAL_ENV"), canon_venv);
        env.insert(QStringLiteral("PATH"),
                   canon_bin + QStringLiteral(":") +
                       env.value(QStringLiteral("PATH")));
      }
    }
  }

  // Always set PYTHONDONTWRITEBYTECODE to avoid permission issues.
  env.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
  proc->setProcessEnvironment(env);

  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, proc](int exit_code, QProcess::ExitStatus) {
    if (d->python_verify_proc != proc) {
      proc->deleteLater();
      return;
    }
    d->python_verify_proc = nullptr;

    if (exit_code == 0) {
      auto output = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
      setPythonStatus(2);
      d->python_status_label->setText(output);
      d->python_status_label->setStyleSheet(
          QStringLiteral("color: palette(mid); font-style: italic;"));
    } else {
      auto err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
      auto out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
      // Show the most useful error info.
      QString detail;
      if (!err.isEmpty()) {
        // Extract just the last line (usually the actual ImportError).
        auto lines = err.split(QLatin1Char('\n'));
        detail = lines.last().trimmed();
        if (detail.length() > 150) {
          detail = detail.left(150) + QStringLiteral("...");
        }
      } else if (!out.isEmpty()) {
        detail = out;
      } else {
        detail = tr("import multiplier failed (exit code %1)").arg(exit_code);
      }
      setPythonStatus(3);
      d->python_status_label->setText(detail);
      d->python_status_label->setStyleSheet(
          QStringLiteral("color: palette(mid); font-style: italic;"));
    }
    proc->deleteLater();
  });

  connect(proc, &QProcess::errorOccurred, this,
          [this, proc](QProcess::ProcessError) {
    if (d->python_verify_proc != proc) {
      proc->deleteLater();
      return;
    }
    d->python_verify_proc = nullptr;
    setPythonStatus(3);
    d->python_status_label->setText(tr("Interpreter not found"));
    d->python_status_label->setStyleSheet(
        QStringLiteral("color: palette(mid); font-style: italic;"));
    proc->deleteLater();
  });

  proc->start();
}

void AgentConfigPanel::populateModels(const QString &backend_type) {
  d->model_combo->clear();
  QStringList presets;
  if (backend_type == QStringLiteral("claude")) {
    presets = {QStringLiteral("claude-opus-4-20250514"),
               QStringLiteral("claude-sonnet-4-20250514"),
               QStringLiteral("claude-haiku-4-5-20251001")};
  } else if (backend_type == QStringLiteral("openai")) {
    presets = {QStringLiteral("gpt-4o"),
               QStringLiteral("gpt-4o-mini"),
               QStringLiteral("o3")};
  } else if (backend_type == QStringLiteral("bedrock")) {
    presets = {QStringLiteral("anthropic.claude-opus-4-20250514-v1:0"),
               QStringLiteral("anthropic.claude-sonnet-4-20250514-v1:0"),
               QStringLiteral("anthropic.claude-haiku-4-5-20251001-v1:0")};
  }
  d->model_combo->addItems(presets);

  // Repopulate role combos with same presets.
  auto repopulate_role = [&](QComboBox *combo) {
    auto saved = combo->currentText();
    auto was_same = (combo->currentIndex() == 0);
    combo->clear();
    combo->addItem(tr("Same as primary"));
    combo->addItems(presets);
    if (was_same || saved.isEmpty()) {
      combo->setCurrentIndex(0);
    } else {
      auto idx = combo->findText(saved);
      if (idx >= 0) {
        combo->setCurrentIndex(idx);
      } else {
        combo->setEditText(saved);
      }
    }
  };
  repopulate_role(d->recommender_model_combo);
  repopulate_role(d->summarizer_model_combo);
  repopulate_role(d->observer_model_combo);
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
