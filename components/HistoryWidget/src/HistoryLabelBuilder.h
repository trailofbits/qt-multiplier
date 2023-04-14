/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <cstdint>
#include <memory>
#include <multiplier/Types.h>
#include <QObject>
#include <QRunnable>
#include <QString>

namespace mx {
class Index;
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

class HistoryLabelBuilder final : public QObject, public QRunnable {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~HistoryLabelBuilder(void);

  explicit HistoryLabelBuilder(const Index &index_,
                               const FileLocationCache &file_cache_,
                               RawEntityId id_, std::uint64_t item_id_,
                               QObject *parent=nullptr);

  virtual void run(void) Q_DECL_FINAL;

 signals:
  void LabelForItem(std::uint64_t item_id, const QString &label);
};

}  // namespace mx::gui
