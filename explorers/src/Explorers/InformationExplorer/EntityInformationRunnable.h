// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Interfaces/IInfoGenerator.h>

#include <QVector>
#include <QObject>
#include <QRunnable>
#include <QString>

#include <atomic>

namespace mx::gui {

static constexpr size_t kMaxBatchSize = 250u;

using AtomicU64 = std::atomic<uint64_t>;
using AtomicU64Ptr = std::shared_ptr<AtomicU64>;

class EntityInformationRunnable Q_DECL_FINAL : public QObject, public QRunnable {
  Q_OBJECT

 protected:
  // The genenerator for this category of entity information.
  const IInfoGeneratorPtr generator;

  // Passed to the generator to help it compute locations.
  const FileLocationCache file_location_cache;

  // Used to keep track of if fetching the information needs to still happen.
  const AtomicU64Ptr version_number;
  const uint64_t captured_version_number;

 public:
  virtual ~EntityInformationRunnable(void);
  
  inline explicit EntityInformationRunnable(
      IInfoGeneratorPtr generator_,
      FileLocationCache file_location_cache_,
      AtomicU64Ptr version_number_)
      : generator(std::move(generator_)),
        file_location_cache(std::move(file_location_cache_)),
        version_number(std::move(version_number_)),
        captured_version_number(version_number->load()) {
    setAutoDelete(true);        
  }

  void run(void) Q_DECL_FINAL;

 signals:
  void NewGeneratedItems(
      uint64_t version_number, QVector<IInfoGenerator::Item> child_items);

  void Finished(void);
};

}  // namespace mx::gui
