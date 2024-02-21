/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "SortFilterProxyModel.h"
#include "EntityInformationModel.h"

#include <multiplier/GUI/Util.h>

namespace mx::gui {

namespace {

QMap<QString, int> GetCategorySortingOrderMap() {
  static const char *kCategoryList[] {
    "Size",
    "Members",
    "Enumerators",
    "Enums",
    "Classes",
    "Structures",
    "Type",
    "Types",
    "Unions",
    "Top Level Entities",
    "Static Local Variables",
    "Address Ofs",
    "Align Ofs",
    "Address Taken By",
    "Functions",
    "Class Methods",
    "Instance Methods",
    "Constructors",
    "Destructors",
    "Concepts",
    "Conversion Operators",
    "Copied Into",
    "Declarations",
    "Declaration Uses",
    "Deduction Guides",
    "Defined Macros",
    "Definitions",
    "Dereferenced By",
    "Called By",
    "Callees",
    "Callers",
    "Casted By",
    "Expansions",
    "Global Variables",
    "Included By",
    "Includes",
    "Interfaces",
    "Local Variables",
    "Macros Used",
    "Overloaded Operators",
    "Parameters",
    "Parentage",
    "Passed As Argument To",
    "Security Type Traits",
    "Size Ofs",
    "Statement Uses",
    "Tested By",
    "Thread Local Variables",
    "Trait Uses",
    "Type Casts",
    "Updated By",
    "Used By",
    "Written By",
    "Users",
    "Vector Type Traits",
  };

  QMap<QString, int> category_map;
  for (int i = 0; i < sizeof(kCategoryList) / sizeof(const char *); ++i) {
    category_map.insert(QObject::tr(kCategoryList[i]), i);
  }

  return category_map;
}

}

SortFilterProxyModel::SortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

SortFilterProxyModel::~SortFilterProxyModel(void) {}

void SortFilterProxyModel::setSourceModel(QAbstractItemModel *source_model) {
  Q_ASSERT(!sourceModel());

  QSortFilterProxyModel::setSourceModel(source_model);

  // connect(source_model, &QAbstractItemModel::rowsAboutToBeInserted,
  //         this, &SortFilterProxyModel::OnBeginInsertRows);

  // connect(source_model, &QAbstractItemModel::dataChanged,
  //         this, &SortFilterProxyModel::OnDataChanged);
}

bool SortFilterProxyModel::lessThan(const QModelIndex &source_left,
                                    const QModelIndex &source_right) const {

  // The original `struct Node` can now be acquired from the model
  // indexes for additional sorting methods
  auto sort_role = sortRole();
  if (!source_left.parent().isValid()) {
    // Well known categories follow our hardcoded sorting order
    auto lhs_sort_order = GetCategorySortOrder(source_left);
    auto rhs_sort_order = GetCategorySortOrder(source_right);

    if (lhs_sort_order != -1 && rhs_sort_order != -1) {
      // Ensure that the category sorting is stable by negating
      // the sort ordering when needed
      if (sortOrder() == Qt::DescendingOrder) {
        std::swap(lhs_sort_order, rhs_sort_order);
      }

      return lhs_sort_order < rhs_sort_order;
    }

    // Sort everything else alphabetically
    auto lhs_display_role = source_left.data().toString();
    auto rhs_display_role = source_right.data().toString();

    if (sortOrder() == Qt::DescendingOrder) {
      std::swap(lhs_display_role, rhs_display_role);
    }

    return lhs_display_role < rhs_display_role;
  }

  switch (sort_role) {
    case Qt::DisplayRole:
    case EntityInformationModel::StringLocationRole:
    case EntityInformationModel::StringFileNameLocationRole:
      return source_left.data(sort_role).toString() <
            source_right.data(sort_role).toString();

    case IModel::TokenRangeDisplayRole: {
      auto left_token_range = qvariant_cast<TokenRange>(source_left.data(sort_role));
      auto right_token_range = qvariant_cast<TokenRange>(source_right.data(sort_role));

      return TokensToString(left_token_range) <
            TokensToString(right_token_range);
    }

    default:
      return source_left.row() < source_right.row();
  }
}

int SortFilterProxyModel::GetCategorySortOrder(const QModelIndex &index) const {
  static const auto kCategorySortingOrderMap{GetCategorySortingOrderMap()};

  auto display_role = index.data(Qt::DisplayRole).toString();
  if (display_role.isEmpty()) {
    return -1;
  }

  return kCategorySortingOrderMap.value(display_role, -1);
}

// void SortFilterProxyModel::OnBeginInsertRows(
//     const QModelIndex &parent, int begin_row, int end_row) {

//   emit beginInsertRows(mapFromSource(parent), begin_row, end_row);
// }

// void SortFilterProxyModel::OnDataChanged(const QModelIndex &top_left,
//                                          const QModelIndex &bottom_right,
//                                          const QList<int> &roles) {
//   emit dataChanged(mapFromSource(top_left), mapFromSource(bottom_right), roles);
// }

}  // namespace mx::gui
