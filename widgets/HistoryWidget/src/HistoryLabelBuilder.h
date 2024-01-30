/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <cstdint>
#include <multiplier/Index.h>
#include <multiplier/Frontend/File.h>
#include <QObject>
#include <QRunnable>
#include <QString>

namespace mx {
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

class HistoryLabelBuilder Q_DECL_FINAL : public QObject, public QRunnable {
  Q_OBJECT

  const FileLocationCache file_cache;
  const VariantEntity entity;
  const uint64_t item_id;
  const unsigned line;
  const unsigned column;

 public:
  virtual ~HistoryLabelBuilder(void);

  inline HistoryLabelBuilder(const FileLocationCache &file_cache_,
                             VariantEntity entity_, uint64_t item_id_,
                             unsigned line_=0, unsigned column_=0,
                             QObject *parent=nullptr)
      : QObject(parent),
        file_cache(file_cache_),
        entity(std::move(entity_)),
        item_id(item_id_),
        line(line_),
        column(column_) {}

  virtual void run(void) Q_DECL_FINAL;

 signals:
  void LabelForItem(uint64_t item_id, const QString &label);
};

}  // namespace mx::gui
