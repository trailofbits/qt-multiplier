/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <gap/core/generator.hpp>
#include <QAtomicInt>
#include <QModelIndex>
#include <QObject>
#include <QRunnable>
#include <QVector>

#include <multiplier/ui/IReferenceExplorerModel.h>

namespace mx {
class FileLocationCache;
class Index;
namespace gui {

class INodeGenerator : public QObject, public QRunnable {
  Q_OBJECT

 private:
  QAtomicInt cancel_requested;

 protected:
  QModelIndex parent_index;

 private:
  INodeGenerator(void) = delete;

 public:
  virtual ~INodeGenerator(void);

  inline explicit INodeGenerator(const QModelIndex &index_)
      : parent_index(index_) {}

  void RequestCancel(void);
  bool CancelRequested(void);

  //! Create a node generator for a root node.
  static INodeGenerator *
  CreateRootGenerator(const Index &index, const FileLocationCache &file_cache_,
                      RawEntityId entity_id, const QModelIndex &parent,
                      IReferenceExplorerModel::ExpansionMode expansion_mode);

  //! Create a node generator for a child node.
  static INodeGenerator *
  CreateChildGenerator(const Index &index, const FileLocationCache &file_cache_,
                       RawEntityId entity_id, const QModelIndex &parent,
                       IReferenceExplorerModel::ExpansionMode expansion_mode);

  virtual void run(void) override;

  virtual gap::generator<IReferenceExplorerModel::Node> GenerateNodes(void);

 signals:
  void NodesAvailable(QVector<IReferenceExplorerModel::Node> node, int row,
                      const QModelIndex &parent);

  void Finished(QVector<IReferenceExplorerModel::Node> node, int row,
                const QModelIndex &parent);
};

}  // namespace gui
}  // namespace mx
