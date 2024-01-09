/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <memory>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/GUI/Interfaces/ITheme.h>
#include <multiplier/Frontend/TokenTree.h>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE
namespace mx::gui {

class ThemeManager;

class CodeModel Q_DECL_FINAL : public QObject {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CodeModel(void);

  CodeModel(QObject *parent = nullptr);

  QTextDocument *Set(TokenTree tokens, const ITheme *theme);
  QTextDocument *Reset(void);

  void ChangeTheme(const ITheme *theme);
};

}  // namespace mx::gui
