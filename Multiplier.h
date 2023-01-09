// Copyright (c) 2021-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#include <QApplication>
#include <QSplashScreen>
#include <QWidget>

namespace mx {
namespace gui {

class Multiplier final : public QApplication {
 private:
  QSplashScreen splash_screen;

  using QApplication::exec;

 public:
  explicit Multiplier(int &argc, char *argv[]);

  int Run(QWidget *widget);
};

}  // namespace gui
}  // namespace mx
