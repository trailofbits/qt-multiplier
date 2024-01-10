/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <QWidget>

#include <memory>

namespace mx {
class TokenTree;
}  // namespace mx
namespace mx::gui {

class ConfigManager;
class MediaManager;
class ThemeManager;

// TODO: Have a class that one can connect to with signals, where the signals
//       trigger things like macro expansion
//
//       A code widget owns an instance of this class, and will git out a
//       pointer to it for those that want to selectively expand macros.
//
// TODO: Code plugin owns things like macro expansions, the global code preview
//       miniwindow (and an event like `com.trailofbits.action.PreviewEntity`)
//       (which can also pop out pinned previews, just like info browser), and
//       asks the main layout to add main widgets (files), which the window
//       manager manages as tabs.

class CodeWidget Q_DECL_FINAL : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CodeWidget(void);

  CodeWidget(const ConfigManager &config_manager,
             const TokenTree &token_tree,
             QWidget *parent = nullptr);

  
  
 private slots:
  void OnIndexChanged(const ConfigManager &);
  void OnThemeChanged(const ThemeManager &);
  void OnIconsChanged(const MediaManager &);
};

}  // namespace mx::gui
