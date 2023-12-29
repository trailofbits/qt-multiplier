/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QAction>
#include <QSize>
#include <QString>
#include <QWidget>

#include <memory>
#include <multiplier/Index.h>
#include <optional>

namespace mx {
class FileLocationCache;
}  // namespace mx
namespace mx::gui {

class MediaManager;

//! \todo Hide the implementation details
class HistoryWidget final : public QWidget {
  Q_OBJECT

  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

 private:
  void InitializeWidgets(QWidget *parent, bool install_global_shortcuts);
  void UpdateMenus(void);
  void UpdateIcons(void);

 public:
  //! Constructor
  //! \param parent The parent widget, where the shortcuts will be installed
  HistoryWidget(const MediaManager &media_manager_,
                const FileLocationCache &file_cache_,
                unsigned max_history_size,
                bool install_global_shortcuts,
                QWidget *parent = nullptr);

  virtual ~HistoryWidget(void);

  //! Set the icon size.
  void SetIconSize(QSize size);

  //! Tells the history what our current location is.
  void SetCurrentLocation(VariantEntity entity,
                          std::optional<QString> opt_label = std::nullopt);

  //! Commits our "last current" location to the history. This makes our last
  //! current location visible in the history menu.
  void CommitCurrentLocationToHistory(void);

 signals:
  void GoToEntity(VariantEntity original_entity,
                  VariantEntity canonical_entity);

 private slots:
  void OnIconsChanged(const MediaManager &media_manager);

  //! Called when the back button is pressed to navigate backward through history.
  //! We distinguish this from the forward menu case because if this is our first
  //! navigation away from our "present location" then we just-in-time materialize
  //! the present location into a history item.
  void OnNavigateBack(void);

  //! Called when the forward button is pressed to navigate forward through
  //! history. We distinguish this from the back menu case, because if we navigate
  //! forward to our "original present" location (i.e. where we started from
  //! before engaging with the history menu) then we just-in-time remove that item
  //! from history. This is so that, upon returning to the present, the present
  //! is allowed to change (with followup clicks and such) without there being a
  //! random record of our "former present" stuck within the history menu.
  void OnNavigateForward(void);

  //! Called when a specific history item in the back button's drop-down menu of
  //! history items is clicked. We distinguish this from the forward menu case
  //! because if this is our first navigation away from our "present location"
  //! then we just-in-time materialize the present location into a history item.
  void OnNavigateBackToHistoryItem(QAction *action);

  //! Called when a specific history item in the forward button's drop-down menu
  //! of history items is clicked. We distinguish this from the back menu case,
  //! because if we navigate forward to our "original present" location (i.e.
  //! where we started from before engaging with the history menu) then we
  //! just-in-time remove that item from history. This is so that, upon returning
  //! to the present, the present is allowed to change (with followup clicks and
  //! such) without there being a random record of our "former present" stuck
  //! within the history menu.
  void OnNavigateForwardToHistoryItem(QAction *action);

  //! Called when we have computed a name for the history item.
  void OnLabelForItem(std::uint64_t item_id, const QString &label);
};

}  // namespace mx::gui
