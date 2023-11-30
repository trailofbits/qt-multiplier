/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ITreeExplorerExpansionThread.h"

namespace mx::gui {

//! A background thread that computes the Nth level of the tree explorer.
class ExpandTreeExplorerThread final : public ITreeExplorerExpansionThread {
  Q_OBJECT

  void run(void) Q_DECL_FINAL;

 public:
  using ITreeExplorerExpansionThread::ITreeExplorerExpansionThread;

  virtual ~ExpandTreeExplorerThread(void) = default;
};

}  // namespace mx::gui
