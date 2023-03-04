/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "NodeImporter.h"
#include "Utils.h"

#include <multiplier/Index.h>

#include <doctest/doctest.h>

namespace mx::gui {

namespace {

NodeImporter::IndexData GetIndexData() {
  NodeImporter::IndexData index_data;
  index_data.index = OpenTestDatabase("sample_database01");

  for (auto [path, id] : index_data.index.file_paths()) {
    index_data.file_path_map.emplace(
        id.Pack(), QString::fromStdString(path.generic_string()));
  }

  return index_data;
}

}  // namespace

TEST_CASE("NodeImporter") {
  auto index_data = GetIndexData();
  REQUIRE(index_data.file_path_map.size() > 0);

  NodeTree node_tree;
  node_tree.node_map = {{0, NodeTree::Node{}}};

  // Used to get the references to the free() function
  const std::uint64_t kEntityId{9223372106782212123ULL};
  const std::uint64_t kReferencedEntityId{9223372106782212123ULL};

  // If you look at the source code (ci/data/sample_database01/src), the first level
  // must be `free` -> `recursiveFreeCaller`. Given that we also have a root node, the map
  // size should be 3
  NodeImporter::ImportEntity(node_tree, index_data, 0, kEntityId,
                             kReferencedEntityId, 1);

  REQUIRE(node_tree.node_map.size() == 3);

  REQUIRE(node_tree.node_map[1].opt_name.has_value());
  REQUIRE(node_tree.node_map[2].opt_name.has_value());

  REQUIRE(node_tree.node_map[1].opt_name.value() == "free");
  REQUIRE(node_tree.node_map[2].opt_name.value() == "recursiveFreeCaller");

  // If we further expand node 2 (i.e. `recursiveFreeCaller`) by one additional level
  // we should get two nodes:
  //
  // 1. `nestedFreeCaller5`
  // 2. `recursiveFreeCaller` (because it's recursive)
  NodeImporter::ExpandNode(node_tree, index_data, 2, 1);
  REQUIRE(node_tree.node_map.size() == 5);

  REQUIRE(node_tree.node_map[3].opt_name.has_value());
  REQUIRE(node_tree.node_map[3].opt_name.value() == "nestedFreeCaller5");

  REQUIRE(node_tree.node_map[4].opt_name.has_value());
  REQUIRE(node_tree.node_map[4].opt_name.value() == "recursiveFreeCaller");

  // If we attempt to expand again something we have already expanded, nothing
  // should happen
  NodeImporter::ExpandNode(node_tree, index_data, 2, 1);
  REQUIRE(node_tree.node_map.size() == 5);

  // Import everything with no limits
  node_tree = {};
  node_tree.node_map = {{0, NodeTree::Node{}}};

  NodeImporter::ImportEntity(node_tree, index_data, 0, kEntityId,
                             kReferencedEntityId, std::nullopt);

  REQUIRE(node_tree.node_map.size() == 12);
}

}  // namespace mx::gui
