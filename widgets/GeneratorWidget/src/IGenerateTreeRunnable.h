/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/GUI/Interfaces/IGeneratedItem.h>

#include <QRunnable>
#include <QObject>

#include <atomic>
#include <memory>

namespace mx::gui {

class ITreeGenerator;

class IGenerateTreeRunnable : public QObject, public QRunnable {
  Q_OBJECT

 protected:
  const std::shared_ptr<ITreeGenerator> generator;
  const std::atomic_uint64_t &version_number;
  const uint64_t captured_version_number;
  const VariantEntity parent_entity;
  const unsigned depth;

 public:
  virtual ~IGenerateTreeRunnable(void);
  
  inline explicit IGenerateTreeRunnable(
      std::shared_ptr<ITreeGenerator> generator_,
      const std::atomic_uint64_t &version_number_,
      VariantEntity parent_entity_, unsigned depth_)
      : generator(std::move(generator_)),
        version_number(version_number_),
        captured_version_number(version_number.load()),
        parent_entity(std::move(parent_entity_)),
        depth(depth_) {
    setAutoDelete(true);        
  }

 signals:
  void NewGeneratedItems(uint64_t version_number, RawEntityId parent_entity_id,
                    QList<IGeneratedItemPtr> child_items,
                    unsigned remaining_depth);
};

}  // namespace mx::gui
