/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Index.h>
#include <multiplier/Types.h>

#include <QAbstractItemModel>
#include <QRegularExpression>

#include <optional>
#include <unordered_set>

namespace mx::gui {

//! A model for the entity explorer widget
class IEntityExplorerModel : public QAbstractItemModel {
  Q_OBJECT

 public:
  //! Additional item data roles for this model
  enum ItemDataRole {
    //! Returns a Token object
    TokenRole = Qt::UserRole + 1,
  };

  //! Search modes supported by the `IEntityExplorerModel::Search` method
  enum class SearchMode {
    ExactMatch,
    Containing,
  };

  //! Factory method
  static IEntityExplorerModel *
  Create(const Index &index, const FileLocationCache &file_location_cache,
         QObject *parent = nullptr);

  //! Constructor
  IEntityExplorerModel(QObject *parent) : QAbstractItemModel(parent) {}

  //! Destructor
  virtual ~IEntityExplorerModel() override = default;

  //! Sorting method
  enum class SortingMethod {
    Ascending,
    Descending,
  };

  //! Sets the sorting method
  virtual void SetSortingMethod(const SortingMethod &sorting_method) = 0;

  //! Sets the given regular expression as a filter
  virtual void SetFilterRegularExpression(const QRegularExpression &regex) = 0;

  //! Token category set used as filter
  using TokenCategorySet = std::unordered_set<TokenCategory>;

  //! Sets the token category filter
  virtual void SetTokenCategoryFilter(
      const std::optional<TokenCategorySet> &opt_token_category_set) = 0;

  //! Disabled copy constructor
  IEntityExplorerModel(const IEntityExplorerModel &) = delete;

  //! Disabled copy assignment operator
  IEntityExplorerModel &operator=(const IEntityExplorerModel &) = delete;

 public slots:
  //! Starts a new search request
  virtual void Search(const QString &name, const SearchMode &search_mode) = 0;

  //! Cancels the active search, if any
  virtual void CancelSearch() = 0;

 signals:
  //! Emitted when a new search request is started
  void SearchStarted();

  //! Emitted when the active search terminates or is cancelled
  void SearchFinished();
};

}  // namespace mx::gui
