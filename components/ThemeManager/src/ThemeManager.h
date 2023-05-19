/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/ui/IThemeManager.h>

#include <memory>

namespace mx::gui {

//! The main implementation for the IThemeManager interface
class ThemeManager final : public IThemeManager {
  Q_OBJECT

 public:
  //! \copybrief IThemeManager::SetTheme
  virtual void SetTheme(const bool &dark) override;

  //! \copybrief IThemeManager::GetPalette
  virtual const QPalette &GetPalette() const override;

  //! \copybrief IThemeManager::GetCodeViewTheme
  virtual const CodeViewTheme &GetCodeViewTheme() const override;

  //! \copybrief IThemeManager::isDarkTheme
  virtual bool isDarkTheme() const override;

  //! Destructor
  virtual ~ThemeManager();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ThemeManager(QApplication &application);

  friend class IThemeManager;
};

}  // namespace mx::gui
