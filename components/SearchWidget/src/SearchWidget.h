/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ISearchWidget.h>

namespace mx::gui {

//! The main class implementing the ISearchWidget interface
class SearchWidget final : public ISearchWidget {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~SearchWidget() override;

  //! \copybrief ISearchWidget::UpdateSearchResultCount
  virtual void
  UpdateSearchResultCount(const std::size_t &search_result_count) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  SearchWidget(Mode mode, QWidget *parent);

  //! Loads the required icons from the resources
  void LoadIcons();

  //! Initializes the internal widgets
  void InitializeWidgets();

  //! Shows the given message in the message display
  void SetDisplayMessage(bool error, const QString &message);

  //! Clears and hides the message display
  void ClearDisplayMessage();

 private slots:
  //! Called whenever the search pattern changes
  void OnSearchInputTextChanged(const QString &text);

  //! Called when the case sensitivity option is toggled
  void OnCaseSensitiveSearchOptionToggled(bool checked);

  //! Called when the whole word option is toggled
  void OnWholeWordSearchOptionToggled(bool checked);

  //! Toggled when the regex option is toggled
  void OnRegexSearchOptionToggled(bool checked);

 public slots:
  //! \copybrief ISearchWidget::Activate
  virtual void Activate() override;

  //! \copybrief ISearchWidget::Deactivate
  virtual void Deactivate() override;

  friend class ISearchWidget;
};

}  // namespace mx::gui
