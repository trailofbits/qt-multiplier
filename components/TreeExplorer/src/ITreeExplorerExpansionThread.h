/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ITreeGenerator.h>

#include <multiplier/Types.h>

#include <QRunnable>
#include <QObject>

namespace mx::gui {

using VersionNumber = std::shared_ptr<std::atomic<uint64_t>>;

class ITreeExplorerExpansionThread : public QObject, public QRunnable {
  Q_OBJECT

 protected:
  struct ThreadData;
  std::unique_ptr<ThreadData> d;

 public:
  virtual ~ITreeExplorerExpansionThread(void);
  explicit ITreeExplorerExpansionThread(
      std::shared_ptr<ITreeGenerator> generator_,
      const VersionNumber &version_number, RawEntityId parent_entity_id,
      unsigned depth);

 signals:
  void NewTreeItems(uint64_t version_number, RawEntityId parent_entity_id,
                    QList<std::shared_ptr<ITreeItem>> child_items,
                    unsigned remaining_depth);
};

struct ITreeExplorerExpansionThread::ThreadData {
  const std::shared_ptr<ITreeGenerator> generator;
  const VersionNumber version_number;
  const uint64_t captured_version_number;
  const RawEntityId parent_entity_id;
  const unsigned depth;

  inline ThreadData(std::shared_ptr<ITreeGenerator> generator_,
                    const VersionNumber &version_number_,
                    RawEntityId parent_entity_id_, unsigned depth_)
      : generator(std::move(generator_)),
        version_number(version_number_),
        captured_version_number(version_number->load()),
        parent_entity_id(parent_entity_id_),
        depth(depth_) {}
};

}  // namespace mx::gui
