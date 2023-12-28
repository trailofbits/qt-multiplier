/*
  Copyright (c) 2022-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <multiplier/GUI/ThemeManager.h>

#include <memory>

namespace mx::gui {

//! The main implementation for the ThemeManager interface
class ThemeManager final : public ThemeManager {
  Q_OBJECT

 public:
  //! \copybrief ThemeManager::SetTheme
  virtual void SetTheme(const bool &dark) override;

  //! \copybrief ThemeManager::GetPalette
  virtual const QPalette &GetPalette() const override;

  //! \copybrief ThemeManager::GetCodeViewTheme
  virtual const CodeViewTheme &GetCodeViewTheme() const override;

  //! \copybrief ThemeManager::SendGlobalUpdate
  virtual void SendGlobalUpdate() const override;

  //! \copybrief ThemeManager::isDarkTheme
  virtual bool isDarkTheme() const override;

  //! Destructor
  virtual ~ThemeManager();

 private:
  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  //! Constructor
  ThemeManager(QApplication &application);

  friend class ThemeManager;
};

}  // namespace mx::gui
