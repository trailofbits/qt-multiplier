// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/Index.h>

#include <QEvent>
#include <QWidget>

#include <memory>
#include <multiplier/Types.h>

typedef struct _object PyObject;

namespace mx {
class Index;
}  // namespace mx
namespace mx::gui {

class CodeViewTheme;

class PythonConsoleWidget final : public QWidget {
  Q_OBJECT

  struct PrivateData;
  std::unique_ptr<PrivateData> d;

  void InitializeModel(void);
  void InitializeWidgets(void);
  void ResetFontColor(void);

 private slots:
  void OnLineEntered(const QString &s);
  void OnPromptEnter(void);
  void OnEvaluationDone(void);
  void OnStdOut(const QString &str);
  void OnStdErr(const QString &str);

 protected:
  bool eventFilter(QObject *source, QEvent *event) final;

 public:
  virtual ~PythonConsoleWidget(void);

  PythonConsoleWidget(const Index &index, QWidget *parent=nullptr);

  void SetHere(VariantEntity entity);
 
 public slots:
  //! Sets the specified theme, refreshing the view
  void SetTheme(const QPalette &palette,
                const CodeViewTheme &code_view_theme);
};

}  // namespace mx::gui
