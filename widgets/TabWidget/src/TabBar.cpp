/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include "TabBar.h"

#include <QCursor>
#include <QWheelEvent>

namespace mx::gui {
namespace {

static QPoint gScrollAmount = {};

}  // namespace

TabBar::~TabBar(void) {}

void TabBar::wheelEvent(QWheelEvent *event) {
  if (event->device()->type() == QInputDevice::DeviceType::TouchPad) {

    if (event->isBeginEvent()) {
      gScrollAmount = {};
      return;

    } else if (event->isEndEvent()) {
      return;
    }

    gScrollAmount += event->pixelDelta();

    auto x_abs = std::abs(gScrollAmount.x());
    if (x_abs < 120) {
      return;
    }

    auto x_pixel_delta = x_abs / 120;
    auto negative_direction = gScrollAmount.x() < 0;

    auto x_remainder = gScrollAmount.x() % 120;
    gScrollAmount.setX(x_remainder * (negative_direction ? -1 : 1));

    auto global_pos = QCursor::pos();
    auto local_pos = mapFromGlobal(global_pos);

    QPoint angle_delta{0, 120 * (negative_direction ? -1 : 1)};
    QPoint pixel_delta{0,
                       x_pixel_delta * 120 * (negative_direction ? -1 : 1)};

    QWheelEvent alt_event(local_pos, global_pos, pixel_delta, angle_delta,
                          event->buttons(), event->modifiers(),
                          Qt::NoScrollPhase, event->inverted());

    QTabBar::wheelEvent(&alt_event);

  } else {
    QTabBar::wheelEvent(event);
  }
}

}  // namespace mx::gui
