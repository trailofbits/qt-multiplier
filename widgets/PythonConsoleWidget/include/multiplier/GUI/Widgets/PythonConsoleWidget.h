// Copyright (c) 2022-present, Trail of Bits, Inc.
// All rights reserved.
//
// This source code is licensed in accordance with the terms specified in
// the LICENSE file found in the root directory of this source tree.

#pragma once

#include <multiplier/GUI/Managers/ThemeManager.h>
#include <multiplier/Index.h>

#include <QEvent>
#include <QWidget>

#include <memory>

typedef struct _object PyObject;

namespace mx::gui {

class PythonConsoleWidget final : public QWidget {
  Q_OBJECT

  struct PrivateData;
  const std::unique_ptr<PrivateData> d;

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

  PythonConsoleWidget(const ThemeManager &theme_manager, const Index &index,
                      QWidget *parent=nullptr);

  void SetHere(VariantEntity entity);
 
 public slots:
  //! Emitted when the theme has changed. Sends out the new theme.
  void OnThemeChanged(ITheme::Ptr);
};

}  // namespace mx::gui
