/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include "ITreeExplorerExpansionThread.h"

#include <multiplier/GUI/IGeneratorModel.h>

#include <multiplier/Types.h>

#include <QAbstractItemModel>
#include <QList>
#include <QRunnable>

#include <atomic>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace mx::gui {

class ITreeExplorerExpansionThread;

//! Implements the IReferenceExplorerModel interface
class GeneratorModel final : public IGeneratorModel {
  Q_OBJECT

  struct PrivateData;

  friend struct PrivateData;

  std::unique_ptr<PrivateData> d;

 public:
  //! Constructor
  GeneratorModel(QObject *parent);

  //! Destructor
  virtual ~GeneratorModel(void);

  //! Install a new generator to back the data of this model.
  void
  InstallGenerator(std::shared_ptr<ITreeGenerator> generator_) Q_DECL_FINAL;

  //! Expand starting at the model index, going up to `depth` levels deep.
  void Expand(const QModelIndex &, unsigned depth) Q_DECL_FINAL;

  //! Find the original version of an item.
  QModelIndex Deduplicate(const QModelIndex &) Q_DECL_FINAL;

  //! Creates a new Qt model index
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the parent of the given model index
  QModelIndex parent(const QModelIndex &child) const Q_DECL_FINAL;

  //! Returns the amount or rows in the model
  //! Since this is a tree model, rows are intended as child items
  int rowCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the amount of columns in the model
  int columnCount(const QModelIndex &parent) const Q_DECL_FINAL;

  //! Returns the index data for the specified role
  //! \todo Fix role=TokenCategoryRole support
  QVariant data(const QModelIndex &index, int role) const Q_DECL_FINAL;

  //! Returns the column names for the tree view header
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const Q_DECL_FINAL;

 private:
  void RunExpansionThread(ITreeExplorerExpansionThread *thread);

 private slots:

  //! Notify us when there's a batch of new data to update.
  void OnNewTreeItems(uint64_t version_number, RawEntityId parent_entity_id,
                      QList<std::shared_ptr<ITreeItem>> child_items,
                      unsigned remaining_depth);

  //! Processes the entire data batch queue
  void ProcessDataBatchQueue(void);

  //! Called when the tree title has been resolved.
  void OnNameResolved(void);

 public slots:
  void CancelRunningRequest() Q_DECL_FINAL;
};

}  // namespace mx::gui