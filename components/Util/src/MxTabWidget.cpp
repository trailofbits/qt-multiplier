/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <multiplier/ui/MxTabWidget.h>

#include <QWheelEvent>
#include <QTabBar>
#include <QCursor>

namespace {

class MxTabBar final : public QTabBar {
  Q_OBJECT

 public:
  MxTabBar(QWidget *parent = nullptr) : QTabBar(parent) {}
  virtual ~MxTabBar() = default;

  MxTabBar(MxTabBar &) = delete;
  MxTabBar &operator=(const MxTabBar &) = delete;

 protected:
  void wheelEvent(QWheelEvent *event) override {
    if (event->device()->type() == QInputDevice::DeviceType::TouchPad) {
      static QPoint scroll_amount;

      if (event->isBeginEvent()) {
        scroll_amount = {};
        return;

      } else if (event->isEndEvent()) {
        return;
      }

      scroll_amount += event->pixelDelta();

      auto x_abs = std::abs(scroll_amount.x());
      if (x_abs < 120) {
        return;
      }

      auto x_pixel_delta = x_abs / 120;
      auto negative_direction = scroll_amount.x() < 0;

      auto x_remainder = scroll_amount.x() % 120;
      scroll_amount.setX(x_remainder * (negative_direction ? -1 : 1));

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
};

}  // namespace

#include "MxTabWidget.moc"

namespace mx::gui {

MxTabWidget::MxTabWidget(QWidget *parent) : QTabWidget(parent) {
  setTabBar(new MxTabBar(this));
}

MxTabWidget::~MxTabWidget() {}

}  // namespace mx::gui
