/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/ITokenLabel.h>
#include <multiplier/ui/IThemeManager.h>

namespace mx::gui {

//! Implements the ITokenLabel
class TokenLabel final : public ITokenLabel {
  Q_OBJECT

 public:
  //! Destructor
  virtual ~TokenLabel() override;

 protected:
  virtual void paintEvent(QPaintEvent *event) override;

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  TokenLabel(TokenRange tokens, QWidget *parent);

 private slots:
  //! Called by the theme manager
  void OnThemeChange(const QPalette &palette,
                     const CodeViewTheme &code_view_theme);

  friend class ITokenLabel;
};

}  // namespace mx::gui
