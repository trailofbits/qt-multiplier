/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QString>
#include <QtPlugin>

#include <multiplier/Index.h>

namespace mx::gui {

class IInfoGenerator;

using IInfoGeneratorPtr = std::shared_ptr<IInfoGenerator>;

//! Data generator for an entity tree. The data generator can be arbitrarily
//! slow at generating its data.
class IInfoGenerator : public std::enable_shared_from_this<IInfoGenerator> {
 public:
  virtual ~IInfoGenerator(void);

  struct Item {
    QString category;
    VariantEntity entity;
    VariantEntity referenced_entity;
    TokenRange tokens;
    bool is_location;
    QString location;

    Item(void) = default;

    inline Item(Item &&that) noexcept
        : category(that.category),
          entity(std::move(that.entity)),
          referenced_entity(std::move(that.referenced_entity)),
          tokens(std::move(that.tokens)),
          location(std::move(that.location)) {
      that.entity = NotAnEntity{};
      that.referenced_entity = NotAnEntity{};
      that.tokens = TokenRange();
      that.is_location = false;
      that.location = {};
    }

    inline Item &operator=(Item &&that_) noexcept {
      Item that(std::move(that_));
      entity = std::move(that.entity);
      referenced_entity = std::move(that.referenced_entity);
      tokens = std::move(that.tokens);
      is_location = std::move(that.is_location);
      location = std::move(that.location);
      return *this;
    }

    Item(const Item &that) noexcept = default;
    Item &operator=(const Item &that_) noexcept = default;
  };

  // Generate the information items for this category.
  //
  // NOTE(pag): These are `shared_ptr`s so that implementations have the
  //            flexibility of having tree items extend the lifetime of
  //            tree generator (`self`) itself via aliasing `std::shared_ptr`.
  //
  // NOTE(pag): This is allowed to block.
  virtual gap::generator<IInfoGenerator::Item> Items(
      IInfoGeneratorPtr self, FileLocationCache file_location_cache) = 0;
};

}  // namespace mx::gui

Q_DECLARE_INTERFACE(mx::gui::IInfoGenerator,
                    "com.trailofbits.interface.IInfoGenerator")
