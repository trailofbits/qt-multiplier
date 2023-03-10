/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QFuture>

namespace mx::gui {

template <typename ResultType>
class DatabaseFuture final : public QFuture<ResultType> {
 public:
  DatabaseFuture();
  virtual ~DatabaseFuture() override;
};

template <typename ResultType>
DatabaseFuture<ResultType>::DatabaseFuture() {}

template <typename ResultType>
DatabaseFuture<ResultType>::~DatabaseFuture() {
  using QFuture<ResultType>::isRunning;
  using QFuture<ResultType>::cancel;
  using QFuture<ResultType>::waitForFinished;

  if (isRunning()) {
    cancel();
    waitForFinished();
  }
}

}  // namespace mx::gui
