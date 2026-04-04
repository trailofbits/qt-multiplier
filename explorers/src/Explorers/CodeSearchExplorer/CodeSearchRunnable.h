// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QRunnable>
#include <QVector>

#include <atomic>
#include <memory>

#include <multiplier/Index.h>
#include <multiplier/Re2.h>

#include "CodeSearchResultsModel.h"

namespace mx::gui {

using AtomicU64 = std::atomic<uint64_t>;
using AtomicU64Ptr = std::shared_ptr<AtomicU64>;

class CodeSearchRunnable Q_DECL_FINAL : public QObject, public QRunnable {
  Q_OBJECT

 public:
  virtual ~CodeSearchRunnable(void);

  explicit CodeSearchRunnable(RegexQuery query_, Index index_,
                              FileLocationCache cache_,
                              AtomicU64Ptr version_);

  void run(void) Q_DECL_FINAL;

 signals:
  void NewResults(uint64_t version, QVector<CodeSearchResultRow> rows);
  void Finished(void);

 private:
  const RegexQuery query;
  const Index index;
  const FileLocationCache file_location_cache;
  const AtomicU64Ptr version_number;
  const uint64_t captured_version_number;
};

}  // namespace mx::gui
