// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QRunnable>

#include <multiplier/Types.h>
#include <multiplier/Index.h>
#include <multiplier/CodeTheme.h>
#include <multiplier/ui/RPC.h>

namespace mx::gui {

// Thread that goes and downloads and structures the relevant code in the
// background.
class DownloadCodeThread final : public QObject, public QRunnable {
  Q_OBJECT

 public:
  virtual ~DownloadCodeThread(void);

  static DownloadCodeThread *
  CreateFileDownloader(const Index &index, const CodeTheme &code_theme,
                       const FileLocationCache &file_location_cache,
                       const std::uint64_t &counter,
                       const RawEntityId &file_id);

  static DownloadCodeThread *
  CreateFragmentDownloader(const Index &index, const CodeTheme &code_theme,
                           const FileLocationCache &file_location_cache,
                           const std::uint64_t &counter,
                           const RawEntityId &fragment_id);

  static DownloadCodeThread *
  CreateTokenRangeDownloader(const Index &index, const CodeTheme &code_theme,
                             const FileLocationCache &file_location_cache,
                             const std::uint64_t &counter,
                             const RawEntityId &start_entity_id,
                             const RawEntityId &end_entity_id);

  DownloadCodeThread(const DownloadCodeThread &) = delete;
  DownloadCodeThread &operator=(const DownloadCodeThread &) = delete;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  struct SingleEntityRequest final {
    DownloadRequestType download_request_type;
    RawEntityId entity_id;
  };

  struct EntityRangeRequest final {
    RawEntityId start_entity_id;
    RawEntityId end_entity_id;
  };

  using Request = std::variant<SingleEntityRequest, EntityRangeRequest>;

  DownloadCodeThread(const Index &index, const CodeTheme &code_theme,
                     const FileLocationCache &file_location_cache,
                     uint64_t counter, const Request &request);

  virtual void run(void) Q_DECL_FINAL;

 signals:
  void DownloadFailed(void);
  void RenderCode(void *code, uint64_t counter);
};

}  // namespace mx::gui
