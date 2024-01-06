/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CategoryComboBox.h"

#include <QComboBox>
#include <QVBoxLayout>

#include <unordered_map>


#include <multiplier/Frontend/TokenCategory.h>
#include <multiplier/Iterator.h>

namespace mx::gui {

namespace {

template <typename Callback>
static void EnumerateTokenCategories(Callback callback) {
  for (auto token_category : EnumerationRange<TokenCategory>()) {
    callback(token_category);
  }
}

static const std::unordered_map<TokenCategory, QString> kLabelMap = {
    {TokenCategory::UNKNOWN, QObject::tr("Unknown/Other")},
    {TokenCategory::IDENTIFIER, QObject::tr("Identifier")},
    {TokenCategory::MACRO_NAME, QObject::tr("Macro name")},
    {TokenCategory::MACRO_PARAMETER_NAME,
     QObject::tr("Macro parameter name")},
    {TokenCategory::MACRO_DIRECTIVE_NAME,
     QObject::tr("Macro directive name")},
    {TokenCategory::KEYWORD, QObject::tr("Keyword")},
    {TokenCategory::OBJECTIVE_C_KEYWORD, QObject::tr("Objective-C keyboard")},
    {TokenCategory::BUILTIN_TYPE_NAME, QObject::tr("Builtin type name")},
    {TokenCategory::PUNCTUATION, QObject::tr("Punctuation")},
    {TokenCategory::LITERAL, QObject::tr("Literal")},
    {TokenCategory::COMMENT, QObject::tr("Comment")},
    {TokenCategory::LOCAL_VARIABLE, QObject::tr("Local variable")},
    {TokenCategory::GLOBAL_VARIABLE, QObject::tr("Global variable")},
    {TokenCategory::PARAMETER_VARIABLE, QObject::tr("Parameter variable")},
    {TokenCategory::FUNCTION, QObject::tr("Function")},
    {TokenCategory::INSTANCE_METHOD, QObject::tr("Instance method")},
    {TokenCategory::INSTANCE_MEMBER, QObject::tr("Instance member")},
    {TokenCategory::CLASS_METHOD, QObject::tr("Class method")},
    {TokenCategory::CLASS_MEMBER, QObject::tr("Class member")},
    {TokenCategory::THIS, QObject::tr("This")},
    {TokenCategory::CLASS, QObject::tr("Class")},
    {TokenCategory::STRUCT, QObject::tr("Struct")},
    {TokenCategory::UNION, QObject::tr("Union")},
    {TokenCategory::CONCEPT, QObject::tr("Concept")},
    {TokenCategory::INTERFACE, QObject::tr("Interface")},
    {TokenCategory::ENUM, QObject::tr("Enum")},
    {TokenCategory::ENUMERATOR, QObject::tr("Enumerator")},
    {TokenCategory::NAMESPACE, QObject::tr("Namespace")},
    {TokenCategory::TYPE_ALIAS, QObject::tr("Type alias")},
    {TokenCategory::TEMPLATE_PARAMETER_TYPE,
     QObject::tr("Template parameter type")},
    {TokenCategory::TEMPLATE_PARAMETER_VALUE,
     QObject::tr("Template parameter value")},
    {TokenCategory::LABEL, QObject::tr("Label")},
    {TokenCategory::WHITESPACE, QObject::tr("Whitespace")},
    {TokenCategory::FILE_NAME, QObject::tr("File name")},
    {TokenCategory::LINE_NUMBER, QObject::tr("Line number")},
    {TokenCategory::COLUMN_NUMBER, QObject::tr("Column number")},
};

const QString &GetTokenCategoryLabel(const TokenCategory &token_category) {
  auto it = kLabelMap.find(token_category);
  Q_ASSERT(it != kLabelMap.end());
  const auto &label = it->second;
  return label;
}

}  // namespace

struct CategoryComboBox::PrivateData final {
  QComboBox *combo_box{nullptr};
};

CategoryComboBox::CategoryComboBox(QWidget *parent)
    : QWidget(parent),
      d(new PrivateData) {
  InitializeWidgets();
}

CategoryComboBox::~CategoryComboBox(void) {}

void CategoryComboBox::Reset() {
  emit CategoryChanged(std::nullopt);
}

void CategoryComboBox::InitializeWidgets(void) {
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  d->combo_box = new QComboBox(this);
  layout->addWidget(d->combo_box);

  setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  d->combo_box->addItem(tr("All"), QVariant());

  EnumerateTokenCategories([&](const auto &token_category) {
    QVariant value;
    value.setValue(token_category);

    const auto &label = GetTokenCategoryLabel(token_category);
    d->combo_box->addItem(label, value);
  });

  connect(d->combo_box, &QComboBox::currentIndexChanged, this,
          &CategoryComboBox::OnCurrentIndexChange);
}

void CategoryComboBox::OnCurrentIndexChange(void) {
  auto token_category_var = d->combo_box->currentData();
  if (!token_category_var.isValid()) {
    emit CategoryChanged(std::nullopt);
  } else {
    emit CategoryChanged(qvariant_cast<TokenCategory>(token_category_var));
  }
}

}  // namespace mx::gui
