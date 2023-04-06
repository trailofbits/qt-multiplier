// Copyright (c) 2023-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <QProxyStyle>

namespace mx::gui {

class MultiplierStyle final : public QProxyStyle {
  Q_OBJECT

 public:
  using QProxyStyle::QProxyStyle;

  int styleHint(
      StyleHint hint, const QStyleOption *option,
      const QWidget *widget, QStyleHintReturn *return_data) const Q_DECL_FINAL;
};

}  // namespace mx::gui
