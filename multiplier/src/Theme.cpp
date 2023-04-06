// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include "Theme.h"

namespace mx::gui {

QPalette GetLightPalette() {
  QPalette palette;

  palette.setColor(QPalette::WindowText, QColor::fromRgb(0xff2e3436));
  palette.setColor(QPalette::Button, QColor::fromRgb(0xfff6f5f4));
  palette.setColor(QPalette::Light, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Midlight, QColor::fromRgb(0xfffbfafa));
  palette.setColor(QPalette::Dark, QColor::fromRgb(0xffdfdcd8));
  palette.setColor(QPalette::Mid, QColor::fromRgb(0xffebe8e6));
  palette.setColor(QPalette::Text, QColor::fromRgb(0xff2e3436));
  palette.setColor(QPalette::BrightText, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::ButtonText, QColor::fromRgb(0xff2e3436));
  palette.setColor(QPalette::Base, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Window, QColor::fromRgb(0xfff6f5f4));
  palette.setColor(QPalette::Shadow, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::Highlight, QColor::fromRgb(0xff3584e4));
  palette.setColor(QPalette::HighlightedText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Link, QColor::fromRgb(0xff1b6acb));
  palette.setColor(QPalette::LinkVisited, QColor::fromRgb(0xff15539e));
  palette.setColor(QPalette::AlternateBase, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::NoRole, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::ToolTipBase, QColor::fromRgb(0xff353535));
  palette.setColor(QPalette::ToolTipText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::PlaceholderText, QColor::fromRgb(0xff2e3436));

  return palette;
}

QPalette GetDarkPalette() {
  QPalette palette;

  palette.setColor(QPalette::WindowText, QColor::fromRgb(0xffeeeeec));
  palette.setColor(QPalette::Button, QColor::fromRgb(0xff373737));
  palette.setColor(QPalette::Light, QColor::fromRgb(0xff515151));
  palette.setColor(QPalette::Midlight, QColor::fromRgb(0xff444444));
  palette.setColor(QPalette::Dark, QColor::fromRgb(0xff1e1e1e));
  palette.setColor(QPalette::Mid, QColor::fromRgb(0xff2a2a2a));
  palette.setColor(QPalette::Text, QColor::fromRgb(0xffeeeeec));
  palette.setColor(QPalette::BrightText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::ButtonText, QColor::fromRgb(0xffeeeeec));
  palette.setColor(QPalette::Base, QColor::fromRgb(0xff2d2d2d));
  palette.setColor(QPalette::Window, QColor::fromRgb(0xff353535));
  palette.setColor(QPalette::Shadow, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::Highlight, QColor::fromRgb(0xff15539e));
  palette.setColor(QPalette::HighlightedText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::Link, QColor::fromRgb(0xff3584e4));
  palette.setColor(QPalette::LinkVisited, QColor::fromRgb(0xff1b6acb));
  palette.setColor(QPalette::AlternateBase, QColor::fromRgb(0xff2d2d2d));
  palette.setColor(QPalette::NoRole, QColor::fromRgb(0xff000000));
  palette.setColor(QPalette::ToolTipBase, QColor::fromRgb(0xff262626));
  palette.setColor(QPalette::ToolTipText, QColor::fromRgb(0xffffffff));
  palette.setColor(QPalette::PlaceholderText, QColor::fromRgb(0xffeeeeec));

  return palette;
}

}  // namespace mx::gui
