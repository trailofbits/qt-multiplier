// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/ui/IDatabase.h>

namespace mx::gui {

class Database final : public IDatabase {
 public:
  virtual ~Database() override;

  virtual QFuture<FileResult> DownloadFile(const RawEntityId &file_id) override;
  virtual QFuture<FileResult>
  DownloadFragment(const RawEntityId &fragment_id) override;

  virtual QFuture<std::optional<QString>>
  GetEntityName(const RawEntityId &fragment_id) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  Database(const Index &index, const FileLocationCache &file_location_cache);

  friend class IDatabase;
};

}  // namespace mx::gui
