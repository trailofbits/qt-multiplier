// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QColor>

namespace mx::gui {

class ColorGenerator final {
public:
  ColorGenerator(const unsigned int &seed,
                 const QColor &background_color,
                 const float &saturation);

  ~ColorGenerator();

  QColor Next();

private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;
};

}  // namespace mx::gui
