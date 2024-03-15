// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Registry.h>

#include <memory>

#include <QAbstractItemModel>

namespace mx::gui {

class ConfigModel final : public QAbstractItemModel {
  Q_OBJECT

 public:
  struct Error final {
    QString module_name;
    QString key_name;
    QString localized_key_name;
    QString error_message;
  };

  static ConfigModel *Create(Registry &registry, QObject *parent);
  virtual ~ConfigModel(void) override;

  virtual QModelIndex
  index(int row, int column,
        const QModelIndex &parent = QModelIndex()) const override;

  virtual QModelIndex parent(const QModelIndex &index) const override;

  virtual int
  rowCount(const QModelIndex &parent = QModelIndex()) const override;

  virtual int
  columnCount(const QModelIndex &parent = QModelIndex()) const override;

  virtual QVariant data(const QModelIndex &index,
                        int role = Qt::DisplayRole) const override;

  virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

  virtual bool setData(const QModelIndex &index, const QVariant &value,
                       int role = Qt::EditRole) override;

  std::optional<Error> LastError(void) const;

  ConfigModel(ConfigModel &) = delete;
  ConfigModel &operator=(const ConfigModel &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  ConfigModel(Registry &registry, QObject *parent);

 private slots:
  void OnSchemaChange(void);
};

}  // namespace mx::gui
