/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/GUI/Interfaces/IGeneratedItem.h>

#include <QVector>
#include <QRunnable>
#include <QObject>

#include <atomic>
#include <memory>

namespace mx::gui {

static constexpr int kBatchIntervalTime{150};
static constexpr int kMaxBatchSize{150};

class ITreeGenerator;

class IGenerateTreeRunnable : public QObject, public QRunnable {
  Q_OBJECT

 protected:
  const std::shared_ptr<ITreeGenerator> generator;
  const std::atomic_uint64_t &version_number;
  const uint64_t captured_version_number;
  const IGeneratedItemPtr parent_item;
  
  // Some kind of identifier for what the parent item node is in the underlying
  // model.
  const uint64_t parent_item_id;

  const unsigned depth;

 public:
  virtual ~IGenerateTreeRunnable(void);
  
  inline explicit IGenerateTreeRunnable(
      std::shared_ptr<ITreeGenerator> generator_,
      const std::atomic_uint64_t &version_number_,
      IGeneratedItemPtr parent_item_, uint64_t parent_item_id_, unsigned depth_)
      : generator(std::move(generator_)),
        version_number(version_number_),
        captured_version_number(version_number.load()),
        parent_item(std::move(parent_item_)),
        parent_item_id(parent_item_id_),
        depth(depth_) {
    setAutoDelete(true);        
  }

 signals:
  void NewGeneratedItems(uint64_t version_number, uint64_t parent_item_id,
                         QVector<IGeneratedItemPtr> child_items,
                         unsigned remaining_depth);

  void Finished(void);
};

}  // namespace mx::gui
