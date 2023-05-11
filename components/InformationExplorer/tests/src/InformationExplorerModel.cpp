/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "InformationExplorerModel.h"

#include <doctest/doctest.h>

#include <QVariant>
#include <QStringList>

#include <unordered_map>
#include <unordered_set>

namespace mx::gui {

namespace {

using PropertyMap = std::unordered_map<int, QVariant>;

const int kTestStringRole{Qt::UserRole + 1};
const QString kTestStringData("Test string");

const int kPropertyPathRole{Qt::UserRole + 2};

QString PathToString(const QStringList &path) {
  QString string_path;

  for (const auto &path_component : path) {
    if (!string_path.isEmpty()) {
      string_path.append(".");
    }

    string_path.append(path_component);
  }

  return string_path;
}

PropertyMap GeneratePropertyMap(const QStringList &path) {
  static const PropertyMap kBaseTestValueMap = {
      {kTestStringRole, QVariant(kTestStringData)},
  };

  auto property_map{kBaseTestValueMap};
  property_map.insert({kPropertyPathRole, PathToString(path)});

  return property_map;
}

}  // namespace

TEST_CASE("InformationExplorerModel") {
  // The context always starts with no nodes
  InformationExplorerModel::Context context;
  REQUIRE(context.node_map.empty());

  // If we reset it (also done automatically by CreateProperty) then
  // we will obtain the hidden root item
  InformationExplorerModel::ResetContext(context);
  REQUIRE(context.node_map.size() == 1);

  // Creating the following properties should create 13 nodes:
  // * root => 1
  // * parents: parent0, parent1, parent2 => 3
  // * children: 3x nodes named child{0,1,2} => 9
  std::size_t expected_property_map_size{};

  for (const auto &parent_name : {"parent0", "parent1", "parent2"}) {
    for (const auto &child_name : {"child0", "child1", "child2"}) {
      QStringList path{parent_name, child_name};

      auto property_map = GeneratePropertyMap(path);
      InformationExplorerModel::CreateProperty(context, path,
                                               std::move(property_map));

      if (expected_property_map_size == 0) {
        expected_property_map_size = property_map.size();
      }
    }
  }

  REQUIRE(expected_property_map_size != 0);
  REQUIRE(context.node_map.size() == 13);

  // Make sure that the nodes we have generated all have a unique id
  std::unordered_set<InformationExplorerModel::Context::NodeID> node_id_list;

  for (const auto &node_map_p : context.node_map) {
    const auto &id = node_map_p.first;
    const auto &node = node_map_p.second;

    node_id_list.insert(id);
    node_id_list.insert(node.parent_node_id);

    for (const auto &child_id : node.child_id_list) {
      node_id_list.insert(child_id);
    }
  }

  REQUIRE(node_id_list.size() == context.node_map.size());

  // The root should only have 3 child items: parent0, parent1, parent2
  // Only the leaf nodes will have values
  const auto &root_node = context.node_map[0];
  REQUIRE(root_node.display_role.toStdString() == "ROOT");
  REQUIRE(root_node.child_id_list.size() == 3);

  std::unordered_set<InformationExplorerModel::Context::NodeID>
      parent_node_id_list;

  for (std::size_t i{0}; i < root_node.child_id_list.size(); ++i) {
    const auto &child_id = root_node.child_id_list[i];
    const auto &child_node = context.node_map[child_id];

    parent_node_id_list.insert(child_id);

    auto expected_name = QString("parent%1").arg(i);
    REQUIRE(expected_name.toStdString() ==
            child_node.display_role.toStdString());

    REQUIRE(child_node.parent_node_id == 0);
    REQUIRE(child_node.child_id_list.size() == 3);
    REQUIRE(child_node.value_map.empty());
  }

  // Each parent will have 3 child items
  for (const auto &parent_node_id : parent_node_id_list) {
    const auto &parent_node = context.node_map[parent_node_id];
    REQUIRE(parent_node.child_id_list.size() == 3);

    for (std::size_t i{}; i < parent_node.child_id_list.size(); ++i) {
      const auto &child_id = parent_node.child_id_list[i];
      const auto &child_node = context.node_map[child_id];

      REQUIRE(child_node.child_id_list.empty());
      REQUIRE(child_node.parent_node_id == parent_node_id);

      auto expected_name = QString("child%1").arg(i);
      REQUIRE(child_node.display_role.toStdString() ==
              expected_name.toStdString());

      REQUIRE(child_node.value_map.size() == expected_property_map_size);
      REQUIRE(
          child_node.value_map.at(kTestStringRole).toString().toStdString() ==
          kTestStringData.toStdString());

      auto expected_path =
          PathToString({parent_node.display_role, child_node.display_role});

      REQUIRE(
          child_node.value_map.at(kPropertyPathRole).toString().toStdString() ==
          expected_path.toStdString());
    }
  }
}

}  // namespace mx::gui
