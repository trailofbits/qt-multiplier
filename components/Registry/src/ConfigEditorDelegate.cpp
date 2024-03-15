// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ConfigEditorDelegate.h"
#include "ConfigModel.h"

#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPainter>
#include <QMessageBox>
#include <QSortFilterProxyModel>

namespace mx::gui {

struct ConfigEditorDelegate::PrivateData final {};

ConfigEditorDelegate *ConfigEditorDelegate::Create(QWidget *parent) {
  return new ConfigEditorDelegate(parent);
}

ConfigEditorDelegate::~ConfigEditorDelegate(void) {}

QWidget *ConfigEditorDelegate::createEditor(QWidget *parent,
                                            const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {

  switch (index.data().userType()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::Short:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
    case QMetaType::UShort: {
      return new QSpinBox(parent);
    }

    case QMetaType::Bool: {
      auto combo_box = new QComboBox(parent);
      combo_box->addItem(tr("false"), false);
      combo_box->addItem(tr("true"), true);

      return combo_box;
    }

    case QMetaType::QString: {
      return new QLineEdit(parent);
    }

    default: {
      return nullptr;
    }
  }
}

void ConfigEditorDelegate::setEditorData(QWidget *editor,
                                         const QModelIndex &index) const {
  switch (index.data().userType()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::LongLong:
    case QMetaType::Short:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
    case QMetaType::UShort: {
      auto type_editor{static_cast<QSpinBox *>(editor)};
      type_editor->setValue(index.data().toInt());
      break;
    }

    case QMetaType::Bool: {
      auto item_index = index.data().toBool() ? 1 : 0;

      auto type_editor{static_cast<QComboBox *>(editor)};
      type_editor->setCurrentIndex(item_index);

      break;
    }

    case QMetaType::QString: {
      auto type_editor{static_cast<QLineEdit *>(editor)};
      type_editor->setText(index.data().toString());
      break;
    }

    default: {
      break;
    }
  }
}

void ConfigEditorDelegate::setModelData(QWidget *editor,
                                        QAbstractItemModel *model,
                                        const QModelIndex &index) const {
  QStyledItemDelegate::setModelData(editor, model, index);

  auto proxy_model = static_cast<const QSortFilterProxyModel *>(model);
  auto &config_model = *static_cast<ConfigModel *>(proxy_model->sourceModel());

  if (auto opt_last_error = config_model.LastError();
      opt_last_error.has_value()) {
    const auto &last_error = opt_last_error.value();

    QString error_message{last_error.error_message};
    if (error_message.isEmpty()) {
      error_message = tr(
          "The value could not be set but no detailed error message was available");
    }

    QMessageBox::critical(editor, tr("Error"), error_message);
  }
}

ConfigEditorDelegate::ConfigEditorDelegate(QWidget *parent)
    : d(new PrivateData) {}

}  // namespace mx::gui
