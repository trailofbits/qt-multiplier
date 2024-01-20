/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QPalette>
#include <QWidget>

#include <memory>
#include <string>

namespace mx::gui {

class MediaManager;

//! A reusable search widget
class SearchWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

 public:
  //! Search parameters, such as pattern and pattern type
  struct SearchParameters final {
    enum class Type {
      Text,
      RegularExpression,
    };

    Type type{Type::Text};
    bool whole_word{false};
    bool case_sensitive{false};
    std::string pattern;
  };

  //! Search widget mode
  enum class Mode {
    //! In search mode, the show prev/show next buttons are shown
    Search,

    //! In filter mode, not show prev/show next button is shown
    Filter,
  };

  //! Destructor
  virtual ~SearchWidget(void);

  //! Constructor
  SearchWidget(const MediaManager &media_manager, Mode mode,
               QWidget *parent = nullptr);

  //! Called by the other client widget to update the search result count
  void UpdateSearchResultCount(size_t search_result_count);

  //! Return the current search paramters.
  const SearchParameters &Parameters(void) const;

 private:

  //! Loads the required icons from the resources
  void LoadIcons(const MediaManager &media_manager);

  //! Initializes the internal widgets
  void InitializeWidgets(void);

  //! Initializes the keyboard shortcuts on the parent widget
  void InitializeKeyboardShortcuts(QWidget *parent);

  //! Shows the given message in the message display
  void SetDisplayMessage(bool error, const QString &message);

  //! Clears and hides the message display
  void ClearDisplayMessage(void);

  //! Helper method for OnShowPreviousResult and OnShowNextResult
  void ShowResult(void);

  //! Updates the icons based on the active theme
  void UpdateIcons(void);

 private slots:
  //! Called whenever the search pattern changes
  void OnSearchInputTextChanged(const QString &text);

  //! Called when the case sensitivity option is toggled
  void OnCaseSensitiveSearchOptionToggled(bool checked);

  //! Called when the whole word option is toggled
  void OnWholeWordSearchOptionToggled(bool checked);

  //! Toggled when the regex option is toggled
  void OnRegexSearchOptionToggled(bool checked);

  //! Called when the show prev result button is pressed
  void OnShowPreviousResult(void);

  //! Called when the show next result button is pressed
  void OnShowNextResult(void);

  //! Called when the theme changes.
  void OnIconsChanged(const MediaManager &media_manager);

  //! Disable this wiget.
  void Disable(void);

 public slots:

  //! Activates the search widget
  void Activate(void);

  //! Deactivates the search widget
  void Deactivate(void);

 signals:
  //! Emitted when the widget has been activated
  void Activated(void);

  //! Emitted when the widget has been deactivated
  void Deactivated(void);

  //! Emitted when search parameters have been changed
  void SearchParametersChanged(void);

  //! Emitted when the user presses the prev/next buttons
  void SearchResultSelected(size_t index);

  //! Emitted when a new search result should be made active
  void ShowSearchResult(size_t result_index);
};

}  // namespace mx::gui
