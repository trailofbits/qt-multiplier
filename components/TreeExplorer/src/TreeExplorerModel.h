/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QRunnable>

#include <multiplier/Types.h>
#include <multiplier/ui/ITreeExplorerModel.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <unordered_map>

namespace mx::gui {

class ITreeExplorerExpansionThread;

//! Implements the IReferenceExplorerModel interface
class TreeExplorerModel final : public ITreeExplorerModel {
  Q_OBJECT

  struct PrivateData;

  std::unique_ptr<PrivateData> d;

 public:

  //! Constructor
  TreeExplorerModel(QObject *parent);

  //! Destructor
  virtual ~TreeExplorerModel(void);

  //! Install a new generator to back the data of this model.
  void InstallGenerator(std::shared_ptr<ITreeGenerator> generator_) Q_DECL_FINAL;

  //! Expands the given entity
  void ExpandEntity(const QModelIndex &index, unsigned depth);

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
  void OnNewTreeItems(uint64_t version_number,
                      RawEntityId parent_entity_id,
                      QList<std::shared_ptr<ITreeItem>> child_items,
                      unsigned remaining_depth);

  //! Processes the entire data batch queue
  void ProcessDataBatchQueue(void);

  //! Called when the tree title has been resolved.
  void OnNameResolved(void);

 public slots:
  void CancelRunningRequest() Q_DECL_FINAL;
};

using VersionNumber = std::shared_ptr<std::atomic<uint64_t>>;

class ITreeExplorerExpansionThread : public QObject, public QRunnable {
  Q_OBJECT
 
 protected:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~ITreeExplorerExpansionThread(void);
  explicit ITreeExplorerExpansionThread(
      std::shared_ptr<ITreeGenerator> generator_,
      const VersionNumber &version_number,
      RawEntityId parent_entity_id, unsigned depth);

 signals:
  void NewTreeItems(uint64_t version_number,
                    RawEntityId parent_entity_id,
                    QList<std::shared_ptr<ITreeItem>> child_items,
                    unsigned remaining_depth);
};

//! A background thread that computes the first level of the tree explorer.
class InitTreeExplorerThread final : public ITreeExplorerExpansionThread {
  Q_OBJECT

  void run(void) Q_DECL_FINAL;

 public:
  using ITreeExplorerExpansionThread::ITreeExplorerExpansionThread;

  virtual ~InitTreeExplorerThread(void) = default;
};

//! A background thread that computes the Nth level of the tree explorer.
class ExpandTreeExplorerThread final : public ITreeExplorerExpansionThread {
  Q_OBJECT

  void run(void) Q_DECL_FINAL;

 public:
  using ITreeExplorerExpansionThread::ITreeExplorerExpansionThread;

  virtual ~ExpandTreeExplorerThread(void) = default;
};

}  // namespace mx::gui
