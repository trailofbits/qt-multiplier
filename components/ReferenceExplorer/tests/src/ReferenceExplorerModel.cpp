/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "ReferenceExplorerModel.h"
#include "Utils.h"

#include <doctest/doctest.h>

#include <iostream>

namespace mx::gui {

namespace {

bool ValidateDisplayRole(const QModelIndex &index,
                         const QString &expected_value) {

  if (!index.isValid()) {
    return false;
  }

  auto display_role_var = index.data();
  if (!display_role_var.isValid()) {
    return false;
  }

  const auto &display_role = display_role_var.toString();
  if (display_role != expected_value) {
    std::cerr << "ValidateDisplayRole: " << display_role.toStdString()
              << " != " << expected_value.toStdString() << "\n";

    return false;
  }

  return true;
}

}  // namespace

TEST_CASE("ReferenceExplorerModel") {
  mx::FileLocationCache file_location_cache;
  auto index = OpenTestDatabase("sample_database01");

  std::unique_ptr<IReferenceExplorerModel> model(
      IReferenceExplorerModel::Create(index, file_location_cache));

  REQUIRE(model->rowCount(QModelIndex()) == 0);

  // Import the free() function call
  const std::uint64_t kEntityId{9223372106782212123ULL};
  model->AppendEntityObject(kEntityId, QModelIndex());

  // Refer to the source code for the sample database `sample_database01`:
  //
  // clang-format off
  //
  // free (root)
  // |_recursiveFreeCaller (lv1)
  //   |_recursiveFreeCaller (lv2, row 1)
  //   |_nestedFreeCaller5 (lv2, row 0)
  //     |_nestedFreeCaller4 (lv3, row 0)
  //       |_nestedFreeCaller3 (lv4, row 0)
  //         |_nestedFreeCaller2 (lv5, row 0) <- the model will stop here
  //
  // clang-format on

  REQUIRE(model->rowCount(QModelIndex()) == 1);
  auto root_index = model->index(0, 0, QModelIndex());
  REQUIRE(ValidateDisplayRole(root_index, "free"));

  REQUIRE(model->rowCount(root_index) == 1);
  auto index_lv1 = model->index(0, 0, root_index);
  REQUIRE(ValidateDisplayRole(index_lv1, "recursiveFreeCaller"));

  REQUIRE(model->rowCount(index_lv1) == 2);

  auto index_lv2_item0 = model->index(0, 0, index_lv1);
  REQUIRE(ValidateDisplayRole(index_lv2_item0, "nestedFreeCaller5"));

  auto index_lv2_item1 = model->index(1, 0, index_lv1);
  REQUIRE(ValidateDisplayRole(index_lv2_item1, "recursiveFreeCaller"));

  auto index_lv5_item0 = model->index(0, 0, index_lv2_item0);
  index_lv5_item0 = model->index(0, 0, index_lv5_item0);
  index_lv5_item0 = model->index(0, 0, index_lv5_item0);
  REQUIRE(ValidateDisplayRole(index_lv5_item0, "nestedFreeCaller2"));

  // Attempt to expand the last time we acquired
  REQUIRE(model->rowCount(index_lv5_item0) == 0);
  model->ExpandEntity(index_lv5_item0);
  REQUIRE(model->rowCount(index_lv5_item0) == 1);

  // Refer to the source code for the sample database `sample_database01`:
  //
  // clang-format off
  //
  // free (root)
  // |_recursiveFreeCaller (lv1)
  //   |_recursiveFreeCaller (lv2, row 1)
  //   |_nestedFreeCaller5 (lv2, row 0)
  //     |_nestedFreeCaller4 (lv3, row 0)
  //       |_nestedFreeCaller3 (lv4, row 0)
  //         |_nestedFreeCaller2 (lv5, row 0)
  //           |_nestedFreeCaller1 (lv6, row 0) <- expansion starts here
  //             |_destroyNodeList (lv7, row 0)
  //             |_destroyNodeList (lv7, row 1)
  //
  // clang-format on

  auto index_lv6_item0 = model->index(0, 0, index_lv5_item0);
  REQUIRE(ValidateDisplayRole(index_lv6_item0, "nestedFreeCaller1"));

  auto index_lv7_item0 = model->index(0, 0, index_lv6_item0);
  REQUIRE(ValidateDisplayRole(index_lv7_item0, "destroyNodeList"));

  auto index_lv7_item1 = model->index(1, 0, index_lv6_item0);
  REQUIRE(ValidateDisplayRole(index_lv7_item1, "destroyNodeList"));
}

}  // namespace mx::gui
