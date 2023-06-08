/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/Token.h>

#include <QWidget>

namespace mx::gui {

//! A label widget that displays a token
class ITokenLabel : public QWidget {
  Q_OBJECT

 public:
  //! Factory function
  static ITokenLabel *Create(Token token, QWidget *parent = nullptr);

  //! Constructor
  ITokenLabel(QWidget *parent) : QWidget(parent) {}

  //! Destructor
  virtual ~ITokenLabel() override = default;

  //! Disabled the copy constructor
  ITokenLabel(const ITokenLabel &) = delete;

  //! Disabled copy assignment
  ITokenLabel &operator=(const ITokenLabel &) = delete;
};

}  // namespace mx::gui
