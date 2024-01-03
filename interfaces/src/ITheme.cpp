/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/GUI/Interfaces/ITheme.h>

namespace mx::gui {
namespace {

static float GetColorContrast(const QColor &color) {
  return (0.2126f * color.redF() + 0.7152f * color.greenF() +
          0.0722f * color.blueF()) /
         1000.0f;
}

}  // namespace

ITheme::~ITheme(void) {}

std::optional<QColor> ITheme::EntityBackgroundColor(
    const VariantEntity &) const {
  return std::nullopt;
}

//! Helper to compute a high-contrast foreground color given a background
//! color.
QColor ITheme::ContrastingColor(const QColor &background_color) {
  QColor black_foreground{Qt::black};
  float black_foreground_contrast{GetColorContrast(black_foreground)};

  QColor white_foreground{Qt::white};
  float white_foreground_contrast{GetColorContrast(white_foreground)};

  float background_contrast{GetColorContrast(background_color)};

  QColor foreground_color;
  if (std::abs(black_foreground_contrast - background_contrast) >
      std::abs(white_foreground_contrast - background_contrast)) {

    foreground_color = black_foreground;

  } else {
    foreground_color = white_foreground;
  }

  return foreground_color;
}

}  // namespace mx::gui
