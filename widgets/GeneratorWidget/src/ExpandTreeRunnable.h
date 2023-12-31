/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "IGenerateTreeRunnable.h"

namespace mx::gui {

//! A background thread that computes the Nth level of the tree explorer.
class ExpandTreeRunnable Q_DECL_FINAL : public IGenerateTreeRunnable {
  Q_OBJECT

  void run(void) Q_DECL_FINAL;

 public:
  using IGenerateTreeRunnable::IGenerateTreeRunnable;

  virtual ~ExpandTreeRunnable(void) = default;
};

}  // namespace mx::gui
