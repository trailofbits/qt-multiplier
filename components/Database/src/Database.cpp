// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Database.h"

#include <requests/IndexedTokenRange.h>

#include <QThreadPool>
#include <QtConcurrent>
#include <thread>

namespace mx::gui {

namespace {

void ExecuteRequest(QPromise<Database::Result> &result_promise,
                    const Index &index,
                    const FileLocationCache &file_location_cache,
                    const Request &request) {
  CreateIndexedTokenRangeData(result_promise, index, file_location_cache,
                              request);
}

}  // namespace

struct Database::PrivateData {
  PrivateData(const Index &index_,
              const FileLocationCache &file_location_cache_)
      : index(std::move(index_)),
        file_location_cache(file_location_cache_) {}

  const Index index;
  const FileLocationCache file_location_cache;

  QThreadPool thread_pool;
};

Database::~Database() {}

Database::FutureResult Database::DownloadFile(RawEntityId file_id) {

  Request request{SingleEntityRequest{
      DownloadRequestType::FileTokens,
      file_id,
  }};

  return QtConcurrent::run(&d->thread_pool, ExecuteRequest, d->index,
                           d->file_location_cache, std::move(request));
}

Database::FutureResult
Database::DownloadFragment(RawEntityId fragment_id) {

  Request request{SingleEntityRequest{
      DownloadRequestType::FragmentTokens,
      fragment_id,
  }};

  return QtConcurrent::run(&d->thread_pool, ExecuteRequest, d->index,
                           d->file_location_cache, std::move(request));
}

Database::Database(const Index &index,
                   const FileLocationCache &file_location_cache)
    : d(new PrivateData(index, file_location_cache)) {

  d->thread_pool.setMaxThreadCount(
      static_cast<int>(std::thread::hardware_concurrency()));
}

}  // namespace mx::gui
