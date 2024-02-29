// Copyright (c) 2024-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "ColorGenerator.h"

#include <random>
#include <cmath>

namespace mx::gui {

constexpr float kGoldenRatioConjugate{0.618033988749895f};

struct ColorGenerator::PrivateData final {
  float hue{};
  float saturation{};
  float value{};
};

ColorGenerator::ColorGenerator(const unsigned int &seed,
                               const QColor &background_color,
                               const float &saturation) :
                                 d(new PrivateData()) {
  d->value = background_color.toHsv().valueF();
  if (d->value > 0.5f) {
    d->value /= 2.0f;
  } else {
    d->value = 1.0f - (d->value / 2.0f);
  }

  d->saturation = saturation;

  std::mt19937 mt(seed);
  std::uniform_real_distribution<float> float_dist(0.0f, 1.0f);
  d->hue = float_dist(mt);
}

ColorGenerator::~ColorGenerator() {}

QColor ColorGenerator::Next() {
  d->hue = std::fmod(d->hue + kGoldenRatioConjugate, 1.0f);
  return QColor::fromHsvF(d->hue, d->saturation, d->value);
}

}  // namespace mx::gui
