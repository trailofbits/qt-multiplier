/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QFuture>

namespace mx::gui {

//! A QFuture wrapper that automatically cancels the future
template <typename ResultType>
class DatabaseFuture final : public QFuture<ResultType> {
 public:
  //! Constructor
  DatabaseFuture();

  //! Destructor
  virtual ~DatabaseFuture() override;
};

template <typename ResultType>
DatabaseFuture<ResultType>::DatabaseFuture() {}

template <typename ResultType>
DatabaseFuture<ResultType>::~DatabaseFuture() {
  using QFuture<ResultType>::isRunning;
  using QFuture<ResultType>::cancel;
  using QFuture<ResultType>::waitForFinished;

  if (!isRunning()) {
    return;
  }

  cancel();
  waitForFinished();
}

}  // namespace mx::gui
