/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QFont>
#include <QIcon>
#include <QObject>
#include <QPixmap>

#include <memory>
#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

class MediaManagerImpl;

class MediaManager Q_DECL_FINAL : public QObject {
  Q_OBJECT

  const std::shared_ptr<MediaManagerImpl> d;

  MediaManager(void) = delete;

 public:
  virtual ~MediaManager(void);

  explicit MediaManager(ThemeManager &theme_manager, QObject *parent = nullptr);

  //! Load a font given its ID, e.g. `com.trailofbits.font.SourceCodePro`.
  QFont Font(const QString &id) const;

  //! Get an icon by its ID, e.g. `com.trailofbits.icon.Logo`.
  QIcon Icon(const QString &id,
             ITheme::IconStyle style = ITheme::IconStyle::NONE) const;

  //! Get a colorized icon by its ID, e.g. `com.trailofbits.icon.Back`.
  QPixmap Pixmap(const QString &id,
                 ITheme::IconStyle style = ITheme::IconStyle::NONE) const;
 
 signals:
  //! Emitted when the theme has been changed.
  void IconsChanged(const MediaManager &manager);

 private slots:
  void OnThemeChanged(const ThemeManager &theme_manager);
};

}  // namespace mx::gui
