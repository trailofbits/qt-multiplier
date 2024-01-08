/*
  Copyright (c) 2024-present, Trail of Bits, Inc.
  All rights reserved.

  This source code is licensed in accordance with the terms specified in
  the LICENSE file found in the root directory of this source tree.
*/

#pragma once

#include <memory>
#include <multiplier/GUI/Interfaces/IModel.h>
#include <multiplier/Frontend/TokenTree.h>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE
namespace mx::gui {

class CodeModel Q_DECL_FINAL : public QObject {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

 public:
  virtual ~CodeModel(void);

  CodeModel(QObject *parent = nullptr);

  QTextDocument *Import(TokenTree tokens);
};

}  // namespace mx::gui
