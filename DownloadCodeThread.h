// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QObject>
#include <QRunnable>

#include <multiplier/Types.h>
#include <multiplier/CodeTheme.h>

namespace mx::gui {

// Thread that goes and downloads and structures the relevant code in the
// background.
class DownloadCodeThread final : public QObject, public QRunnable {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void run(void) Q_DECL_FINAL;

  DownloadCodeThread(PrivateData *d_);

 public:
  virtual ~DownloadCodeThread(void);

  static DownloadCodeThread *CreateFileDownloader(
      const Index &index_, const CodeTheme &theme_,
      const FileLocationCache &locs_,
      uint64_t counter, RawEntityId file_id_);

  static DownloadCodeThread *CreateFragmentDownloader(
      const Index &index_, const CodeTheme &theme_,
      const FileLocationCache &locs_,
      uint64_t counter, RawEntityId frag_id_);

  static DownloadCodeThread *CreateTokenRangeDownloader(
      const Index &index_, const CodeTheme &theme_,
      const FileLocationCache &locs_, uint64_t counter,
      RawEntityId begin_tok_id, RawEntityId end_tok_id);

 signals:
  void DownloadFailed(void);
  void RenderCode(void *code, uint64_t counter);
};

}  // namespace mx::gui
