/*
  Copyright (c) 2023-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/Managers/ThemeManager.h>

namespace mx::gui {

class ProxyTheme Q_DECL_FINAL : public ITheme {
  Q_OBJECT

 public:

  std::vector<IThemeProxyPtr> proxies;
  ITheme *current_theme;

  virtual ~ProxyTheme(void);

  ProxyTheme(ITheme *current_theme_, QObject *parent);

  void Add(IThemeProxyPtr proxy);

  void Apply(QApplication &application) Q_DECL_FINAL;

  const QPalette &Palette(void) const Q_DECL_FINAL;

  QString Name(void) const Q_DECL_FINAL;

  QString Id(void) const Q_DECL_FINAL;

  QFont Font(void) const Q_DECL_FINAL;

  QColor IconColor(IconStyle style) const Q_DECL_FINAL;

  QColor GutterForegroundColor(void) const Q_DECL_FINAL;

  QColor GutterBackgroundColor(void) const Q_DECL_FINAL;

  QColor DefaultForegroundColor(void) const Q_DECL_FINAL;

  QColor DefaultBackgroundColor(void) const Q_DECL_FINAL;

  QColor CurrentLineBackgroundColor(void) const Q_DECL_FINAL;

  QColor CurrentEntityBackgroundColor(const VariantEntity &) const Q_DECL_FINAL;

  ColorAndStyle TokenColorAndStyle(const Token &) const Q_DECL_FINAL;

  std::optional<QColor> EntityBackgroundColor(
      const VariantEntity &entity) const;

 signals:
  void UninstallProxy(void);

 public slots:
  void Remove(IThemeProxy *proxy);
};

}  // namespace mx::gui
