/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <QAction>
#include <QSize>
#include <QString>
#include <QWidget>

#include <memory>
#include <multiplier/Types.h>
#include <optional>

namespace mx {
class Index;
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

class HistoryWidget final : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 private:
  void InitializeWidgets(void);
  void UpdateMenus(void);

 public:
  HistoryWidget(const Index &index_,
                const FileLocationCache &file_cache_,
                unsigned max_history_size=30,
                QWidget *parent=nullptr);

  virtual ~HistoryWidget(void);

  //! Set the icon size.
  void SetIconSize(QSize size);

  //! Tells the history what our current location is.
  void SetCurrentLocation(RawEntityId id,
                          std::optional<QString> opt_label=std::nullopt);

  //! Commits our "last current" location to the history. This makes our last
  //! current location visible in the history menu.
  void CommitCurrentLocationToHistory(void);

 signals:
  void GoToEntity(RawEntityId original_id, RawEntityId canonical_id);

 private slots:
  void OnNavigateBack(void);
  void OnNavigateForward(void);
  void OnNavigateBackToHistoryItem(QAction *action);
  void OnNavigateForwardToHistoryItem(QAction *action);
  void OnLabelForItem(std::uint64_t item_id, const QString &label);
};

}  // namespace mx::gui
