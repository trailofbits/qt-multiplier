// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ConfigEditorDelegate.h"

#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPainter>

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

ConfigEditorDelegate::ConfigEditorDelegate(QWidget *parent)
    : d(new PrivateData) {}

}  // namespace mx::gui
