// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IRPC.h>

namespace mx::gui {

class RPC final : public IRPC {
 public:
  virtual ~RPC() override;

  virtual FutureResult DownloadFile(const RawEntityId &file_id) override;

  virtual FutureResult
  DownloadFragment(const RawEntityId &fragment_id) override;

  virtual FutureResult
  DownloadTokenRange(const RawEntityId &start_entity_id,
                     const RawEntityId &end_entity_id) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  RPC(const Index &index, const FileLocationCache &file_location_cache);

  friend class IRPC;
};

}  // namespace mx::gui
