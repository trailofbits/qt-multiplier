/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

// Include the headers we don't want to risk to break with our defines.
// The include guards will prevent them from being included again
// when building the .cpp files from the library.
//
// If this fails to build, just copy & paste all the main includes
// of phantomstyle.cpp here
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyleoption.h>

// Import QStringRef from the Qt5 compat library
#include <QStringRef>

// Ensure we have the right version defined
#define QT_VERSION QT_VERSION_CHECK(6, 4, 2)

//
// Patch the deprecated method names
//

#define background window

#define tabWidth reservedShortcutWidth

#define TextBypassShaping TextDontClip

#define SH_ScrollBar_StopMouseOverSlider SH_Slider_StopMouseOverSlider

//
// Fix QStyleOptionProgressBar
//

// We can't fix this with a define!
//
// As we are not using progress bars that are not horizontal
// we can just hardcode this value
class PatchedQStyleOptionProgressBar final : public QStyleOptionProgressBar {
 public:
  PatchedQStyleOptionProgressBar() = default;
  virtual ~PatchedQStyleOptionProgressBar() = default;

  static const int orientation{Qt::Horizontal};
};

// Make sure the rest of the library uses our hardcoded
// value
#define QStyleOptionProgressBar PatchedQStyleOptionProgressBar
