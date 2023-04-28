/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#include <QIcon>
#include <QPixmap>

namespace mx::gui {

//! Icon style
enum class IconStyle {
  None,
  Highlighted,
  Disabled,
};

//! This function assumes that all icons are white
QIcon GetIcon(const QString &path, const IconStyle &style = IconStyle::None);

//! This function assumes that all icons are white
QPixmap GetPixmap(const QString &path,
                  const IconStyle &style = IconStyle::None);

}  // namespace mx::gui
