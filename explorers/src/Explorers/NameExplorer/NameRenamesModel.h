// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QAbstractTableModel>
#include <QMap>
#include <QString>
#include <QVector>

#include <multiplier/Types.h>

#include <vector>

namespace mx::gui {

struct RenameEntry {
  RawEntityId canonical_id{kInvalidEntityId};
  QString original_name;
  QString new_name;
  QString kind;
  QString location;
  QVector<RawEntityId> all_ids;
};

class NameRenamesModel Q_DECL_FINAL : public QAbstractTableModel {
  Q_OBJECT

 public:
  virtual ~NameRenamesModel(void);

  explicit NameRenamesModel(QObject *parent = nullptr);

  void addRename(RawEntityId canonical_id, const QString &original_name,
                 const QString &new_name, const QString &kind,
                 const QString &location,
                 const QVector<RawEntityId> &all_ids);

  void removeRename(RawEntityId canonical_id);

  void clear(void);

  QMap<RawEntityId, QString> buildRenameMap(void) const;

  int findRow(RawEntityId canonical_id) const;

  // QAbstractTableModel overrides.
  int rowCount(const QModelIndex &parent = {}) const Q_DECL_FINAL;
  int columnCount(const QModelIndex &parent = {}) const Q_DECL_FINAL;
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const Q_DECL_FINAL;

  const std::vector<RenameEntry> &entries(void) const { return entries_; }

 signals:
  void renamesChanged(const QMap<RawEntityId, QString> &renames);

 private:
  void emitRenamesChanged(void);
  std::vector<RenameEntry> entries_;
};

}  // namespace mx::gui
