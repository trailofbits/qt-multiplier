/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "CategoryComboBox.h"

#include <multiplier/GUI/Assert.h>

#include <QComboBox>
#include <QVBoxLayout>

#include <unordered_map>

namespace mx::gui {

namespace {

void EnumerateTokenCategories(
    const std::function<void(const TokenCategory &token_category)> &callback) {

  for (const auto &token_category : {TokenCategory::UNKNOWN,
                                     TokenCategory::MACRO_NAME,
                                     TokenCategory::GLOBAL_VARIABLE,
                                     TokenCategory::FUNCTION,
                                     TokenCategory::INSTANCE_METHOD,
                                     TokenCategory::INSTANCE_MEMBER,
                                     TokenCategory::CLASS_METHOD,
                                     TokenCategory::CLASS_MEMBER,
                                     TokenCategory::CLASS,
                                     TokenCategory::STRUCT,
                                     TokenCategory::UNION,
                                     TokenCategory::CONCEPT,
                                     TokenCategory::INTERFACE,
                                     TokenCategory::ENUM,
                                     TokenCategory::ENUMERATOR,
                                     TokenCategory::TYPE_ALIAS}) {
    callback(token_category);
  }
}

const QString &GetTokenCategoryLabel(const TokenCategory &token_category) {

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

  auto it = kLabelMap.find(token_category);
  Assert(it != kLabelMap.end(), "Invalid token category");

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

CategoryComboBox::~CategoryComboBox() {}

void CategoryComboBox::Reset() {
  emit CategoryChanged(std::nullopt);
}

void CategoryComboBox::InitializeWidgets() {
  auto layout = new QVBoxLayout();
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

void CategoryComboBox::OnCurrentIndexChange() {
  std::optional<TokenCategory> opt_token_category;

  auto token_catery_var = d->combo_box->currentData();
  if (token_catery_var.isValid()) {
    opt_token_category = qvariant_cast<TokenCategory>(token_catery_var);
  }

  emit CategoryChanged(opt_token_category);
}

}  // namespace mx::gui
