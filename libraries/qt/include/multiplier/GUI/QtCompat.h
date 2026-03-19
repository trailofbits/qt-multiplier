// Qt version compatibility helpers for qt-multiplier.
// Bridges API differences between Qt 6.4 (Ubuntu 24.04) and Qt 6.7+.

#pragma once

#include <QtGlobal>
#include <QCheckBox>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  #define MXQT_CHECK_STATE_CHANGED checkStateChanged
  using MxQtCheckState = Qt::CheckState;
#else
  #define MXQT_CHECK_STATE_CHANGED stateChanged
  using MxQtCheckState = int;
#endif
