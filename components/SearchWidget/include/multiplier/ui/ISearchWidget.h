/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

namespace mx::gui {

//! A reusable search widget
class ISearchWidget : public QWidget {
  Q_OBJECT

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

  //! Factory function
  static ISearchWidget *Create(Mode mode, QWidget *parent = nullptr);

  //! Constructor
  ISearchWidget(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~ISearchWidget() override = default;

  //! Called by the other widget to update the search result count
  virtual void
  UpdateSearchResultCount(const std::size_t &search_result_count) = 0;

  //! Disable the copy constructor
  ISearchWidget(const ISearchWidget &) = delete;

  //! Disable copy assignment
  ISearchWidget &operator=(const ISearchWidget &) = delete;

 public slots:
  //! Activates the search widget
  virtual void Activate() = 0;

  //! Deactivates the search widget
  virtual void Deactivate() = 0;

 signals:
  //! Emitted when search parameters have been changed
  void SearchParametersChanged(const SearchParameters &search_parameters);

  //! Emitted when the user presses the prev/next buttons
  void SearchResultSelected(const std::size_t &index);
};

}  // namespace mx::gui
